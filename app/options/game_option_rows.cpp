// LE CATALOGUE DES RANGÉES DE RÉGLAGES : ce que chaque panneau montre.
//
// ⚠ SÉPARÉ DE CE QUI LES APPLIQUE, et pas seulement pour la longueur : ce
// fichier-ci ne se lit jamais de bout en bout. On y cherche UNE rangée, on la
// copie sur sa voisine, on s'en va. Le fichier d'à côté, lui, se lit : il porte
// les règles qui disent ce qu'un réglage TOUCHE, et un `switch` dont un cas
// tombe dans le suivant s'y voit.
//
// 🔴 LES LIGNES SE REFONT À CHAQUE IMAGE, panneau ouvert. C'est ce qui
// fait qu'un changement de langue se voit à l'image suivante, sans rien de figé —
// et c'est pourquoi rien ici ne doit coûter cher.

#include <imgui.h>

#include "ro/core/i18n.h"

#include "game/system_fonts.h"
#include "options/game_options.h"
#include "options/option_row_kinds.h"

namespace moonclient {
namespace {

using ro::core::Tr;
using ro::ui::OptionKind;
using ro::ui::OptionRowView;

[[nodiscard]] OptionRowView Volume(GameOption which, const char* label, float ratio) {
  return OptionRowView{.id = static_cast<int>(which),
                       .label = label,
                       .kind = OptionKind::kSlider,
                       .value = ratio * kPercent,
                       .lowest = 0.0F,
                       .highest = kPercent,
                       .format = "%.0f %%"};
}

}  // namespace

void GameOptions::Refresh(bool a_panel_is_open, int shown_bar) {
  if (!a_panel_is_open) {
    // L'aperçu des dégâts ne vit que panneau ouvert : fermé, personne ne
    // doit retrouver son personnage battu par des coups factices.
    damage_.preview = false;
    return;
  }
  sound_rows_.clear();
  sound_rows_.push_back(Toggle(GameOption::kMusic, Tr("Musique"),
                               Tr("La musique de la carte où l'on se trouve."), "/bgm",
                               sound_options_.music));
  sound_rows_.push_back(
      Volume(GameOption::kMusicVolume, Tr("Volume de la musique"), sound_options_.music_volume));
  sound_rows_.push_back(Toggle(GameOption::kSound, Tr("Bruitages"),
                               Tr("Les pas, les coups, les sorts."), "/snd", sound_options_.sound));
  sound_rows_.push_back(
      Volume(GameOption::kSoundVolume, Tr("Volume des bruitages"), sound_options_.sound_volume));
  // LES TROIS FAMILLES, SOUS LE GÉNÉRAL qu'elles multiplient.
  sound_rows_.push_back(Volume(GameOption::kSpriteSoundVolume, Tr("Volume des sprites et monstres"),
                               sound_options_.sprite_sound_volume));
  sound_rows_.push_back(Volume(GameOption::kEffectSoundVolume, Tr("Volume des effets et skills"),
                               sound_options_.effect_sound_volume));
  sound_rows_.push_back(Volume(GameOption::kAmbienceVolume, Tr("Volume des sons d'ambiance"),
                               sound_options_.ambience_volume));
  sound_rows_.push_back(
      Toggle(GameOption::kSpatialSound, Tr("Son spatialisé"),
             Tr("Un bruit s'entend du côté d'où il vient. Le client d'origine ne le dit pas : "
                "il n'atténue qu'avec la distance, et celle-ci reste dans les deux cas."),
             "", sound_options_.spatial_sound));

  display_rows_.clear();
  language_names_.clear();
  int chosen = 0;
  for (const Language& language : KnownLanguages()) {
    if (language_ == language.code) {
      chosen = static_cast<int>(language_names_.size());
    }
    language_names_.push_back(language.name);
  }
  display_rows_.push_back(OptionRowView{.id = static_cast<int>(GameOption::kLanguage),
                                        .label = Tr("Langue"),
                                        .kind = OptionKind::kChoice,
                                        .chosen = chosen,
                                        .choices = language_names_});
  display_rows_.back().heading = Tr("Texte");
  // ⚠ LA POLICE EST SOUS LA LANGUE, et pas ailleurs : les deux décident de ce
  // qu'on LIT, et l'une explique l'autre — changer de langue ne sert à rien si
  // la police ne dessine pas ses signes.
  font_names_.clear();
  // ⚠ LE LITTÉRAL MARQUÉ, PAS SA TRADUCTION : la liste déroulante traduit
  // elle-même ce qu'on lui donne, à la LECTURE. Le traduire ici le ferait deux
  // fois — et une chaîne ainsi calculée n'est dans aucun catalogue.
  //
  // ⚠ LES SEULES FAMILLES PRÉSENTES : la place dans la liste n'est donc pas le
  // rang du réglage, et `FontFamilyAtMenuRank` traduit l'une en l'autre.
  for (const int family : InstalledFontFamilies()) {
    font_names_.push_back(SystemFontFamilies()[static_cast<std::size_t>(family)].label.data());
  }
  display_rows_.push_back(
      OptionRowView{.id = static_cast<int>(GameOption::kFontFamily),
                    .label = Tr("Police"),
                    .help = Tr("Malgun Gothic est la seule qui dessine tout : les "
                               "accents, la ponctuation et les chemins coréens de "
                               "l'installation. Les autres sont latines."),
                    .kind = OptionKind::kChoice,
                    .chosen = MenuRankOfFontFamily(font_family_),
                    .choices = font_names_});
  // 🔴 DES PALIERS ET NON UN CURSEUR. Poser l'échelle redimensionne TOUTES les
  // fenêtres, et la passe est à PERTE : un curseur invite à balayer la plage,
  // donc à empiler les troncatures, et à s'arrêter sur 1,07 — une valeur que
  // personne ne voulait. Voir `kInterfaceScalePercents`.
  //
  // ⚠ LES PALIERS SE COMPOSENT AVANT LA RANGÉE : le facteur courant en fait
  // partie quand il ne tombe sur aucun d'eux.
  ReadInterfaceScales();
  display_rows_.push_back(Choice(GameOption::kInterfaceScale, Tr("Échelle de l'interface"),
                                 Tr("Agrandit ou réduit toutes les fenêtres à la fois. Un "
                                    "changement les déplace ; le client demande alors s'il "
                                    "faut le garder."),
                                 interface_scale_choices_, InterfaceScaleRank()));
  display_rows_.back().command = "/echelle";
  display_rows_.back().heading = Tr("Fenêtres");

  // ⚠ DANS « INTERFACE » ET NON DANS LA CONVERSATION : elle vaut pour TOUTE
  // fenêtre qu'on peut traîner, et la chatbox n'en est qu'une parmi d'autres.
  display_rows_.push_back(Toggle(GameOption::kAlignGrid, Tr("Grille d'alignement"),
                                 Tr("Un quadrillage plein écran pour ranger les "
                                    "fenêtres. Il se peint sous elles."),
                                 nullptr, align_grid_.shown));
  display_rows_.push_back(
      Toggle(GameOption::kAlignGridMagnet, Tr("Aimanter à la grille"),
             Tr("Une fenêtre qu'on déplace se pose sur la ligne la plus proche. "
                "Indépendant de l'affichage : on peut aimanter sans voir la grille."),
             nullptr, align_grid_.magnetic));
  display_rows_.push_back(
      OptionRowView{.id = static_cast<int>(GameOption::kAlignGridCell),
                    .label = Tr("Pas de la grille"),
                    .kind = OptionKind::kSlider,
                    .value = static_cast<float>(align_grid_.cell_pixels),
                    .lowest = static_cast<float>(ro::ui::kSmallestGridCellPixels),
                    .highest = static_cast<float>(ro::ui::kLargestGridCellPixels),
                    .format = "%.0f px"});
  // ⚠ EN DERNIER DE SON GROUPE : son sélecteur s'ouvre vers le bas et recouvre
  // les rangées suivantes, que le panneau neutralise le temps qu'il est déroulé.
  // Ce qu'il couvre se réduit ainsi à un intertitre et à une case.
  display_rows_.push_back(OptionRowView{.id = static_cast<int>(GameOption::kAlignGridColor),
                                        .label = Tr("Couleur de la grille"),
                                        .help = Tr("Transparence comprise : un maillage discret se "
                                                   "voit sans couvrir ce qu'il sert à aligner."),
                                        .kind = OptionKind::kColor,
                                        .color = align_grid_.color});

  // ⚠ EN BAS, ET SOUS SON PROPRE INTERTITRE. Il reste dans « Interface » —
  // c'est le seul réglage qu'une page de dialogue porte, et cette section règle
  // précisément ce que chaque fenêtre fait —, mais posé en tête il séparait la
  // langue de la police et l'échelle de sa grille. Un réglage seul de sa sorte
  // se range au bout, pas au milieu de ceux qui vont par paires.
  display_rows_.push_back(
      Toggle(GameOption::kNpcMenuSearch, Tr("Recherche dans les menus de PNJ"),
             Tr("Ajoute un champ de recherche au-dessus des longs menus de PNJ, à partir "
                "de huit choix, pour filtrer les options. Décoche pour un menu épuré."),
             nullptr, npc_menu_search_));
  display_rows_.back().heading = Tr("Dialogue de PNJ");

  display_rows_.push_back(
      Toggle(GameOption::kUnlockNotice, Tr("Avis d'interface déverrouillée"),
             Tr("Tant qu'un élément d'interface est déverrouillé, un avis le nomme "
                "et offre de tout verrouiller. Décoche pour laisser un élément "
                "déverrouillé sans l'avoir sous les yeux."),
             nullptr, unlock_notice_));
  display_rows_.back().heading = Tr("Pose de l'interface");

  // ⚠ LE JEU LES SÉPARE, NOUS LES RASSEMBLONS, et c'est un écart assumé :
  // `gamesettings.lub` range « Display Effects » dans son onglet EFFECT et
  // « /mineffect » dans ETC, avec /emblem et /font. Ses onglets ne sont pas les
  // nôtres — mais le joueur qui cherche l'un cherche l'autre.
  display_rows_.push_back(Toggle(GameOption::kEffectsShown, Tr("Afficher les effets"),
                                 Tr("Les effets des skills et des items. Une liste du "
                                    "client en garde cent quatorze visibles quand même."),
                                 "/effect", effects_shown_));
  display_rows_.back().heading = Tr("Les effets");
  display_rows_.push_back(Toggle(GameOption::kMinimalEffects, Tr("Effets allégés"),
                                 Tr("Remplace les effets les plus chargés par une version "
                                    "plus sobre, quand le client en connaît une."),
                                 "/mineffect", minimal_effects_));

  // LA PARTIE PVP : la place des deux pièces du classement. Voir
  // `game_option_pvp.cpp` — une famille, un fichier, comme les jauges.
  ComposePvpHudRows();
  // LE BANDEAU DU NOM DE LA CARTE, sous son propre intertitre.
  pages_.map_sign.ComposeInto(display_rows_);
  display_rows_.back().heading = Tr("Arrivée sur une carte");
  pages_.window_title.ComposeInto(display_rows_);
  // LE RENONCEMENT AU RAPPORT DE BUG, en fin d'interface.
  pages_.bug_report.AppendRows(display_rows_);

  // ⚠ LEUR PROPRE SECTION, et non celle de la conversation : une bulle n'est
  // pas la chatbox. L'une se lit à son rythme, l'autre passe au-dessus d'une
  // tête et disparaît ; les ranger ensemble ferait chercher un réglage de
  // durée parmi des filtres de fil.
  bubble_rows_.clear();
  bubble_rows_.push_back(
      Toggle(GameOption::kBubbleShowOwn, Tr("Voir aussi les miennes"),
             Tr("Le client ne les montre pas - on vient de les taper. Mais l'écho "
                "du serveur dit ce qui est VRAIMENT parti, sigil compris."),
             nullptr, bubbles_.show_own));
  bubble_rows_.push_back(Toggle(GameOption::kBubbleFade,
                                Tr("Fondu en fin "
                                   "d'affichage"),
                                nullptr, nullptr, bubbles_.fade));
  bubble_rows_.push_back(Toggle(GameOption::kBubbleClientTiming, Tr("Garder la durée du client"),
                                Tr("Cinq secondes pour tout le monde : un pavé de trois lignes "
                                   "disparaît aussi vite qu'un « ok ». Décoché, la durée suit la "
                                   "longueur."),
                                nullptr, bubbles_.client_timing));
  bubble_rows_.push_back(OptionRowView{.id = static_cast<int>(GameOption::kBubbleBaseMs),
                                       .label = Tr("Durée de base"),
                                       .kind = OptionKind::kSlider,
                                       .value = bubbles_.base_ms,
                                       .lowest = ro::ui::kShortestBubbleMs,
                                       .highest = ro::ui::kLongestBubbleMs,
                                       .format = "%.0f ms"});
  bubble_rows_.push_back(OptionRowView{.id = static_cast<int>(GameOption::kBubblePerCharacterMs),
                                       .label = Tr("Rallonge par caractère"),
                                       .kind = OptionKind::kSlider,
                                       .value = bubbles_.per_character_ms,
                                       .lowest = 0.0F,
                                       .highest = ro::ui::kMostBubblePerCharacterMs,
                                       .format = "%.0f ms"});
  bubble_rows_.push_back(OptionRowView{.id = static_cast<int>(GameOption::kBubbleRise),
                                       .label = Tr("Décalage vertical"),
                                       .kind = OptionKind::kSlider,
                                       .value = bubbles_.rise_pixels,
                                       .lowest = 0.0F,
                                       .highest = ro::ui::kMostBubbleRisePixels,
                                       .format = "%.0f px"});
  bubble_rows_.push_back(
      OptionRowView{.id = static_cast<int>(GameOption::kBubbleWidth),
                    .label = Tr("Largeur maximale"),
                    .help = Tr("En fraction de l'écran. Une phrase plus longue se "
                               "replie sur plusieurs lignes ; à zéro elle reste "
                               "d'un seul tenant, quitte à sortir de l'écran."),
                    .kind = OptionKind::kSlider,
                    .value = bubbles_.width_fraction,
                    .lowest = 0.0F,
                    .highest = 1.0F,
                    .format = "%.2f"});
  bubble_rows_.push_back(OptionRowView{.id = static_cast<int>(GameOption::kBubbleOpacity),
                                       .label = Tr("Opacité"),
                                       .kind = OptionKind::kSlider,
                                       .value = bubbles_.opacity,
                                       .lowest = 0.0F,
                                       .highest = 1.0F,
                                       .format = "%.2f"});
  // L'APPARENCE DE LA BULLE, À LA FIN DE SA SECTION : le fond, ses coins et
  // ses marges, puis les deux couleurs.
  bubble_rows_.push_back(
      Toggle(GameOption::kBubbleBackground, Tr("Fond derrière le texte"),
             Tr("Le client ne peint qu'un liseré sur du transparent : sa bulle "
                "disparaît dès que le décor est clair. Son liseré est gardé, le fond "
                "s'ajoute dessous."),
             nullptr, bubbles_.background));
  bubble_rows_.back().heading = Tr("Apparence");
  bubble_rows_.push_back(OptionRowView{.id = static_cast<int>(GameOption::kBubbleCorner),
                                       .label = Tr("Arrondi des coins"),
                                       .kind = OptionKind::kSlider,
                                       .value = bubbles_.corner_radius,
                                       .lowest = 0.0F,
                                       .highest = ro::ui::kMostBubbleCornerRadius,
                                       .format = "%.0f px"});
  bubble_rows_.push_back(OptionRowView{.id = static_cast<int>(GameOption::kBubblePaddingX),
                                       .label = Tr("Marge horizontale"),
                                       .kind = OptionKind::kSlider,
                                       .value = bubbles_.padding_x,
                                       .lowest = 0.0F,
                                       .highest = ro::ui::kMostBubblePadding,
                                       .format = "%.0f px"});
  bubble_rows_.push_back(OptionRowView{.id = static_cast<int>(GameOption::kBubblePaddingY),
                                       .label = Tr("Marge verticale"),
                                       .kind = OptionKind::kSlider,
                                       .value = bubbles_.padding_y,
                                       .lowest = 0.0F,
                                       .highest = ro::ui::kMostBubblePadding,
                                       .format = "%.0f px"});
  // Les nuanciers EN DERNIER : leur sélecteur s'ouvre vers le bas.
  bubble_rows_.push_back(OptionRowView{.id = static_cast<int>(GameOption::kBubbleBackgroundColor),
                                       .label = Tr("Couleur du fond"),
                                       .kind = OptionKind::kColor,
                                       .color = bubbles_.background_color});
  bubble_rows_.push_back(
      OptionRowView{.id = static_cast<int>(GameOption::kBubbleBorderColor),
                    .label = Tr("Couleur du liseré"),
                    .help = Tr("Celle du client : un gris moyen, le seul trait qu'il "
                               "peigne autour d'une bulle."),
                    .kind = OptionKind::kColor,
                    .color = bubbles_.border_color});

  chat_rows_.clear();
  chat_rows_.push_back(
      OptionRowView{.id = static_cast<int>(GameOption::kChatFont),
                    .label = Tr("Taille du texte"),
                    .help = Tr("Celle du fil seul - l'échelle de l'interface, elle, "
                               "vaut pour tout le reste."),
                    .kind = OptionKind::kSlider,
                    .value = static_cast<float>(chat_look_.font_percent),
                    .lowest = static_cast<float>(ro::ui::kSmallestChatFontPercent),
                    .highest = static_cast<float>(ro::ui::kLargestChatFontPercent),
                    .format = "%.0f %%"});
  chat_rows_.push_back(
      OptionRowView{.id = static_cast<int>(GameOption::kChatPadding),
                    .label = Tr("Marges"),
                    .kind = OptionKind::kSlider,
                    .value = static_cast<float>(chat_look_.padding_pixels),
                    .lowest = 0.0F,
                    .highest = static_cast<float>(ro::ui::kLargestChatPaddingPixels),
                    .format = "%.0f px"});
  chat_rows_.push_back(
      OptionRowView{.id = static_cast<int>(GameOption::kChatLineGap),
                    .label = Tr("Interligne"),
                    .kind = OptionKind::kSlider,
                    .value = static_cast<float>(chat_look_.line_gap_pixels),
                    .lowest = 0.0F,
                    .highest = static_cast<float>(ro::ui::kLargestChatLineGapPixels),
                    .format = "%.0f px"});
  chat_rows_.push_back(OptionRowView{.id = static_cast<int>(GameOption::kChatKeptPerKind),
                                     .label = Tr("Lignes gardées par genre"),
                                     .help = Tr("Ce que la conversation garde en mémoire pendant "
                                                "qu'on joue. Par genre, pour qu'une carte bavarde "
                                                "n'efface pas un mot de guilde."),
                                     .kind = OptionKind::kSlider,
                                     .value = static_cast<float>(chat_kept_per_kind_),
                                     .lowest = static_cast<float>(ro::sim::kFewestChatLinesKept),
                                     .highest = static_cast<float>(ro::sim::kMostChatLinesKept),
                                     .format = "%.0f"});
  chat_rows_.push_back(
      Toggle(GameOption::kChatGrowInput,
             Tr("Agrandir le champ pour une longue "
                "phrase"),
             Tr("Un champ d'une ligne qui défile cache le DÉBUT de ce qu'on écrit, "
                "alors que la longueur est bornée et qu'un dépassement est refusé "
                "sans un mot."),
             nullptr, chat_look_.grow_input));
  chat_rows_.push_back(
      Toggle(GameOption::kChatBattleMode, Tr("Battle mode : replier la barre de saisie"),
             Tr("Le clavier revient au jeu, et la barre n'apparaît qu'à la demande. "
                "Entrée l'ouvre, Entrée sur une ligne vide ou Échap la referme, et "
                "taper écrit dedans. C'est le /bm du client."),
             nullptr, chat_look_.battle_mode));
  chat_rows_.push_back(Toggle(GameOption::kChatTimestamps, Tr("Horodatage"),
                              Tr("L'heure de chaque ligne, en tête."), nullptr,
                              chat_look_.timestamps));
  chat_rows_.push_back(
      Toggle(GameOption::kChatKeepHistory, Tr("Garder la conversation d'une partie à l'autre"),
             Tr("Prend effet au prochain démarrage. Le fichier contient les chuchotements "
                "EN CLAIR ; décocher l'efface."),
             nullptr, chat_look_.keep_history));
  chat_rows_.push_back(OptionRowView{.id = static_cast<int>(GameOption::kChatKeptLines),
                                     .label = Tr("Lignes gardées"),
                                     .kind = OptionKind::kSlider,
                                     .value = static_cast<float>(chat_look_.kept_lines),
                                     .lowest = static_cast<float>(kSmallestChatHistory),
                                     .highest = static_cast<float>(kLargestChatHistory),
                                     .format = "%.0f"});
  // ⚠ LA SEULE DES TROIS QUI PART COCHÉE, et le prix est dit dans son aide :
  // ce qu'on retape le plus sont les `@commandes`, et un service qu'il faut
  // d'abord aller cocher n'existe pas pour qui ignore qu'il existe.
  chat_rows_.push_back(Toggle(GameOption::kChatKeepTyped,
                              Tr("Garder les lignes tapées d'une partie à l'autre"),
                              // ⚠ « la flèche du haut » ET NON LE CARACTÈRE : U+2191 n'est pas
                              // dans l'atlas, et sortirait en « ? » à l'écran. Le contrôle des
                              // traductions le dit, et c'est lui qui l'a rattrapé ici.
                              Tr("Ce que la flèche du haut rappelle dans la barre de saisie — les "
                                 "@commandes comprises. Même fichier que la conversation, donc les "
                                 "chuchotements TAPÉS y sont EN CLAIR ; décocher l'efface."),
                              nullptr, chat_look_.keep_typed));
  chat_rows_.push_back(OptionRowView{.id = static_cast<int>(GameOption::kChatKeptTyped),
                                     .label = Tr("Lignes tapées gardées"),
                                     .kind = OptionKind::kSlider,
                                     .value = static_cast<float>(chat_look_.kept_typed),
                                     .lowest = static_cast<float>(ro::ui::kSmallestTypedHistory),
                                     .highest = static_cast<float>(ro::ui::kLargestTypedHistory),
                                     .format = "%.0f"});
  chat_rows_.push_back(
      Toggle(GameOption::kChatKeepLog, Tr("Tenir un journal sur le disque"),
             Tr("Un fichier texte par jour, dans le dossier Chat, écrit au fur et à "
                "mesure — il survit donc à un plantage. Il contient les chuchotements "
                "EN CLAIR ; décocher le laisse en place."),
             nullptr, chat_look_.keep_log));
  chat_rows_.push_back(
      Toggle(GameOption::kChatDiagnostic, Tr("Diagnostic : tout montrer, genre en tête"),
             Tr("Lève les filtres et le vidage, et écrit le genre de chaque ligne "
                "devant elle. Une ligne visible ici mais absente d'un onglet a été "
                "écartée par un filtre ; une ligne absente même ici n'est jamais "
                "arrivée."),
             nullptr, chat_look_.diagnostic));
  pages_.ComposeChatExtras(chat_rows_);
  // ⚠ LES COULEURS EN DERNIER DE LA SECTION : le nuancier s'ouvre vers le BAS,
  // et recouvrirait les rangées suivantes. Elles ont leur intertitre à elles.
  chat_rows_.push_back(OptionRowView{.id = static_cast<int>(GameOption::kChatBody),
                                     .label = Tr("Fond"),
                                     .help = Tr("Transparence comprise : c'est elle "
                                                "qu'on vient régler le plus souvent."),
                                     .kind = OptionKind::kColor,
                                     .color = chat_look_.body,
                                     .heading = Tr("Apparence")});
  chat_rows_.push_back(OptionRowView{.id = static_cast<int>(GameOption::kChatBorder),
                                     .label = Tr("Bordure"),
                                     .kind = OptionKind::kColor,
                                     .color = chat_look_.border});
  chat_rows_.push_back(OptionRowView{.id = static_cast<int>(GameOption::kChatTabs),
                                     .label = Tr("Onglets"),
                                     .help = Tr("L'onglet montré est le même gris éclairci, le "
                                                "survolé l'est à peine."),
                                     .kind = OptionKind::kColor,
                                     .color = chat_look_.tabs});
  chat_rows_.push_back(
      OptionRowView{.id = static_cast<int>(GameOption::kChatClientColors),
                    .label = Tr("Couleurs du client"),
                    .help = Tr("Rend aux trois couleurs celles du client d'origine. "
                               "Ne touche à aucun autre réglage."),
                    .kind = OptionKind::kAction});
  chat_rows_.push_back(
      OptionRowView{.id = static_cast<int>(GameOption::kChatForgetHistory),
                    .label = Tr("Vider l'historique"),
                    .help = Tr("Efface le fichier gardé d'une partie à l'autre — la "
                               "conversation ET les lignes tapées, qui y vivent "
                               "ensemble. Ce qui viendra ensuite sera gardé comme "
                               "d'habitude : ce sont les cases au-dessus qui en "
                               "décident."),
                    .kind = OptionKind::kAction,
                    .confirm = Tr("Effacer la conversation et les lignes tapées ? Sans retour.")});

  // L'AFFICHAGE, à part : voir `display_settings.cpp`.
  ComposeScreenRows();

  // LES TROIS FENÊTRES D'OBJETS, à part : le sac, la charrette et le coffre
  // portent une section chacune, et elles se répondent. Voir
  // `game_option_items.cpp`.
  ComposeItemWindowRows();
  ComposeItemObtainRows();

  // Les barres de raccourcis, à part : quarante-deux lignes composées en boucle.
  // Voir `game_option_bars.cpp`.
  ComposeShortcutRows(shown_bar);
  character_rows_.clear();
  character_rows_.push_back(
      Toggle(GameOption::kCharacterHoverDescription, Tr("Description au survol"),
             Tr("Le survol d'une pièce portée montre sa description entière plutôt que son "
                "seul nom. Elle REMPLACE l'infobulle de nom, elle ne s'y ajoute pas."),
             nullptr, character_hover_description_));

  // LA BARRE DÉTATS : une famille, un fichier. Voir
  // `game_option_statuses.cpp`.
  ComposeStatusBarRows();

  game_rows_.clear();
  game_rows_.push_back(
      Toggle(GameOption::kFixedCamera, Tr("Caméra collée au personnage"),
             Tr("Sinon elle le rattrape, et c'est ce qui donne à Ragnarok son flottement."),
             "/camera", fixed_camera_));
  game_rows_.push_back(Toggle(GameOption::kExpandedZoom, Tr("Dézoom étendu"),
                              Tr("Laisse reculer la caméra un cinquième plus loin dehors."),
                              "/zoom", expanded_zoom_));

  // LES DEUX MODIFICATEURS DE CIBLAGE. ⚠ LEUR LIBELLÉ DIT CE QUE LA TOUCHE
  // DEVIENT INUTILE À FAIRE, et non ce que la commande « désactive » : son nom
  // — « pas de MAJ » — se lit à l'envers de son effet.
  game_rows_.push_back(
      Toggle(GameOption::kNoShift, Tr("Viser sans tenir Maj"),
             Tr("Maj force la cible : frapper un joueur, ou soigner un monstre. Ce réglage "
                "le fait tenir tout seul."),
             "/noshift", no_shift_));
  game_rows_.push_back(
      Toggle(GameOption::kNoCtrl, Tr("Attaquer en continu sans tenir Ctrl"),
             Tr("Sinon un clic ne porte qu'un coup, et c'est Ctrl qui fait poursuivre la "
                "cible jusqu'à sa chute."),
             "/noctrl", no_ctrl_));
  game_rows_.push_back(Toggle(GameOption::kSkillFailShown, Tr("Messages d'échec de skill"),
                              Tr("Décoché, les échecs sans motif précis et les délais de "
                                 "réutilisation ne s'écrivent plus dans la discussion."),
                              "/skillfail", skill_fail_shown_));
  // LES ÉCHOPPES DES AUTRES : leur ouverture, et leurs enseignes.
  pages_.shops.ComposeInto(game_rows_);

  // LES DEUX RÉPLIQUES FORCÉES. ⚠ ELLES PARTENT AU SERVEUR, sous NOTRE nom :
  // c'est notre personnage qui parle quand un autre crie ou plaisante. Éteintes
  // par défaut, comme au client du joueur.
  game_rows_.push_back(
      Toggle(GameOption::kSpeakScream, Tr("Crier les répliques du Scream"),
             Tr("Quand une danseuse lance Scream, votre personnage envoie au chat une "
                "phrase tirée au sort. Décoché, il se tait."),
             nullptr, speak_scream_));
  game_rows_.back().heading = Tr("Les répliques forcées");
  game_rows_.push_back(
      Toggle(GameOption::kSpeakFrostJoke, Tr("Dire les plaisanteries du Frost Joke"),
             Tr("Quand un barde lance Frost Joke, votre personnage envoie au chat une "
                "plaisanterie tirée au sort. Décoché, il se tait."),
             nullptr, speak_frost_joke_));
  // L'APPEL D'URGENCE : un réglage que le serveur tient — voir `call_options.h`.
  pages_.call.ComposeInto(game_rows_);
  pages_.battleground.ComposeInto(game_rows_);

  game_rows_.push_back(
      Toggle(GameOption::kItemDropArc, Tr("Butin qui jaillit"),
             Tr("Le butin part de la créature qui vient de le lâcher, s'élève et retombe "
                "sur sa case.\n\n"
                "Décoché, il tombe du ciel à la verticale, comme chez le client "
                "d'origine — quinze unités plus haut, quatre dixièmes de seconde.\n\n"
                "Purement visuel : la case où l'item se ramasse ne change pas."),
             nullptr, item_drop_.arc));
  game_rows_.back().heading = Tr("Le butin qui tombe");
  const bool no_arc = !item_drop_.arc;
  game_rows_.push_back(Measure(GameOption::kItemDropArcHeight, Tr("Crête de l'arc"),
                               Tr("En unités du monde : une case en fait cinq. Vingt, le "
                                  "défaut, est le double d'un saut de personnage."),
                               item_drop_.height_units, ro::render::kItemArcLowestHeightUnits,
                               ro::render::kItemArcHighestHeightUnits, "%.0f"));
  game_rows_.back().indented = true;
  game_rows_.back().disabled = no_arc;
  game_rows_.push_back(
      Measure(GameOption::kItemDropArcKick, Tr("Écart du départ"),
              Tr("De combien l'item part à côté de sa case quand personne ne l'a lâché "
                 "à portée — la direction vient de son identifiant, si bien qu'une pluie "
                 "de butin s'ouvre en gerbe.\n\n"
                 "À zéro, il monte et retombe droit."),
              item_drop_.kick_units, 0.0F, ro::render::kItemArcHighestKickUnits, "%.0f"));
  game_rows_.back().indented = true;
  game_rows_.back().disabled = no_arc;
  game_rows_.push_back(
      Measure(GameOption::kItemDropArcSourceHeight, Tr("Hauteur de départ"),
              Tr("Où, sur le corps de celui qui l'a lâché, le butin naît : zéro aux "
                 "pieds, cent au sommet de la tête.\n\n"
                 "C'est une part de SA taille, et non une distance : un Poring et un "
                 "Baphomet n'ont pas la même, et une hauteur fixe passerait au-dessus "
                 "de l'un et sous les genoux de l'autre."),
              item_drop_.source_height_fraction * kPercent, 0.0F,
              ro::render::kItemArcHighestSourceFraction * kPercent, "%.0f %%"));
  game_rows_.back().indented = true;
  game_rows_.back().disabled = no_arc;
  game_rows_.push_back(
      Measure(GameOption::kItemDropArcDuration, Tr("Durée du vol"),
              Tr("En millisecondes, rebonds non compris. Le client d'origine met environ "
                 "quatre cent vingt millisecondes à faire tomber un item tout droit."),
              static_cast<float>(item_drop_.duration_ms),
              static_cast<float>(ro::render::kItemArcMinDurationMs),
              static_cast<float>(ro::render::kItemArcHighestDurationMs), "%.0f ms"));
  game_rows_.back().indented = true;
  game_rows_.back().disabled = no_arc;

  // ⚠ LE MÊME `shown_bar` QUE LES BARRES DE RACCOURCIS, et ce n'est pas un
  // partage fautif : la fenêtre ne montre qu'UNE section à la fois, donc son
  // onglet ouvert ne désigne jamais deux choses en même temps.
  BuildCaptureRows();
  ComposeDamageRows(shown_bar);
  BuildStatBarRows(shown_bar);
  // ⚠ LES NOMS ET LA ZONE CLIQUABLE SE COMPOSENT AVEC LES BARRES, dans la même
  // page : voir `BuildEntityBarRows`. La zone se compose AVANT, puisqu'elle y
  // est ajoutée. ⚠ LA LARGEUR DE LA FENÊTRE EST CELLE D'IMGUI, relevée à
  // l'image : les planchers que la page annonce sont ceux de cet écran-là.
  pages_.pick_precision.Compose(ImGui::GetIO().DisplaySize.x);
  BuildEntityBarRows(shown_bar);

  // ── LES NEUF SECTIONS, EN DEUX FAMILLES ───────────────────────────────────
  //
  // 🔴 TOUTES LES LIGNES SONT COMPOSÉES AVANT QU'UNE SEULE VUE NE SE PRENNE, et
  // c'est un plantage qui l'a dit. Une vue est un `span` sur le vecteur : prise
  // avant la composition, elle survit tant que le vecteur ne se réalloue pas —
  // c'est-à-dire jusqu'au premier onglet qui compose PLUS de lignes que le
  // précédent. La vue pointe alors sur un tampon libéré, et le premier
  // intertitre lu est du 0xDD.
  //
  // 🔴 L'ORDRE EST CELUI DE LA LECTURE, et les familles sont contiguës : le
  // titre d'une famille est porté par son PREMIER membre, donc une section
  // déplacée hors de son bloc emporterait son titre avec elle.
  //
  // CORE règle ce que le PROGRAMME fait — l'image, le son, le monde. UI règle ce
  // que CHAQUE FENÊTRE fait. C'est la seule coupure qu'un joueur puisse faire
  // avant d'avoir ouvert le panneau.
  // ⚠ LES DEUX FAMILLES SONT ÉCRITES ICI, EN TOUTES LETTRES : le relevé des
  // traductions ne lit qu'un appel porté sur un littéral, et passer par une
  // constante les rendrait invisibles aux catalogues.
  //
  // ⚠ ET ELLES RESTENT EN ANGLAIS, comme « Inventory » ou « Cart » : c'est le
  // vocabulaire dans lequel les joueurs de Ragnarok lisent leur interface.
  sections_[Rank(OptionSection::kDisplay)] =
      ro::ui::OptionSectionView{.title = Tr("Display"), .group = Tr("Core"), .rows = screen_rows_};
  // ⚠ COMPOSÉE AVANT la vue qui la désigne : elle n'en garde qu'un span.
  pages_.graphics.Compose();
  sections_[Rank(OptionSection::kGraphics)] =
      ro::ui::OptionSectionView{.title = Tr("Graphics"), .rows = pages_.graphics.rows()};
  // LES SONS DES MONSTRES ET DES PETS EN DERNIER : leur bloc repliable court
  // jusqu'à la fin de la section.
  pages_.sprite_sounds.ComposeInto(sound_rows_);
  sections_[Rank(OptionSection::kSound)] =
      ro::ui::OptionSectionView{.title = Tr("Sound"), .rows = sound_rows_};
  // ⚠ « GAMEPLAY » GARDE SA SECTION malgré son unique réglage : c'est là que
  // viendront les options du client d'origine — effets, brouillard, ombres —,
  // soixante-deux que `gamesettings.lub` liste déjà.
  // ⚠ APRÈS les lignes de jeu et AVANT la vue qui les désigne : elle n'en garde
  // qu'un span, et pousser ensuite le réallouerait sous elle.
  BuildSkillAreaRows();
  // Quick cast, sous la zone des sorts, dans le même « Gameplay ».
  pages_.quick_cast.ComposeInto(game_rows_);
  // GREYWORLD EN DERNIER : il ouvre un bloc repliable, qui court jusqu'à la fin
  // de la section.
  pages_.grey_world.ComposeInto(game_rows_);
  sections_[Rank(OptionSection::kGameplay)] =
      ro::ui::OptionSectionView{.title = Tr("Gameplay"), .rows = game_rows_};
  // ⚠ CONTRE « GAMEPLAY », ET DANS « CORE » : l'écran de veille ne règle aucune
  // fenêtre — il prend la caméra, éteint tout le reste et teinte l'image.
  // ⚠ COMPOSÉ AVANT la vue qui le désigne : elle n'en garde qu'un span, et
  // pousser ensuite le réallouerait sous elle.
  pages_.afk.Compose();
  sections_[Rank(OptionSection::kAfkScreen)] =
      ro::ui::OptionSectionView{.title = Tr("Écran de veille"), .rows = pages_.afk.rows()};
  // ⚠ DANS « CORE », CONTRE « GAMEPLAY » : les nombres sont posés SUR LE MONDE
  // par le renderer, pas par une fenêtre — et c'est la première chose qu'un
  // joueur qui trouve ses chiffres trop petits vient chercher. Quatre onglets
  // pour un seul objet : voir `game_option_damage.cpp`.
  sections_[Rank(OptionSection::kDamage)] =
      ro::ui::OptionSectionView{.title = Tr("Dégâts"),
                                .rows = damage_rows_,
                                .shared_rows = damage_shared_rows_,
                                .side_rows = pages_.damage_side_rows,
                                .tabs = damage_names_.tabs};
  // ⚠ DANS « CORE », CONTRE LES DÉGÂTS : ces barres sont posées SUR LE MONDE,
  // au-dessus des têtes, et non dans une fenêtre qu'on range. Six onglets, un
  // par espèce, parce que ce que le serveur dit de la vie d'autrui dépend
  // exactement de qui c'est : voir `game_option_entities.cpp`.
  sections_[Rank(OptionSection::kEntityBars)] =
      ro::ui::OptionSectionView{.title = Tr("Entités"),
                                .rows = entity_bar_rows_,
                                .shared_rows = entity_bar_shared_rows_,
                                .pinned_rows = pages_.entity_pinned_rows,
                                .tabs = entity_bar_tabs_};
  // ⚠ DANS « CORE » ET NON DANS L'INTERFACE : c'est le PROGRAMME qui relit son
  // image, et ce que la capture montre est tout ce que l'écran porte.
  sections_[Rank(OptionSection::kCapture)] =
      ro::ui::OptionSectionView{.title = Tr("Capture d'écran"), .rows = capture_rows_};
  sections_[Rank(OptionSection::kInterface)] =
      ro::ui::OptionSectionView{.title = Tr("Interface"),
                                .group = Tr("UI"),
                                .rows = display_rows_,
                                .body = pages_.profiles_body};
  sections_[Rank(OptionSection::kSkin)] = pages_.ComposeSkinSection();
  pages_.minimap_panel.Compose();
  sections_[Rank(OptionSection::kMinimap)] =
      ro::ui::OptionSectionView{.title = Tr("Minimap"), .rows = pages_.minimap_panel.rows()};
  pages_.menu_icons.Compose();
  sections_[Rank(OptionSection::kMenuIcons)] =
      ro::ui::OptionSectionView{.title = Tr("Icônes du menu"), .rows = pages_.menu_icons.rows()};
  sections_[Rank(OptionSection::kShortcutBars)] =
      ro::ui::OptionSectionView{.title = Tr("Barres de raccourcis"),
                                .rows = shortcut_rows_,
                                .shared_rows = shortcut_shared_rows_,
                                // ⚠ HUIT ONGLETS, UN PAR BARRE : dix-neuf
                                // réglages fois huit feraient cent cinquante
                                // lignes qui se ressemblent toutes.
                                .tabs = shortcut_tabs_};
  sections_[Rank(OptionSection::kStatBars)] =
      ro::ui::OptionSectionView{.title = Tr("Jauges"),
                                .rows = stat_bar_rows_,
                                .shared_rows = stat_bar_shared_rows_,
                                .tabs = stat_bar_tabs_};
  sections_[Rank(OptionSection::kStatuses)] =
      ro::ui::OptionSectionView{.title = Tr("États"), .rows = status_rows_};
  // ⚠ COMPOSÉ AVANT la vue qui le désigne : elle n'en garde qu'un span.
  pages_.damage_meter.Compose();
  sections_[Rank(OptionSection::kDamageMeter)] = ro::ui::OptionSectionView{
      .title = Tr("Compteur de dégâts"), .rows = pages_.damage_meter.rows()};
  // ⚠ COMPOSÉ AVANT la vue qui le désigne, comme l'écran de veille.
  pages_.targeting.Compose();
  sections_[Rank(OptionSection::kTarget)] =
      ro::ui::OptionSectionView{.title = Tr("Fenêtre de cible"), .rows = pages_.targeting.rows()};
  pages_.monster_sheet.Compose();
  sections_[Rank(OptionSection::kMonsterSheet)] = ro::ui::OptionSectionView{
      .title = Tr("Fiche de monstre"), .rows = pages_.monster_sheet.rows()};
  pages_.mvp_tracker.Compose();
  sections_[Rank(OptionSection::kMvpTracker)] = ro::ui::OptionSectionView{
      .title = Tr("Carnet de chasse MVP"), .rows = pages_.mvp_tracker.rows()};
  // LE MENU DU CLIC DROIT, contre la fenêtre de cible : le clic droit d'un cadre
  // de la fenêtre ouvre ce menu. ⚠ COMPOSÉ AVANT la vue, comme elle.
  pages_.context_menu.Compose();
  sections_[Rank(OptionSection::kContextMenu)] =
      ro::ui::OptionSectionView{.title = Tr("Menu contextuel"), .rows = pages_.context_menu.rows()};
  pages_.party.Compose();
  sections_[Rank(OptionSection::kPartyGrid)] =
      ro::ui::OptionSectionView{.title = Tr("Groupe (grille)"), .rows = pages_.party.grid_rows()};
  sections_[Rank(OptionSection::kPartyFriend)] =
      ro::ui::OptionSectionView{.title = Tr("Groupe / Amis"), .rows = pages_.party.window_rows()};
  sections_[Rank(OptionSection::kChat)] =
      ro::ui::OptionSectionView{.title = Tr("Conversation"), .rows = chat_rows_};
  sections_[Rank(OptionSection::kBubbles)] =
      ro::ui::OptionSectionView{.title = Tr("Bulles"), .rows = bubble_rows_};
  // ⚠ « INVENTORY », « CART » ET « STORAGE » RESTENT EN ANGLAIS : c'est le
  // vocabulaire de jeu, que les joueurs lisent ainsi depuis des années.
  sections_[Rank(OptionSection::kItemObtain)] =
      ro::ui::OptionSectionView{.title = Tr("Item obtenu"), .rows = item_obtain_rows_};
  sections_[Rank(OptionSection::kInventory)] =
      ro::ui::OptionSectionView{.title = Tr("Inventory"), .rows = inventory_rows_};
  sections_[Rank(OptionSection::kCart)] =
      ro::ui::OptionSectionView{.title = Tr("Cart"), .rows = cart_rows_};
  sections_[Rank(OptionSection::kStorage)] =
      ro::ui::OptionSectionView{.title = Tr("Storage"), .rows = storage_rows_};
  pages_.craft_atlas.Compose();
  sections_[Rank(OptionSection::kCraftAtlas)] =
      ro::ui::OptionSectionView{.title = Tr("Recettes"), .rows = pages_.craft_atlas.rows()};
  pages_.make_item.Compose();
  sections_[Rank(OptionSection::kMakeItem)] =
      ro::ui::OptionSectionView{.title = Tr("Fabrication"), .rows = pages_.make_item.rows()};
  pages_.weapon_refine.Compose();
  sections_[Rank(OptionSection::kWeaponRefine)] =
      ro::ui::OptionSectionView{.title = Tr("Refine"), .rows = pages_.weapon_refine.rows()};
  pages_.card_album.Compose();
  sections_[Rank(OptionSection::kCardAlbum)] =
      ro::ui::OptionSectionView{.title = Tr("Album de cartes"), .rows = pages_.card_album.rows()};
  pages_.descriptions.Compose();
  sections_[Rank(OptionSection::kDescriptions)] =
      ro::ui::OptionSectionView{.title = Tr("Descriptions"), .rows = pages_.descriptions.rows()};
  sections_[Rank(OptionSection::kCharacter)] =
      ro::ui::OptionSectionView{.title = Tr("Personnage"), .rows = character_rows_};
  pages_.pet.Compose();
  sections_[Rank(OptionSection::kPet)] =
      ro::ui::OptionSectionView{.title = Tr("Fiche de pet"), .rows = pages_.pet.rows()};
  pages_.quest_tracker.Compose();
  sections_[Rank(OptionSection::kQuestTracker)] = ro::ui::OptionSectionView{
      .title = Tr("Quêtes et suivi"), .rows = pages_.quest_tracker.rows()};
  // ⚠ AUCUNE RANGÉE : la page se dessine elle-même. Voir `options/commands_page.h`.
  sections_[Rank(OptionSection::kCommands)] = ro::ui::OptionSectionView{
      .title = Tr("Commands Settings"), .group = Tr("Server"), .body = pages_.commands_body};
  pages_.MarkRestorable(sections_);
}

}  // namespace moonclient
