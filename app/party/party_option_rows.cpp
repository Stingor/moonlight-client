// Les lignes des deux pages du groupe : celles de la fenêtre Groupe / Amis, puis
// celles de la grille.

#include <cstddef>
#include <vector>

#include "ro/core/i18n.h"
#include "ro/ui/status_bar.h"

#include "party/party_options.h"

namespace moonclient {
namespace {

using ro::core::Tr;
using ro::ui::OptionKind;
using ro::ui::OptionRowView;

// Les bornes des curseurs de la fenêtre. Celles de la grille sont dans
// `ro/ui/party_grid.h`, que le dessin applique aussi.
constexpr float kWindowFewestStatusPixels = 8.0F;
constexpr float kWindowMostStatusPixels = 32.0F;
constexpr float kFewestStatusIcons = 1.0F;
constexpr float kMostStatusIcons = 24.0F;
constexpr float kFewestStatusRows = 1.0F;
constexpr float kMostStatusRows = 4.0F;
constexpr float kSmallestIconPixels = 16.0F;
constexpr float kLargestIconPixels = 56.0F;
constexpr float kWidestRowSpacingPixels = 12.0F;
constexpr float kNarrowestBarPixels = 20.0F;
constexpr float kWidestBarPixels = 260.0F;
constexpr float kThinnestBarPixels = 3.0F;
constexpr float kThickestBarPixels = 20.0F;
constexpr float kLargestBarTextPixels = 20.0F;

constexpr const char* kPixels = "%.0f px";
constexpr const char* kCount = "%.0f";
constexpr const char* kPercent = "%.0f %%";

[[nodiscard]] OptionRowView Toggle(int id, const char* label, const char* help, bool on) {
  return OptionRowView{
      .id = id, .label = label, .help = help, .kind = OptionKind::kToggle, .on = on};
}

[[nodiscard]] OptionRowView Slider(int id, const char* label, const char* help, int value,
                                   float lowest, float highest, const char* format) {
  return OptionRowView{.id = id,
                       .label = label,
                       .help = help,
                       .kind = OptionKind::kSlider,
                       .value = static_cast<float>(value),
                       .lowest = lowest,
                       .highest = highest,
                       .format = format};
}

[[nodiscard]] OptionRowView Choice(int id, const char* label, const char* help,
                                   std::span<const char* const> choices, int chosen) {
  return OptionRowView{.id = id,
                       .label = label,
                       .help = help,
                       .kind = OptionKind::kChoice,
                       .chosen = chosen,
                       .choices = choices};
}

[[nodiscard]] OptionRowView Color(int id, const char* label, const char* help, ImU32 color) {
  return OptionRowView{
      .id = id, .label = label, .help = help, .kind = OptionKind::kColor, .color = color};
}

// CE QUE LE GRISAGE D'UNE CASE NE PEUT PAS SAVOIR, commun aux deux surfaces.
[[nodiscard]] const char* SweepWarning() {
  return Tr(
      "La durée d'origine n'est portée par AUCUN paquet : elle est exacte quand on a vu "
      "l'état commencer, et repart de « plein » quand on le découvre en route.");
}

[[nodiscard]] int Window(PartyFriendRow row) { return PartyFriendRowId(row); }
[[nodiscard]] int Grid(PartyGridRow row) { return PartyGridRowId(row); }

// Les trois lignes du texte du temps restant, les mêmes sur les deux pages.
struct CountdownRowIds {
  int automatic = 0;
  int pixels = 0;
  int background = 0;
};

void AppendCountdownRows(std::vector<OptionRowView>& rows, bool shown, bool automatic, int pixels,
                         ImU32 background, CountdownRowIds ids) {
  rows.push_back(Toggle(ids.automatic, Tr("Taille automatique"),
                        Tr("Le texte fait la moitié de l'icône : il suit la taille réglée "
                           "au-dessus, sans jamais déborder sur la rangée du dessous."),
                        automatic));
  rows.back().indented = true;
  rows.back().disabled = !shown;
  rows.push_back(Slider(ids.pixels, Tr("Taille du texte"), nullptr, pixels,
                        ro::ui::kSmallestStatusCountdownPixels,
                        ro::ui::kLargestStatusCountdownPixels, kPixels));
  rows.back().indented = true;
  rows.back().disabled = !shown || automatic;
  rows.push_back(Color(ids.background, Tr("Fond du texte"),
                       Tr("Transparence comprise. Sous l'icône, le temps restant se pose "
                          "sur le décor : un fond discret le rend lisible partout."),
                       background));
  rows.back().indented = true;
  rows.back().disabled = !shown;
}

}  // namespace

void PartyOptions::Compose() {
  head_names_ = {Tr("Aucune"), Tr("Groupe"), Tr("Amis"), Tr("Les deux")};
  map_names_ = {Tr("Nom complet"), Tr("Nom court"), Tr("Masquée")};
  gauge_names_ = {Tr("Rien"), Tr("Chiffres"), Tr("Pourcentage"), Tr("Chiffres et pourcentage")};
  sweep_names_ = {Tr("Aucun"), Tr("Balayage horaire"), Tr("Voile descendant")};
  ComposeWindow();
  ComposeGrid();
}

void PartyOptions::ComposeWindow() {
  std::vector<OptionRowView>& rows = window_rows_;
  rows.clear();
  rows.push_back(Toggle(Window(PartyFriendRow::kLockSize), Tr("Verrouiller la taille"),
                        Tr("Empêche le redimensionnement, PAS le déplacement : la fenêtre se "
                           "déplace toujours par sa barre de titre.\n\nUne fois la largeur "
                           "réglée, viser le bord au lieu du titre la dérègle d'un pixel et "
                           "recompose les lignes sous la souris."),
                        window_.lock_size));
  rows.back().heading = Tr("Fenêtre");

  rows.push_back(Toggle(Window(PartyFriendRow::kJobIcon), Tr("Icône de classe"),
                        Tr("L'art du client, à gauche du nom. Éteinte, la ligne se resserre — "
                           "utile sur une fenêtre étroite."),
                        window_.show_job_icon));
  rows.back().heading = Tr("Contenu d'une ligne");
  rows.push_back(Choice(Window(PartyFriendRow::kHeadMode), Tr("Tête du personnage"),
                        Tr("La tête du personnage à la place de l'icône de classe, comme la "
                           "fenêtre des membres de guilde.\n\nElle vient de son sprite et suit un "
                           "changement de coiffure aussitôt : seul un personnage à l'écran en a "
                           "une, et la ligne reste sinon sans vignette."),
                        head_names_, window_.head_mode));
  rows.push_back(Toggle(Window(PartyFriendRow::kLevel), Tr("Niveau devant le nom"), nullptr,
                        window_.show_level));
  rows.push_back(Choice(Window(PartyFriendRow::kMapMode), Tr("Carte"),
                        Tr("Le client écrit « Gonryun, the Hermit Land (Kunlun) » là où "
                           "« Gonryun » suffit à se repérer — et le nom complet pousse le reste "
                           "de la ligne hors d'une fenêtre étroite.\n\nMasquée, la ligne ne porte "
                           "plus que le nom. L'infobulle au survol continue de donner la carte "
                           "entière."),
                        map_names_, window_.map_mode));
  rows.push_back(Toggle(Window(PartyFriendRow::kTooltip), Tr("Infobulle au survol"),
                        Tr("Au survol d'une ligne : la classe, la carte complète, la position et "
                           "les PV/SP chiffrés — ce qui ne tient pas dans la ligne."),
                        window_.show_tooltip));
  rows.push_back(Toggle(
      Window(PartyFriendRow::kClickTargets), Tr("Clic gauche : cibler le membre"),
      Tr("Comme une tuile du HUD en grille : la ligne devient une cible cliquable, et un liseré "
         "blanc marque la cible courante.\n\nSans effet sur un membre hors ligne ou hors de "
         "portée — ses PV et sa position viennent de son sprite, et il n'y en a pas.\n\nDemande "
         "le mode Ciblage, qui a son propre panneau : c'est lui qui décide qu'une cible existe."),
      window_.click_targets));
  rows.push_back(Toggle(Window(PartyFriendRow::kBuffs), Tr("Buffs et debuffs"),
                        Tr("Les icônes d'état du membre, à gauche de sa pastille."),
                        window_.show_buffs));
  rows.back().help_warning =
      Tr("Une ligne SANS icône ne veut pas dire « aucun buff ». Le serveur ne diffuse ces états "
         "qu'aux joueurs qui VOIENT le personnage, et seulement au moment où ils COMMENCENT : un "
         "joueur déjà béni quand il entre à l'écran arrive sans rien.\n\nCe qui s'affiche ici "
         "est donc ce qui est TOMBÉ sous vos yeux, pas l'état complet du personnage.");
  if (window_.show_buffs) {
    rows.push_back(Toggle(Window(PartyFriendRow::kBuffPreview), Tr("Aperçu (faux statuts)"),
                          Tr("Remplit l'affichage de faux états, aux durées étagées, le temps de "
                             "le régler — sans attendre d'en avoir de vrais.\n\nNe se garde pas "
                             "d'une session à l'autre."),
                          window_preview_));
    rows.push_back(Slider(Window(PartyFriendRow::kBuffPixels), Tr("Taille des icônes"), nullptr,
                          window_.buff_px, kWindowFewestStatusPixels, kWindowMostStatusPixels,
                          kPixels));
    rows.push_back(Slider(Window(PartyFriendRow::kBuffMost), Tr("Icônes au plus"), nullptr,
                          window_.buff_max, kFewestStatusIcons, kMostStatusIcons, kCount));
    rows.push_back(Slider(Window(PartyFriendRow::kBuffRows), Tr("Lignes d'icônes"),
                          Tr("Une rangée unique s'allonge jusqu'à manger la place du nom ; en deux "
                             "lignes, le même nombre d'états tient sur moitié moins de "
                             "largeur.\n\nLe compte maximum se répartit entre les lignes — six "
                             "icônes sur deux lignes font trois par ligne."),
                          window_.buff_rows, kFewestStatusRows, kMostStatusRows, kCount));
    rows.push_back(Slider(Window(PartyFriendRow::kBuffOpacity), Tr("Opacité des icônes"), nullptr,
                          window_.buff_opacity_percent,
                          static_cast<float>(ro::ui::kFaintestStatusOpacityPercent),
                          static_cast<float>(ro::ui::kFullStatusOpacityPercent), kPercent));
    rows.push_back(Color(
        Window(PartyFriendRow::kBuffIconBackground), Tr("Fond des icônes"),
        Tr("Posé derrière chaque icône, transparence comprise. Les icônes d'état sont en partie "
           "translucides : noir et opaque, elles restent lisibles sur tout "
           "décor ; transparent, le décor passe à travers."),
        window_.buff_icon_background));
    rows.push_back(Toggle(Window(PartyFriendRow::kBuffTime), Tr("Temps restant sous l'icône"),
                          nullptr, window_.buff_time));
    AppendCountdownRows(rows, window_.buff_time, window_.buff_time_auto, window_.buff_time_px,
                        window_.buff_time_background,
                        CountdownRowIds{.automatic = Window(PartyFriendRow::kBuffTimeAuto),
                                        .pixels = Window(PartyFriendRow::kBuffTimePixels),
                                        .background = Window(PartyFriendRow::kBuffTimeBackground)});
    rows.push_back(Choice(Window(PartyFriendRow::kBuffSweep), Tr("Grisage de la case"),
                          Tr("La case s'assombrit à mesure que l'état s'écoule."), sweep_names_,
                          window_.buff_sweep));
    rows.back().help_warning = SweepWarning();
    rows.push_back(Color(Window(PartyFriendRow::kBuffSweepColor), Tr("Couleur du voile"), nullptr,
                         window_.buff_sweep_color));
    rows.back().indented = true;
    rows.back().disabled = ro::ui::StatusSweepOf(window_.buff_sweep) == ro::ui::StatusSweep::kNone;
  }

  rows.push_back(Slider(Window(PartyFriendRow::kIconPixels), Tr("Taille de l'icône"), nullptr,
                        window_.icon_px, kSmallestIconPixels, kLargestIconPixels, kPixels));
  rows.back().heading = Tr("Densité");
  rows.push_back(Slider(Window(PartyFriendRow::kRowSpacing), Tr("Espace entre les lignes"),
                        Tr("À zéro, les lignes se touchent : c'est le réglage qui gagne le plus de "
                           "hauteur sur un groupe nombreux."),
                        window_.row_spacing, 0.0F, kWidestRowSpacingPixels, kPixels));

  rows.push_back(
      Toggle(Window(PartyFriendRow::kHpBar), Tr("Barre de vie"), nullptr, window_.show_hp_bar));
  rows.back().heading = Tr("Jauges");
  rows.push_back(Choice(Window(PartyFriendRow::kHpText), Tr("Texte des PV"), nullptr, gauge_names_,
                        window_.hp_text_mode));
  rows.push_back(
      Toggle(Window(PartyFriendRow::kSpBar), Tr("Barre de SP"), nullptr, window_.show_sp));
  rows.push_back(Choice(Window(PartyFriendRow::kSpText), Tr("Texte du SP"),
                        Tr("Le SP d'un autre joueur ne circule dans AUCUN paquet du jeu : il est "
                           "demandé au serveur, membre par membre. Il n'apparaît donc que pour "
                           "ceux qui sont à portée de vue, et coûte un peu de réseau — d'où le "
                           "défaut éteint."),
                        gauge_names_, window_.sp_text_mode));
  rows.push_back(Slider(Window(PartyFriendRow::kBarWidth), Tr("Largeur des jauges"), nullptr,
                        window_.bar_w, kNarrowestBarPixels, kWidestBarPixels, kPixels));
  rows.push_back(Slider(Window(PartyFriendRow::kBarHeight), Tr("Hauteur d'une jauge"), nullptr,
                        window_.bar_h, kThinnestBarPixels, kThickestBarPixels, kPixels));
  rows.push_back(Toggle(Window(PartyFriendRow::kBarsStacked),
                        Tr("Jauges collées l'une sous l'autre"),
                        Tr("PV au-dessus, SP juste dessous, sans rien entre les deux.\n\nLe texte "
                           "passe alors DANS les jauges : à côté, il pousserait la seconde d'une "
                           "hauteur de ligne et elles ne seraient plus collées."),
                        window_.bars_stacked));
  rows.push_back(Toggle(Window(PartyFriendRow::kTextInBars), Tr("Texte dans les jauges"),
                        Tr("Centré sur la jauge, avec une ombre pour rester lisible sur le vert "
                           "comme sur le fond. La ligne ne s'allonge pas — mais il faut une jauge "
                           "assez haute."),
                        window_.text_in_bars));
  rows.push_back(Slider(Window(PartyFriendRow::kTextPixels), Tr("Taille du texte des jauges"),
                        Tr("À zéro, celle de l'interface."), window_.text_px, 0.0F,
                        kLargestBarTextPixels, kPixels));
}

void PartyOptions::ComposeGrid() {
  std::vector<OptionRowView>& rows = grid_rows_;
  rows.clear();
  rows.push_back(Toggle(Grid(PartyGridRow::kShown), Tr("Afficher la grille de groupe"),
                        Tr("Le groupe en grille de tuiles : la barre de vie EST le fond de la "
                           "tuile, et sa couleur dit l'état du membre."),
                        grid_.shown));
  const std::size_t governed_from = rows.size();
  rows.push_back(Toggle(Grid(PartyGridRow::kLocked), Tr("Verrouiller la position"),
                        Tr("Fige le cadre et laisse passer les clics vers le jeu.\n\nMaintenir "
                           "MAJ, CURSEUR SUR LA GRILLE, la déverrouille le temps d'un déplacement "
                           ": pas besoin de revenir décocher ici.\n\nAilleurs à l'écran, MAJ garde "
                           "son rôle habituel (attaque forcée) — la grille ne reprend la souris "
                           "que sous le curseur."),
                        grid_.locked));
  rows.push_back(Toggle(Grid(PartyGridRow::kFakeMembers), Tr("Mode réglage (faux membres)"),
                        Tr("Remplit la grille de faux membres, un par état qu'une tuile sait "
                           "montrer — vie haute, moyenne, basse, à terre, hors de portée, hors "
                           "ligne, et un chef —, le temps de régler tailles et couleurs sans "
                           "groupe.\n\nAucun geste ne part sur eux, et le mode ne se garde pas "
                           "d'une session à l'autre."),
                        grid_fake_members_));

  rows.push_back(Slider(Grid(PartyGridRow::kColumns), Tr("Colonnes"),
                        Tr("1 colonne donne une liste, comme le HUD d'origine ; 2 ou 3 donnent la "
                           "grille compacte des raid frames."),
                        grid_.columns, static_cast<float>(ro::ui::kPartyGridFewestColumns),
                        static_cast<float>(ro::ui::kPartyGridMostColumns), kCount));
  rows.back().heading = Tr("Disposition");
  rows.push_back(Slider(Grid(PartyGridRow::kTileWidth), Tr("Largeur des tuiles"), nullptr,
                        grid_.tile_width_px,
                        static_cast<float>(ro::ui::kPartyGridNarrowestTilePixels),
                        static_cast<float>(ro::ui::kPartyGridWidestTilePixels), kPixels));
  rows.push_back(Slider(Grid(PartyGridRow::kTileHeight), Tr("Hauteur des tuiles"), nullptr,
                        grid_.tile_height_px,
                        static_cast<float>(ro::ui::kPartyGridShortestTilePixels),
                        static_cast<float>(ro::ui::kPartyGridTallestTilePixels), kPixels));
  rows.push_back(Slider(Grid(PartyGridRow::kGap), Tr("Espacement"),
                        Tr("La taille du cadre se DÉDUIT de ces valeurs : c'est la tuile qui "
                           "commande.\n\nTirer le cadre par sa poignée n'agit donc pas sur sa "
                           "taille mais sur le nombre de COLONNES : l'élargir en ajoute, le "
                           "rétrécir en retire. La hauteur, elle, suit le nombre de membres."),
                        grid_.gap_px, 0.0F, static_cast<float>(ro::ui::kPartyGridWidestGapPixels),
                        kPixels));

  rows.push_back(Toggle(Grid(PartyGridRow::kJobIcon), Tr("Icône de classe"),
                        Tr("L'art du client. C'est ce qui rend une grille lisible d'un coup d'œil "
                           ": on reconnaît le soigneur à sa silhouette, pas à son nom."),
                        grid_.show_job_icon));
  rows.back().heading = Tr("Contenu");
  rows.push_back(
      Toggle(Grid(PartyGridRow::kSelf), Tr("M'inclure dans la grille"), nullptr, grid_.show_self));
  rows.push_back(Toggle(Grid(PartyGridRow::kOffline), Tr("Garder les membres hors ligne"), nullptr,
                        grid_.show_offline));
  rows.push_back(
      Toggle(Grid(PartyGridRow::kLevel), Tr("Afficher le niveau"), nullptr, grid_.show_level));
  rows.push_back(Choice(Grid(PartyGridRow::kHpText), Tr("Points de vie"), nullptr, gauge_names_,
                        static_cast<int>(ro::ui::PartyGridHpTextOf(grid_.hp_text))));
  rows.push_back(Slider(Grid(PartyGridRow::kTextPixels), Tr("Taille du texte"),
                        Tr("Indépendante de la police des fenêtres : une grille se lit en "
                           "périphérie de l'écran, pas de face."),
                        grid_.text_px, static_cast<float>(ro::ui::kPartyGridSmallestTextPixels),
                        static_cast<float>(ro::ui::kPartyGridLargestTextPixels), kPixels));
  rows.push_back(Toggle(Grid(PartyGridRow::kTooltip), Tr("Infobulle au survol"),
                        Tr("Le texte d'une tuile est DÉCOUPÉ à ses bords : sur une grille serrée, "
                           "il ne reste parfois que les premières lettres d'un nom. L'infobulle le "
                           "redonne en entier, avec la classe, la carte et les PV/SP chiffrés."),
                        grid_.show_tooltip));
  rows.push_back(Toggle(Grid(PartyGridRow::kSp), Tr("Barre de SP"),
                        Tr("Le SP d'un autre joueur ne circule dans AUCUN paquet du jeu : il est "
                           "demandé au serveur membre par membre. Il n'apparaît donc que pour "
                           "ceux qui sont à portée de vue."),
                        grid_.show_sp));
  rows.push_back(Slider(Grid(PartyGridRow::kSpBar), Tr("Hauteur de la barre de SP"), nullptr,
                        grid_.sp_bar_px, static_cast<float>(ro::ui::kPartyGridThinnestSpBarPixels),
                        static_cast<float>(ro::ui::kPartyGridThickestSpBarPixels), kPixels));
  rows.back().disabled = !grid_.show_sp;

  rows.push_back(Toggle(
      Grid(PartyGridRow::kCastOnTile), Tr("Lancer les sorts de soutien sur la tuile survolée"),
      Tr("Un skill de soutien (soin, buff) part sur le membre dont la tuile est sous "
         "le curseur : la touche arme, la tuile désigne. Le liseré blanc montre qui sera "
         "visé.\n\nLa grille ne prend PAS le clic : marcher et frapper restent possibles "
         "curseur dessus.\n\nLes sorts d'ATTAQUE ne sont jamais concernés — viser un allié "
         "relève du PVP, que le jeu réserve au clic manuel."),
      grid_.cast_on_tile));
  rows.back().heading = Tr("Interaction");
  rows.push_back(Toggle(
      Grid(PartyGridRow::kClickable), Tr("Cliquer les tuiles (cibler, menu du groupe)"),
      Tr("Clic gauche : cibler le membre, comme un clic sur son personnage — sans effet si le mode "
         "Ciblage est éteint.\nClic droit : le menu du personnage, celui de son sprite. Sur un "
         "membre qu'aucun sprite ne représente, un menu de repli propose ce qui voyage par nom : "
         "chuchoter, expulser."),
      grid_.clickable));
  rows.back().help_warning =
      Tr("La grille PREND alors la souris sur toute sa surface : impossible de marcher ou de "
         "frapper en cliquant dessous. C'est le prix des gestes sur les tuiles — décoché, la "
         "grille se contente d'afficher et laisse tout passer.");
  rows.push_back(Toggle(Grid(PartyGridRow::kStatuses), Tr("Buffs et debuffs"),
                        Tr("Les icônes d'état du membre, calées à droite de sa tuile. Le nom se "
                           "découpe sur ce qu'elles laissent."),
                        grid_.show_statuses));
  rows.back().help_warning =
      Tr("Une tuile SANS icône ne veut pas dire « aucun buff ». Le serveur ne diffuse ces états "
         "qu'aux joueurs qui VOIENT le personnage, et seulement au moment où ils COMMENCENT : un "
         "joueur déjà béni quand il entre à l'écran arrive sans rien.\n\nCe qui s'affiche ici "
         "est donc ce qui est TOMBÉ sous vos yeux, pas l'état complet du personnage.");
  if (grid_.show_statuses) {
    rows.push_back(Toggle(Grid(PartyGridRow::kStatusPreview), Tr("Aperçu (faux statuts)"),
                          Tr("Remplit l'affichage de faux états, aux durées étagées, le temps de "
                             "le régler — sans attendre d'en avoir de vrais.\n\nNe se garde pas "
                             "d'une session à l'autre."),
                          grid_preview_));
    rows.push_back(Slider(Grid(PartyGridRow::kStatusPixels), Tr("Taille des icônes"), nullptr,
                          grid_.status_px,
                          static_cast<float>(ro::ui::kPartyGridSmallestStatusPixels),
                          static_cast<float>(ro::ui::kPartyGridLargestStatusPixels), kPixels));
    rows.push_back(Slider(Grid(PartyGridRow::kStatusMost), Tr("Icônes au plus"), nullptr,
                          grid_.status_most, kFewestStatusIcons, kMostStatusIcons, kCount));
    rows.push_back(Slider(Grid(PartyGridRow::kStatusRows), Tr("Lignes d'icônes"),
                          Tr("Une rangée unique s'allonge jusqu'à manger la place du nom ; en deux "
                             "lignes, le même nombre d'états tient sur moitié moins de "
                             "largeur.\n\nLe compte maximum se répartit entre les lignes — six "
                             "icônes sur deux lignes font trois par ligne."),
                          grid_.status_rows, kFewestStatusRows, kMostStatusRows, kCount));
    rows.push_back(Slider(Grid(PartyGridRow::kStatusOpacity), Tr("Opacité des icônes"), nullptr,
                          grid_.status_opacity_percent,
                          static_cast<float>(ro::ui::kFaintestStatusOpacityPercent),
                          static_cast<float>(ro::ui::kFullStatusOpacityPercent), kPercent));
    rows.push_back(Color(
        Grid(PartyGridRow::kStatusIconBackground), Tr("Fond des icônes"),
        Tr("Posé derrière chaque icône, transparence comprise. Les icônes d'état sont en partie "
           "translucides : noir et opaque, elles restent lisibles sur tout "
           "décor ; transparent, le décor passe à travers."),
        grid_.status_icon_background));
    rows.push_back(Toggle(Grid(PartyGridRow::kStatusTime), Tr("Temps restant sous l'icône"),
                          nullptr, grid_.status_countdown));
    AppendCountdownRows(rows, grid_.status_countdown, grid_.status_countdown_auto,
                        grid_.status_countdown_px, grid_.status_countdown_background,
                        CountdownRowIds{.automatic = Grid(PartyGridRow::kStatusTimeAuto),
                                        .pixels = Grid(PartyGridRow::kStatusTimePixels),
                                        .background = Grid(PartyGridRow::kStatusTimeBackground)});
    rows.push_back(Choice(Grid(PartyGridRow::kStatusSweep), Tr("Grisage de la case"),
                          Tr("La case s'assombrit à mesure que l'état s'écoule."), sweep_names_,
                          grid_.status_sweep));
    rows.back().help_warning = SweepWarning();
    rows.push_back(Color(Grid(PartyGridRow::kStatusSweepColor), Tr("Couleur du voile"), nullptr,
                         grid_.status_sweep_color));
    rows.back().indented = true;
    rows.back().disabled = ro::ui::StatusSweepOf(grid_.status_sweep) == ro::ui::StatusSweep::kNone;
  }

  rows.push_back(Color(Grid(PartyGridRow::kFrameBackground), Tr("Fond du cadre"),
                       Tr("Le panneau qui porte les tuiles. Sans lui, une grille sombre sur une "
                          "carte sombre devient impossible à distinguer du décor."),
                       grid_.frame_background));
  rows.back().heading = Tr("Couleurs");
  rows.push_back(Color(Grid(PartyGridRow::kTileBackground), Tr("Fond d'une tuile"), nullptr,
                       grid_.tile_background));
  rows.push_back(Color(Grid(PartyGridRow::kHpHigh), Tr("Vie — haute"), nullptr, grid_.hp_high));
  rows.push_back(Color(Grid(PartyGridRow::kHpMid), Tr("Vie — moyenne"), nullptr, grid_.hp_mid));
  rows.push_back(Color(Grid(PartyGridRow::kHpLow), Tr("Vie — basse"), nullptr, grid_.hp_low));
  rows.push_back(Color(Grid(PartyGridRow::kSpColor), "SP", nullptr, grid_.sp));
  rows.push_back(Color(Grid(PartyGridRow::kText), Tr("Texte"), nullptr, grid_.text));
  rows.push_back(Color(Grid(PartyGridRow::kFarText), Tr("Texte — hors de portée"),
                       Tr("Le membre est EN JEU, simplement trop loin pour que le client "
                          "connaisse ses PV : il peut revenir à portée d'un instant à l'autre. À "
                          "ne pas confondre avec un membre déconnecté."),
                       grid_.far_text));
  rows.push_back(Color(Grid(PartyGridRow::kOfflineText), Tr("Texte — hors ligne"), nullptr,
                       grid_.offline_text));
  rows.push_back(
      Color(Grid(PartyGridRow::kSelfBorder), Tr("Liseré de ma tuile"), nullptr, grid_.self_border));
  rows.push_back(Slider(Grid(PartyGridRow::kMidPercent), Tr("Seuil « vie moyenne »"), nullptr,
                        grid_.hp_mid_percent,
                        static_cast<float>(ro::ui::kPartyGridLowestMidPercent),
                        static_cast<float>(ro::ui::kPartyGridHighestMidPercent), kPercent));
  rows.push_back(Slider(Grid(PartyGridRow::kLowPercent), Tr("Seuil « vie basse »"),
                        Tr("En dessous de ces pourcentages, la tuile change de couleur. C'est ce "
                           "qui permet de repérer un blessé sans lire un seul chiffre."),
                        grid_.hp_low_percent,
                        static_cast<float>(ro::ui::kPartyGridLowestLowPercent),
                        static_cast<float>(ro::ui::kPartyGridHighestLowPercent), kPercent));

  // La grille éteinte grise tout ce qu'elle commande.
  for (std::size_t rank = governed_from; rank < rows.size(); ++rank) {
    rows[rank].disabled = rows[rank].disabled || !grid_.shown;
  }
}

}  // namespace moonclient
