// LES RANGÉES DES BARRES DE VIE ET DE MAGIE DES ENTITÉS, un onglet par espèce.
//
// ⚠ À PART DU CATALOGUE, pour la raison des jauges libres et des barres de
// raccourcis : ceci est une BOUCLE, pas une liste de littéraux qu'on copie six
// fois. Les six espèces portent les mêmes vingt-cinq réglages.
//
// 🔴 CE QUI RESTE AU-DESSUS DES ONGLETS N'EST PAS « CE QUI SE RESSEMBLE », C'EST
// « CE QUI NE PEUT PAS DIFFÉRER » : le maître et le mode réglage. Tout le reste diffère
// par espèce, et doit pouvoir différer — une barre fine et muette sur les
// moutons, une barre large et chiffrée sur un MVP.
//
// 🔴 ET CE QUE LE SERVEUR NE DIT PAS SE DIT DANS L'INFOBULLE. Un joueur qui
// allume « barre de magie » sur les monstres doit apprendre ICI qu'aucun paquet
// du jeu ne la porte, sinon il cherchera la panne dans son écran.

#include <algorithm>
#include <cstddef>
#include <span>
#include <vector>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/render/actor_pick.h"
#include "ro/ui/entity_bars.h"
#include "ro/ui/entity_sample.h"

#include "options/entity_page_tabs.h"
#include "options/game_options.h"
#include "options/option_row_kinds.h"

namespace moonclient {
namespace {

using ro::core::Tr;
using ro::ui::OptionKind;
using ro::ui::OptionRowView;

// Les formats des curseurs. ⚠ L'UNITÉ EST DANS LE FORMAT et non dans le
// libellé : « Largeur » suivi de « 60 » ne dit pas si ce sont des pixels ou des
// cases.
constexpr const char* kPixelFormat = "%.0f px";
constexpr const char* kPercentFormat = "%.0f %%";

// Les bornes d'un pour cent réglé au curseur. ⚠ EN CENTIÈMES DANS LE RÉGLAGE,
// EN POUR CENT SUR LE CURSEUR : une part de zéro à un se règle très mal au pixel
// près, et « 0,25 » se lit moins bien que « 25 % ».
constexpr float kPercentScale = 100.0F;

}  // namespace

// CE QUI VAUT POUR LES SIX ESPÈCES, puis les onglets.
void GameOptions::BuildEntityBarShared() {
  entity_bar_shared_rows_.clear();
  // 🔴 LE MODE RÉGLAGE EN TÊTE, ET POUR TOUT : c'est lui qui montre le cadre
  // d'essai, où barres, nom, cast et zone cliquable se règlent ensemble.
  entity_bar_shared_rows_.push_back(
      Toggle(GameOption::kEntityBarsTuning, Tr("Mode réglage"),
             Tr("Montre en haut de la page l'entité de l'onglet, telle que la carte la "
                "dessine au zoom courant, avec ses barres, son nom, sa barre de cast et sa "
                "zone cliquable.\n\n"
                "La vie et la magie y varient du vide au plein — sur les entités à l'écran "
                "aussi —, et la barre de cast s'y remplit en boucle : on voit en direct ce "
                "qu'un réglage change, seuil d'alerte compris. Les valeurs sont factices, et "
                "ne s'affichent que tant que ce panneau est ouvert."),
             nullptr, entity_bars_.tuning));
  entity_bar_shared_rows_.back().fold = Tr("Pour toutes les espèces");
  const std::size_t bars_first = entity_bar_shared_rows_.size();
  entity_bar_shared_rows_.push_back(
      Toggle(GameOption::kEntityBarsShown, Tr("Montrer les barres d'entité"),
             Tr("Les barres de vie et de magie posées au-dessus — ou plutôt sous les pieds — "
                "de ce qui peuple la carte.\n\n"
                "C'est ce que le client d'origine peint au-dessus d'un monstre entamé : "
                "soixante pixels sur cinq, verts, rouges sous un quart de vie."),
             nullptr, entity_bars_.shown));
  entity_bar_shared_rows_[bars_first].heading = Tr("Barres");
  entity_bar_shared_rows_.push_back(
      Toggle(GameOption::kEntityBarsHideVendors, Tr("Masquer les barres des marchands"),
             Tr("Qui tient une échoppe, de vente ou d'achat, ne porte plus de barre, quel "
                "que soit le réglage de son onglet. Un salon de discussion ne compte pas.\n\n"
                "Utile surtout au staff : là où le serveur donne la vie de tous les joueurs, "
                "une rue de marchands se couvre de barres pleines."),
             nullptr, entity_bars_.hide_vendors));
  entity_bar_shared_rows_.back().indented = true;
  entity_bar_shared_rows_.back().disabled = !entity_bars_.shown;

  // LES ONGLETS. ⚠ REFAITS À CHAQUE IMAGE : ces noms se traduisent, et un nom
  // mis en cache resterait dans la langue d'avant.
  entity_bar_tabs_.clear();
  entity_bar_tabs_.reserve(kEntityPageTabCount);
  for (std::size_t tab = 0; tab < kEntityPageTabCount; ++tab) {
    entity_bar_tabs_.push_back(EntityPageTabName(static_cast<EntityPageTab>(tab)));
  }
}

// CE QUE LE SERVEUR DIT DE LA VIE DE CETTE ESPÈCE-LÀ — l'infobulle qui évite de
// chercher une panne qui n'existe pas.
//
// 🔴 PAR ESPÈCE, PARCE QUE LA RÉPONSE L'EST. Un texte commun dirait « selon les
// cas », ce qui n'aide personne à comprendre pourquoi la barre d'un adversaire
// reste vide alors que celle du monstre d'à côté se remplit.
const char* GameOptions::EntityBarSourceHelp(std::size_t kind) {
  switch (static_cast<ro::ui::EntityBarKind>(kind)) {
    case ro::ui::EntityBarKind::kOwn:
      return Tr(
          "Ta vie et ta magie sont connues en permanence : le serveur te les envoie en "
          "clair.");
    case ro::ui::EntityBarKind::kParty:
      return Tr(
          "La vie d'un compagnon de groupe arrive par un paquet à elle "
          "(ZC_NOTIFY_HP_TO_GROUPM), et c'est la seule source : hors du groupe, elle "
          "reste inconnue.\n\n"
          "Sa magie, elle, n'est dans aucun paquet du jeu — sauf s'il est la cible.");
    case ro::ui::EntityBarKind::kPlayer:
      return Tr(
          "La vie d'un joueur hors de ton groupe n'est connue que s'il est ta CIBLE, et "
          "seulement si le serveur accepte de la dire — à ta guilde, à ton groupe, pas "
          "à un adversaire.\n\n"
          "Une barre réglée sur « toujours » ne paraîtra donc pas plus souvent — sauf "
          "pour le staff à qui le serveur donne la vie de tous les joueurs de la zone.");
    case ro::ui::EntityBarKind::kMonster:
    case ro::ui::EntityBarKind::kBoss:
      return Tr(
          "La vie d'un monstre est annoncée à tous, mais SEULEMENT UNE FOIS ENTAMÉ : un "
          "monstre intact n'en a pas, et ce n'est pas une panne.\n\n"
          "Sa magie n'existe pas — le serveur lui en donne un point pour éviter des "
          "divisions par zéro, et ce point ne veut rien dire.");
    case ro::ui::EntityBarKind::kOther:
      return Tr(
          "PNJ, familiers, homoncules, mercenaires : leur vie n'arrive que par le "
          "dossier de la CIBLE, quand le serveur la donne. Ailleurs, rien ne la porte.");
  }
  return nullptr;
}

// LES VINGT-CINQ RÉGLAGES D'UNE ESPÈCE.
//
// 🔴 CE QUI NE S'APPLIQUE PAS EST GRISÉ, JAMAIS RETIRÉ. Une ligne qui disparaît
// quand on éteint son maître fait sauter la mise en page, et le joueur cherche
// ce qu'il vient de perdre. Grisée, elle dit « pas maintenant », et OÙ elle est.
void GameOptions::BuildEntityBarRows(int shown_tab) {
  // 🔴 L'ONGLET N'EST PLUS UNE ESPÈCE DE BARRE : la page porte les barres ET les
  // noms, et leurs espèces diffèrent. Voir `entity_page_tabs.h`.
  const EntityPageTab tab = EntityPageTabOf(shown_tab);
  const ro::ui::EntitySampleShown settles = EntityPageSettles(tab);
  BuildEntityBarShared();

  const bool bars_off = !entity_bars_.shown;
  entity_bar_rows_.clear();
  pages_.cast_bars.Compose(settles.bars);

  // 🔴 NI UN `static`, NI UN LOCAL : une rangée ne COPIE pas ses choix, elle en
  // garde un `span`. Un `static` les capture une fois et le catalogue qui les
  // porte est remplacé au premier changement de langue ; un tableau local meurt
  // avant le dessin. Les deux mènent au même écran noir.
  entity_bar_when_names_.clear();
  entity_bar_when_names_.push_back(Tr("Toujours"));
  entity_bar_when_names_.push_back(Tr("Une fois entamé"));
  entity_bar_when_names_.push_back(Tr("Au survol"));
  entity_bar_when_names_.push_back(Tr("La cible"));
  entity_bar_when_names_.push_back(Tr("Survol ou cible"));
  entity_bar_text_names_.clear();
  entity_bar_text_names_.push_back(Tr("Rien"));
  entity_bar_text_names_.push_back(Tr("Pour cent"));
  entity_bar_text_names_.push_back(Tr("Valeurs"));
  entity_bar_text_names_.push_back(Tr("Les deux"));

  const auto push = [this](OptionRowView row) { entity_bar_rows_.push_back(row); };

  // L'ENTITÉ, SES BARRES ET SON NOM, posés comme en jeu, en haut de la page —
  // en mode réglage seulement : c'est l'outil de ce mode, et il prend de la
  // place.
  //
  // 🔴 LA VIGNETTE SE LIT AU MOMENT DE PEINDRE, par son adresse, jamais copiée à
  // la composition. La partie la recompose à chaque image, APRÈS cette page, et
  // une copie retenait le numéro d'une texture déjà remplacée — que le pilote
  // rendait à la suivante venue : l'atlas des polices s'affichait à sa place,
  // par éclairs. Les réglages, eux, se copient : ils ne changent qu'entre deux
  // images.
  // ⚠ SAUF SUR LES ENSEIGNES : elles n'ont pas d'entité d'essai, et se
  // regardent sur la carte, derrière le panneau.
  pages_.entity_pinned_rows.clear();
  const bool signboards = tab == EntityPageTab::kSignboards;
  if (entity_bars_.tuning && !signboards) {
    const ro::ui::EntitySample* sample = &pages_.entity_sample;
    const ro::ui::EntityBarsLook bars = entity_bars_;
    const ro::render::EntityNamesLook names = entity_names_;
    const ro::ui::CastBarsLook cast = pages_.cast_bars.look();
    const ro::ui::EntitySampleShown drawn = EntityPageDrawn(tab);
    // LA ZONE QUI RÉPOND AU CLIC, du plancher de la famille : un item au sol a
    // le sien, plus petit que celui d'un acteur.
    const ro::ui::EntityPickZones zones = pages_.pick_precision.ZonesFor(
        tab == EntityPageTab::kGroundItem ? ro::render::kGroundPickFloorFactor
                                          : ro::render::kActorPickFloorFactor,
        ImGui::GetIO().DisplaySize.x);
    // ⚠ ÉPINGLÉ, HORS DES RANGÉES : il reste à l'écran quand la page défile.
    pages_.entity_pinned_rows.push_back(
        OptionRowView{.id = kEntityBarSampleRow,
                      .kind = OptionKind::kCanvas,
                      .canvas_height = ro::ui::EntitySampleHeight(
                          *sample, ro::ui::EntitySampleFloor(*sample, bars, names, drawn),
                          ro::ui::EntitySampleHeadroom(*sample, cast, drawn, ImGui::GetFontSize())),
                      .paint = [sample, bars, names, cast, drawn, zones](ImDrawList& into,
                                                                         ImVec2 min, ImVec2 max) {
                        ro::ui::PaintEntitySample(
                            into, min, max, *sample,
                            ro::ui::EntitySampleLooks{.bars = bars, .names = names, .cast = cast},
                            drawn, zones, ImGui::GetTime());
                      }});
  }

  // ── LES BARRES ────────────────────────────────────────────────────────────
  if (settles.bars.has_value()) {
    const auto kind = static_cast<std::size_t>(*settles.bars);
    ro::ui::EntityBarLook& look = entity_bars_.kinds[kind];
    const bool off = bars_off || !look.shown;
    const bool mute = look.text == ro::ui::EntityBarText::kNothing;
    const std::size_t first = entity_bar_rows_.size();
    push(OptionRowView{.id = EntityBarRow(kind, EntityBarField::kShown),
                       .label = Tr("Montrer pour cette espèce"),
                       .help = EntityBarSourceHelp(kind),
                       .kind = OptionKind::kToggle,
                       .on = look.shown,
                       .disabled = bars_off});
    push(OptionRowView{.id = EntityBarRow(kind, EntityBarField::kHealth),
                       .label = Tr("Barre de vie"),
                       .kind = OptionKind::kToggle,
                       .on = look.health,
                       .indented = true,
                       .disabled = off});
    push(OptionRowView{
        .id = EntityBarRow(kind, EntityBarField::kMagic),
        .label = Tr("Barre de magie"),
        .help = Tr("Elle ne paraît que là où la donnée existe — sur toi, et sur ta cible par "
                   "le paquet privé du serveur. Aucun paquet du jeu ne porte la magie d'une "
                   "entité tierce : allumée ailleurs, elle ne s'affichera jamais plutôt que de "
                   "montrer une barre vide."),
        .kind = OptionKind::kToggle,
        .on = look.magic,
        .indented = true,
        .disabled = off});
    push(OptionRowView{.id = EntityBarRow(kind, EntityBarField::kWhen),
                       .label = Tr("Quand la montrer"),
                       .help = Tr("« Une fois entamé » est ce que fait le client d'origine : le "
                                  "serveur n'annonce la vie d'un monstre qu'à partir du premier "
                                  "coup, et une carte de moutons intacts reste lisible."),
                       .kind = OptionKind::kChoice,
                       .chosen = static_cast<int>(look.when),
                       .choices = entity_bar_when_names_,
                       .disabled = off});
    push(OptionRowView{.id = EntityBarRow(kind, EntityBarField::kHideDead),
                       .label = Tr("Cacher les morts"),
                       .help = Tr("Un mort n'a plus de vie à montrer, et le serveur ne la reprend "
                                  "pas : sa dernière barre resterait affichée sur le cadavre."),
                       .kind = OptionKind::kToggle,
                       .on = look.hide_dead,
                       .disabled = off});

    // ── LA PLACE ──────────────────────────────────────────────────────────────
    // ⚠ SUIVRE LE ZOOM N'EST PAS UNE RANGÉE, et ce n'est plus un oubli ni un
    // choix : la barre ne suit plus le zoom du tout, comme chez le client. Seul
    // l'ÉCART s'échelonne, sur la fenêtre et sur la taille du corps. Voir
    // `ro::render::ScreenAnchorScale`.
    push(OptionRowView{
        .id = EntityBarRow(kind, EntityBarField::kUnderFeet),
        .label = Tr("Sous les pieds"),
        .help = Tr("La place du client d'origine : douze pixels sous les pieds, là où s'écrit "
                   "aussi le nom.\n\n"
                   "Décochée, la barre passe AU-DESSUS du sprite — elle suit alors sa taille, "
                   "donc elle monte au-dessus d'un grand monstre."),
        .kind = OptionKind::kToggle,
        .on = look.under_feet,
        .heading = Tr("Place et taille"),
        .disabled = off});
    push(OptionRowView{.id = EntityBarRow(kind, EntityBarField::kOffset),
                       .label = Tr("Écart"),
                       .help = Tr("De combien la barre s'éloigne des pieds, ou du haut du sprite."),
                       .kind = OptionKind::kSlider,
                       .value = look.offset_pixels,
                       .lowest = 0.0F,
                       .highest = ro::ui::kLargestEntityBarOffset,
                       .format = kPixelFormat,
                       .indented = true,
                       .disabled = off});
    push(OptionRowView{.id = EntityBarRow(kind, EntityBarField::kWidth),
                       .label = Tr("Largeur"),
                       .kind = OptionKind::kSlider,
                       .value = look.width_pixels,
                       .lowest = ro::ui::kSmallestEntityBarWidth,
                       .highest = ro::ui::kLargestEntityBarWidth,
                       .format = kPixelFormat,
                       .disabled = off});
    push(OptionRowView{.id = EntityBarRow(kind, EntityBarField::kHeight),
                       .label = Tr("Hauteur de la vie"),
                       .kind = OptionKind::kSlider,
                       .value = look.height_pixels,
                       .lowest = ro::ui::kSmallestEntityBarHeight,
                       .highest = ro::ui::kLargestEntityBarHeight,
                       .format = kPixelFormat,
                       .disabled = off || !look.health});
    push(OptionRowView{.id = EntityBarRow(kind, EntityBarField::kMagicHeight),
                       .label = Tr("Hauteur de la magie"),
                       .help = Tr("Plus fine que la vie, par défaut. Le client, lui, coupe sa "
                                  "barre en deux moitiés égales — et les rend illisibles toutes "
                                  "les deux."),
                       .kind = OptionKind::kSlider,
                       .value = look.magic_height_pixels,
                       .lowest = ro::ui::kSmallestEntityBarHeight,
                       .highest = ro::ui::kLargestEntityBarHeight,
                       .format = kPixelFormat,
                       .disabled = off || !look.magic});
    push(OptionRowView{.id = EntityBarRow(kind, EntityBarField::kGap),
                       .label = Tr("Écart entre les deux"),
                       .kind = OptionKind::kSlider,
                       .value = look.gap_pixels,
                       .lowest = 0.0F,
                       .highest = ro::ui::kLargestEntityBarGap,
                       .format = kPixelFormat,
                       .disabled = off || !look.magic || !look.health});

    // ── LES COULEURS ──────────────────────────────────────────────────────────
    push(OptionRowView{.id = EntityBarRow(kind, EntityBarField::kFill),
                       .label = Tr("Vie"),
                       .help = Tr("Le vert du client, relevé dans son constructeur de jauge."),
                       .kind = OptionKind::kColor,
                       .color = look.fill,
                       .heading = Tr("Couleurs"),
                       .disabled = off || !look.health});
    push(OptionRowView{
        .id = EntityBarRow(kind, EntityBarField::kLowFill),
        .label = Tr("Vie en alerte"),
        .help = Tr("La couleur sous le seuil. Le client bascule d'un coup à un quart de vie, et "
                   "cette bascule est un SIGNAL : on la voit du coin de l'œil."),
        .kind = OptionKind::kColor,
        .color = look.low_fill,
        .disabled = off || !look.health});
    push(OptionRowView{.id = EntityBarRow(kind, EntityBarField::kLowShare),
                       .label = Tr("Seuil d'alerte"),
                       .help = Tr("Un quart chez le client, et c'est écrit dans son code de "
                                  "peinture, pas déduit d'une capture."),
                       .kind = OptionKind::kSlider,
                       .value = look.low_share * kPercentScale,
                       .lowest = 0.0F,
                       .highest = kPercentScale,
                       .format = kPercentFormat,
                       .indented = true,
                       .disabled = off || !look.health});
    push(OptionRowView{
        .id = EntityBarRow(kind, EntityBarField::kGradient),
        .label = Tr("Dégradé continu"),
        .help = Tr("La couleur glisse vers l'alerte à mesure qu'on descend sous le seuil, au "
                   "lieu de basculer d'un coup.\n\n"
                   "Cela s'écarte du client, et perd son signal : un dégradé ne se remarque pas."),
        .kind = OptionKind::kToggle,
        .on = look.gradient,
        .indented = true,
        .disabled = off || !look.health});
    push(OptionRowView{.id = EntityBarRow(kind, EntityBarField::kMagicFill),
                       .label = Tr("Magie"),
                       .kind = OptionKind::kColor,
                       .color = look.magic_fill,
                       .disabled = off || !look.magic});
    push(OptionRowView{.id = EntityBarRow(kind, EntityBarField::kBackground),
                       .label = Tr("Creux"),
                       .help = Tr("Le fond, celui qu'on voit à la place de ce qui manque."),
                       .kind = OptionKind::kColor,
                       .color = look.background,
                       .disabled = off});
    push(OptionRowView{.id = EntityBarRow(kind, EntityBarField::kBorder),
                       .label = Tr("Cadre"),
                       .kind = OptionKind::kToggle,
                       .on = look.border,
                       .disabled = off});
    push(OptionRowView{.id = EntityBarRow(kind, EntityBarField::kBorderColor),
                       .label = Tr("Couleur du cadre"),
                       .kind = OptionKind::kColor,
                       .color = look.border_color,
                       .indented = true,
                       .disabled = off || !look.border});
    push(OptionRowView{.id = EntityBarRow(kind, EntityBarField::kRounding),
                       .label = Tr("Arrondi"),
                       .help = Tr("Le cadre le suit : il se pose SUR la barre, il ne la rogne "
                                  "pas.\n\n"
                                  "Au-delà de la moitié de la hauteur, le curseur ne change plus "
                                  "rien : un coin ne peut pas être plus rond que la barre n'est "
                                  "haute. Sur une barre de cinq pixels, deux suffisent donc à "
                                  "l'arrondir autant que possible."),
                       .kind = OptionKind::kSlider,
                       .value = look.rounding,
                       .lowest = 0.0F,
                       .highest = ro::ui::kLargestEntityBarHeight * 0.5F,
                       .format = kPixelFormat,
                       .disabled = off});
    push(OptionRowView{.id = EntityBarRow(kind, EntityBarField::kOpacity),
                       .label = Tr("Opacité"),
                       .help = Tr("Appliquée à tout : cadre, creux, remplissage et texte."),
                       .kind = OptionKind::kSlider,
                       .value = look.opacity * kPercentScale,
                       .lowest = 0.0F,
                       .highest = kPercentScale,
                       .format = kPercentFormat,
                       .disabled = off});

    // ── LE TEXTE ──────────────────────────────────────────────────────────────
    push(OptionRowView{
        .id = EntityBarRow(kind, EntityBarField::kText),
        .label = Tr("Ce qu'elle écrit"),
        .help = Tr("Le client n'écrit rien sur ces barres : sa peinture ne pose que des "
                   "rectangles. Vingt barres chiffrées sur une carte peuplée ne se lisent "
                   "plus — mais sur un MVP qu'on suit dix minutes, le pour cent aide."),
        .kind = OptionKind::kChoice,
        .chosen = static_cast<int>(look.text),
        .choices = entity_bar_text_names_,
        .heading = Tr("Texte"),
        .disabled = off || !look.health});
    push(OptionRowView{.id = EntityBarRow(kind, EntityBarField::kFontPixels),
                       .label = Tr("Taille du texte"),
                       .kind = OptionKind::kSlider,
                       .value = look.font_pixels,
                       .lowest = ro::ui::kSmallestEntityBarFont,
                       .highest = ro::ui::kLargestEntityBarFont,
                       .format = kPixelFormat,
                       .indented = true,
                       .disabled = off || mute || !look.health});
    push(OptionRowView{.id = EntityBarRow(kind, EntityBarField::kTextColor),
                       .label = Tr("Couleur du texte"),
                       .kind = OptionKind::kColor,
                       .color = look.text_color,
                       .indented = true,
                       .disabled = off || mute || !look.health});
    entity_bar_rows_[first].fold = Tr("Barres");
  }

  // ── LE NOM ────────────────────────────────────────────────────────────────
  BuildEntityNameRows(settles.name.has_value() ? static_cast<int>(*settles.name) : -1);
  if (!entity_name_rows_.empty()) {
    entity_name_rows_.front().fold = Tr("Nom");
    entity_bar_rows_.insert(entity_bar_rows_.end(), entity_name_rows_.begin(),
                            entity_name_rows_.end());
  } else if (!signboards) {
    push(OptionRowView{.id = kEntityNameNoteRow,
                       .label = Tr("Les noms du groupe se règlent avec ceux des joueurs, dans "
                                   "l'onglet « Joueurs » : leur carte de nom est la même."),
                       .kind = OptionKind::kNote,
                       .fold = Tr("Nom")});
  }

  // ── LE CAST ───────────────────────────────────────────────────────────────
  const std::span<const OptionRowView> cast = pages_.cast_bars.rows();
  if (!cast.empty()) {
    const std::size_t cast_first = entity_bar_rows_.size();
    entity_bar_rows_.insert(entity_bar_rows_.end(), cast.begin(), cast.end());
    entity_bar_rows_[cast_first].fold = Tr("Cast");
  }

  // ── LES ENSEIGNES ─────────────────────────────────────────────────────────
  if (signboards) {
    pages_.signboards.ComposeInto(entity_bar_rows_, pages_.shops);
  }

  // ── CE QUI VAUT POUR TOUS LES ONGLETS : barres, noms, cast ───────────────
  entity_name_shared_rows_.front().heading = Tr("Noms");
  entity_bar_shared_rows_.insert(entity_bar_shared_rows_.end(), entity_name_shared_rows_.begin(),
                                 entity_name_shared_rows_.end());
  const std::span<const OptionRowView> cast_shared = pages_.cast_bars.shared_rows();
  const std::size_t cast_shared_first = entity_bar_shared_rows_.size();
  entity_bar_shared_rows_.insert(entity_bar_shared_rows_.end(), cast_shared.begin(),
                                 cast_shared.end());
  if (cast_shared_first < entity_bar_shared_rows_.size()) {
    entity_bar_shared_rows_[cast_shared_first].heading = Tr("Cast");
  }
  // ET LA ZONE QUI RÉPOND AU CLIC, dans son bloc : elle aussi vaut pour toutes
  // les espèces, et le cadre la montre autour de l'entité.
  const std::span<const OptionRowView> pick = pages_.pick_precision.rows();
  const std::size_t pick_first = entity_bar_shared_rows_.size();
  entity_bar_shared_rows_.insert(entity_bar_shared_rows_.end(), pick.begin(), pick.end());
  if (pick_first < entity_bar_shared_rows_.size()) {
    entity_bar_shared_rows_[pick_first].heading = Tr("Zone cliquable");
  }
}

// CE QU'UNE LIGNE DE BARRE D'ENTITÉ CHANGE.
//
// ⚠ MÊME MÉCANIQUE QUE LES JAUGES LIBRES, et elle rejette les leurs : leurs
// identifiants sont sous `kEntityBarRowBase`, donc l'ordre d'appel des trois
// aiguillages encodés n'a pas d'importance.
//
// ⚠ LES RÉGLAGES COMMUNS PASSENT AUSSI PAR ICI, bien qu'ils aient un
// identifiant ordinaire : ils vivent avec les lignes qui les composent.
//
// 🔴 COMPARÉS EN ENTIERS, SANS CONVERSION VERS `GameOption` : l'énumération
// tient sur un octet, et l'identifiant d'une rangée encodée, tronqué, tomberait
// sur l'un de ces trois.
bool GameOptions::ChangeEntityBar(const ro::ui::OptionsWindowAction& asked) {
  const auto is = [&asked](GameOption which) { return asked.changed == static_cast<int>(which); };
  if (is(GameOption::kEntityBarsShown)) {
    entity_bars_.shown = asked.on;
    return true;
  }
  if (is(GameOption::kEntityBarsHideVendors)) {
    entity_bars_.hide_vendors = asked.on;
    return true;
  }
  if (is(GameOption::kEntityBarsTuning)) {
    entity_bars_.tuning = asked.on;
    return true;
  }
  if (asked.changed < kEntityBarRowBase) {
    return false;
  }
  const int offset = asked.changed - kEntityBarRowBase;
  const auto kind = static_cast<std::size_t>(offset / kEntityBarFieldsPerKind);
  if (kind >= ro::ui::kEntityBarKindCount) {
    return false;
  }
  ro::ui::EntityBarLook& look = entity_bars_.kinds[kind];
  switch (static_cast<EntityBarField>(offset % kEntityBarFieldsPerKind)) {
    case EntityBarField::kShown:
      look.shown = asked.on;
      break;
    case EntityBarField::kHealth:
      look.health = asked.on;
      break;
    case EntityBarField::kMagic:
      look.magic = asked.on;
      break;
    case EntityBarField::kWhen:
      look.when = ro::ui::EntityBarWhenOf(asked.chosen);
      break;
    case EntityBarField::kHideDead:
      look.hide_dead = asked.on;
      break;
    case EntityBarField::kUnderFeet:
      look.under_feet = asked.on;
      break;
    case EntityBarField::kOffset:
      look.offset_pixels = asked.value;
      break;
    case EntityBarField::kWidth:
      look.width_pixels = asked.value;
      break;
    case EntityBarField::kHeight:
      look.height_pixels = asked.value;
      break;
    case EntityBarField::kMagicHeight:
      look.magic_height_pixels = asked.value;
      break;
    case EntityBarField::kGap:
      look.gap_pixels = asked.value;
      break;
    case EntityBarField::kFill:
      look.fill = asked.color;
      break;
    case EntityBarField::kLowFill:
      look.low_fill = asked.color;
      break;
    case EntityBarField::kLowShare:
      // ⚠ LE CURSEUR COMPTE EN POUR CENT, LE RÉGLAGE EN PART : diviser ici et
      // multiplier à l'affichage, jamais l'inverse.
      look.low_share = asked.value / kPercentScale;
      break;
    case EntityBarField::kGradient:
      look.gradient = asked.on;
      break;
    case EntityBarField::kMagicFill:
      look.magic_fill = asked.color;
      break;
    case EntityBarField::kBackground:
      look.background = asked.color;
      break;
    case EntityBarField::kBorder:
      look.border = asked.on;
      break;
    case EntityBarField::kBorderColor:
      look.border_color = asked.color;
      break;
    case EntityBarField::kRounding:
      look.rounding = asked.value;
      break;
    case EntityBarField::kOpacity:
      look.opacity = asked.value / kPercentScale;
      break;
    case EntityBarField::kText:
      look.text = ro::ui::EntityBarTextOf(asked.chosen);
      break;
    case EntityBarField::kFontPixels:
      look.font_pixels = asked.value;
      break;
    case EntityBarField::kTextColor:
      look.text_color = asked.color;
      break;
    case EntityBarField::kCount:
      // Jamais encodé : il ne compte les champs que pour la garde d'en-tête.
      break;
  }
  return true;
}

}  // namespace moonclient
