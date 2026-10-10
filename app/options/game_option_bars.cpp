// LES RANGÉES DES BARRES DE RACCOURCIS : dix-neuf par barre, toutes composées.
//
// ⚠ À PART DU CATALOGUE D'À CÔTÉ, et pas pour la longueur : celui-là est une
// liste de littéraux qu'on copie l'un sur l'autre, celui-ci est une BOUCLE. Les
// huit barres portent les mêmes réglages ; les écrire à la main serait cent
// cinquante occasions de se tromper de numéro, et autant de plus le jour où une
// neuvième s'ajoute.
//
// 🔴 UNE BARRE À LA FOIS, ET C'EST LA SECTION QUI LE DEMANDE. Huit fois dix-neuf
// font cent cinquante-deux lignes qui se ressemblent toutes : le joueur y
// chercherait « la taille des cases de la barre 5 » en faisant défiler, et en
// comptant. La section porte donc une seconde navigation — un onglet par barre —
// et l'on ne compose que les lignes de celle qu'il regarde.
//
// 🔴 ET LES LIBELLÉS SE COMPOSENT, ce que le reste du catalogue ne fait jamais.
// D'où le tampon de chaînes, rempli EN ENTIER avant qu'une seule ligne ne le
// pointe : une vue prise en cours de remplissage pendrait à la première
// réallocation du vecteur.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>

#include "ro/core/i18n.h"
#include "ro/ui/shortcut_bar.h"

#include "options/game_options.h"
#include "options/option_row_kinds.h"

namespace moonclient {
namespace {

using ro::core::Tr;
using ro::ui::OptionKind;
using ro::ui::OptionRowView;

// LES TROIS FAÇONS DE DÉTACHER UN TEXTE DE CE QU'IL Y A DESSOUS, REFAITES À
// CHAQUE IMAGE.
//
// 🔴 ET SURTOUT PAS DANS UN `static`. Le pointeur que `Tr` rend appartient au
// catalogue et MEURT au changement de langue — `ro/core/i18n.h` le dit à la
// ligne où il le rend. Une liste gardée une fois pour toutes montrerait donc de
// la mémoire libérée dès que le joueur change de langue, et ce n'est pas un
// affichage fautif : c'est une lecture après libération.
//
// ⚠ ÉCRITS ICI, EN TOUTES LETTRES : le relevé des traductions ne lit qu'un appel
// porté sur un littéral, et passer par une constante les rendrait invisibles aux
// catalogues.
//
// ⚠ ET LEUR ORDRE EST CELUI DE `ro::ui::SlotTextShadow`, qui est celui du
// fichier de réglages : un libellé intercalé décalerait ce qu'un joueur a déjà
// choisi.
void FillShadowNames(std::vector<const char*>& shadows) {
  shadows.clear();
  shadows.push_back(Tr("Aucune"));
  shadows.push_back(Tr("Contour"));
  shadows.push_back(Tr("Ombre portée"));
}

[[nodiscard]] std::string BarName(std::size_t bar) {
  std::array<char, 32> text{};
  std::snprintf(text.data(), text.size(), "%s %zu", Tr("Barre"), bar + 1);
  return std::string(text.data());
}

}  // namespace

void GameOptions::ComposeShortcutRows(int shown_bar) {
  // ⚠ REMPLI EN ENTIER AVANT QU'UNE SEULE LIGNE NE LE POINTE : une vue prise en
  // cours de remplissage pendrait à la première réallocation du vecteur.
  FillShadowNames(shortcut_shadow_names_);

  // LES ONGLETS : un par barre, et leurs noms dans un tampon stable.
  //
  // ⚠ REFAITS À CHAQUE IMAGE comme tout le reste : « Barre » se traduit, et un
  // nom mis en cache resterait dans la langue d'avant.
  shortcut_tab_names_.clear();
  shortcut_tab_names_.reserve(ro::ui::kShortcutBarCount);
  for (std::size_t bar = 0; bar < ro::ui::kShortcutBarCount; ++bar) {
    shortcut_tab_names_.push_back(BarName(bar));
  }
  shortcut_tabs_.clear();
  shortcut_tabs_.reserve(shortcut_tab_names_.size());
  for (const std::string& name : shortcut_tab_names_) {
    shortcut_tabs_.push_back(name.c_str());
  }

  // 🔴 LES DEUX RÉGLAGES COMMUNS VONT AU-DESSUS DES ONGLETS, et la place est
  // tout ce qui les distingue. Posés sous eux, ils se lisaient comme ceux de la
  // barre ouverte — et qui décoche « verrouillées » sur l'onglet 3 croit n'avoir
  // déverrouillé que la barre 3. Aucun libellé ne pouvait le détromper.
  shortcut_shared_rows_.clear();
  shortcut_shared_rows_.push_back(
      OptionRowView{.id = kShortcutLockedRow,
                    .label = Tr("Barres verrouillées"),
                    .help = Tr("Verrouillées, les barres ne se déplacent plus — leurs cases "
                               "restent utilisables. Déverrouillées, on les attrape n'importe "
                               "où, et leurs cases deviennent inertes."),
                    .kind = OptionKind::kToggle,
                    .on = shortcut_bars_.locked});
  shortcut_shared_rows_.push_back(OptionRowView{.id = kShortcutShowKeysRow,
                                                .label = Tr("Afficher les touches sur les cases"),
                                                .kind = OptionKind::kToggle,
                                                .on = shortcut_bars_.show_keys});
  ComposeArmedSkillRows();

  shortcut_rows_.clear();
  const auto bar = static_cast<std::size_t>(
      std::clamp(shown_bar, 0, static_cast<int>(ro::ui::kShortcutBarCount) - 1));
  const ro::ui::ShortcutBarLayout& look = shortcut_bars_.bars[bar];
  shortcut_rows_.push_back(OptionRowView{.id = ShortcutBarRow(bar, ShortcutBarField::kShown),
                                         .label = Tr("Afficher cette barre"),
                                         .kind = OptionKind::kToggle,
                                         .on = look.shown});
  // ⚠ LE RESTE NE PARAÎT QUE SI LA BARRE EST MONTRÉE : régler la couleur d'une
  // barre éteinte ne veut rien dire, et l'onglet dirait alors qu'il se passe
  // quelque chose.
  if (!look.shown) {
    return;
  }

  shortcut_rows_.push_back(
      OptionRowView{.id = ShortcutBarRow(bar, ShortcutBarField::kClickThrough),
                    .label = Tr("Le clic la traverse"),
                    .help = Tr("La barre ne prend plus rien à la souris : on clique le monde "
                               "au travers. Maintenir Maj la rend vivante le temps d'un geste "
                               "— c'est ainsi qu'on la remplit ou qu'on la déplace encore."),
                    .kind = OptionKind::kToggle,
                    .on = look.click_through});
  shortcut_rows_.push_back(
      OptionRowView{.id = ShortcutBarRow(bar, ShortcutBarField::kColumns),
                    .label = Tr("Colonnes"),
                    .help = Tr("Une seule colonne donne une barre verticale."),
                    .kind = OptionKind::kSlider,
                    .value = static_cast<float>(look.columns),
                    .lowest = 1.0F,
                    .highest = static_cast<float>(ro::ui::kShortcutSlotsPerBar),
                    .format = "%.0f"});
  shortcut_rows_.push_back(
      OptionRowView{.id = ShortcutBarRow(bar, ShortcutBarField::kShownSlots),
                    .label = Tr("Cases montrées"),
                    .help = Tr("En montrer moins n'efface rien : les cases cachées gardent "
                               "leur contenu et leur touche."),
                    .kind = OptionKind::kSlider,
                    .value = static_cast<float>(look.shown_slots),
                    .lowest = 1.0F,
                    .highest = static_cast<float>(ro::ui::kShortcutSlotsPerBar),
                    .format = "%.0f"});
  shortcut_rows_.push_back(OptionRowView{.id = ShortcutBarRow(bar, ShortcutBarField::kSlotSide),
                                         .label = Tr("Taille des cases"),
                                         .kind = OptionKind::kSlider,
                                         .value = look.slot_side_px,
                                         .lowest = ro::ui::kSmallestSlotSidePixels,
                                         .highest = ro::ui::kLargestSlotSidePixels,
                                         .format = "%.0f px"});
  shortcut_rows_.push_back(OptionRowView{.id = ShortcutBarRow(bar, ShortcutBarField::kGap),
                                         .label = Tr("Espacement"),
                                         .kind = OptionKind::kSlider,
                                         .value = look.gap_px,
                                         .lowest = 0.0F,
                                         .highest = ro::ui::kLargestSlotGapPixels,
                                         .format = "%.0f px"});

  // ── LES TEXTES, AVANT LES NUANCIERS ──────────────────────────────────────
  //
  // ⚠ LES NUANCIERS EN DERNIER, comme pour les bulles : leur sélecteur s'ouvre
  // VERS LE BAS et recouvre les rangées suivantes.
  shortcut_rows_.push_back(
      OptionRowView{.id = ShortcutBarRow(bar, ShortcutBarField::kKeyScale),
                    .label = Tr("Taille du texte des touches"),
                    .help = Tr("Un multiplicateur, et non des points : une case va de seize à "
                               "soixante-quatre pixels. Un libellé qui déborde rétrécit de "
                               "lui-même."),
                    .kind = OptionKind::kSlider,
                    .value = look.key_scale,
                    .lowest = ro::ui::kSmallestLabelScale,
                    .highest = ro::ui::kLargestLabelScale,
                    .format = "%.2fx"});
  shortcut_rows_.push_back(
      OptionRowView{.id = ShortcutBarRow(bar, ShortcutBarField::kCountScale),
                    .label = Tr("Taille du texte des nombres"),
                    .help = Tr("Le rang d'un skill, la quantité d'un item, et le temps "
                               "qui reste."),
                    .kind = OptionKind::kSlider,
                    .value = look.count_scale,
                    .lowest = ro::ui::kSmallestLabelScale,
                    .highest = ro::ui::kLargestLabelScale,
                    .format = "%.2fx"});
  shortcut_rows_.push_back(
      OptionRowView{.id = ShortcutBarRow(bar, ShortcutBarField::kShadowKind),
                    .label = Tr("Ombre des textes"),
                    .help = Tr("Le contour entoure, l'ombre portée se décale. Sur un fond uni "
                               "qu'on a choisi, aucune des deux n'est obligatoire."),
                    .kind = OptionKind::kChoice,
                    .chosen = look.shadow_kind,
                    .choices = shortcut_shadow_names_});
  if (look.shadow_kind == static_cast<int>(ro::ui::SlotTextShadow::kDrop)) {
    // ⚠ SEULEMENT POUR L'OMBRE PORTÉE : un décalage n'a aucun sens sur un
    // contour, qui entoure. Le montrer quand même offrirait un curseur sans
    // effet, ce qui se lit « cassé ».
    shortcut_rows_.push_back(
        OptionRowView{.id = ShortcutBarRow(bar, ShortcutBarField::kShadowOffset),
                      .label = Tr("Décalage de l'ombre"),
                      .kind = OptionKind::kSlider,
                      .value = look.shadow_offset,
                      .lowest = 0.0F,
                      .highest = ro::ui::kLargestShadowOffset,
                      .format = "%.0f px"});
  }
  shortcut_rows_.push_back(
      OptionRowView{.id = ShortcutBarRow(bar, ShortcutBarField::kBorderThickness),
                    .label = Tr("Épaisseur du cadre"),
                    .help = Tr("Zéro éteint le cadre : des cases jointives sans bordure est une "
                               "disposition qui se défend."),
                    .kind = OptionKind::kSlider,
                    .value = look.border_thickness,
                    .lowest = 0.0F,
                    .highest = ro::ui::kLargestBorderThickness,
                    .format = "%.0f px"});
  shortcut_rows_.push_back(
      OptionRowView{.id = ShortcutBarRow(bar, ShortcutBarField::kCopyLook),
                    .label = Tr("Copier cette apparence sur les autres barres"),
                    .help = Tr("Les couleurs, les tailles et le cadre — pas la position, ni ce "
                               "que chaque barre montre."),
                    .kind = OptionKind::kAction});

  // ── LES NUANCIERS, EN DERNIER ────────────────────────────────────────────
  shortcut_rows_.push_back(OptionRowView{.id = ShortcutBarRow(bar, ShortcutBarField::kEmptyColor),
                                         .label = Tr("Fond d'une case vide"),
                                         .kind = OptionKind::kColor,
                                         .color = look.empty_color});
  shortcut_rows_.push_back(
      OptionRowView{.id = ShortcutBarRow(bar, ShortcutBarField::kSkillColor),
                    .label = Tr("Fond d'un skill"),
                    .help = Tr("Une icône le recouvre : c'est sa transparence qui se voit, et "
                               "c'est elle qui permet de teinter une barre par famille."),
                    .kind = OptionKind::kColor,
                    .color = look.skill_color});
  shortcut_rows_.push_back(OptionRowView{.id = ShortcutBarRow(bar, ShortcutBarField::kItemColor),
                                         .label = Tr("Fond d'un item"),
                                         .kind = OptionKind::kColor,
                                         .color = look.item_color});
  shortcut_rows_.push_back(OptionRowView{.id = ShortcutBarRow(bar, ShortcutBarField::kBorderColor),
                                         .label = Tr("Couleur du cadre"),
                                         .kind = OptionKind::kColor,
                                         .color = look.border_color});
  shortcut_rows_.push_back(OptionRowView{.id = ShortcutBarRow(bar, ShortcutBarField::kKeyColor),
                                         .label = Tr("Couleur du texte des touches"),
                                         .kind = OptionKind::kColor,
                                         .color = look.key_color});
  shortcut_rows_.push_back(OptionRowView{.id = ShortcutBarRow(bar, ShortcutBarField::kCountColor),
                                         .label = Tr("Couleur du texte des nombres"),
                                         .kind = OptionKind::kColor,
                                         .color = look.count_color});
  shortcut_rows_.push_back(
      OptionRowView{.id = ShortcutBarRow(bar, ShortcutBarField::kCountdownColor),
                    .label = Tr("Couleur du temps restant"),
                    .help = Tr("Le seul texte d'une case qui dise « pas encore » : le confondre "
                               "avec les autres lui ôterait ce qu'il a d'alerte."),
                    .kind = OptionKind::kColor,
                    .color = look.countdown_color});
  shortcut_rows_.push_back(OptionRowView{.id = ShortcutBarRow(bar, ShortcutBarField::kShadowColor),
                                         .label = Tr("Couleur de l'ombre"),
                                         .kind = OptionKind::kColor,
                                         .color = look.shadow_color});
}

// CE QU'UNE LIGNE DE BARRE DE RACCOURCIS CHANGE.
//
// Rend faux quand l'identifiant n'est pas l'un des siens : l'aiguillage
// ordinaire reprend alors la main.
bool GameOptions::ChangeShortcutBar(const ro::ui::OptionsWindowAction& asked) {
  if (asked.changed == kShortcutLockedRow) {
    shortcut_bars_.locked = asked.on;
    return true;
  }
  if (asked.changed == kShortcutShowKeysRow) {
    shortcut_bars_.show_keys = asked.on;
    return true;
  }
  if (asked.changed < kShortcutBarRowBase) {
    return false;
  }
  const int offset = asked.changed - kShortcutBarRowBase;
  const auto bar = static_cast<std::size_t>(offset / kShortcutFieldsPerBar);
  if (bar >= ro::ui::kShortcutBarCount) {
    return false;
  }
  ro::ui::ShortcutBarLayout& look = shortcut_bars_.bars[bar];
  // ⚠ LES BORNES SONT CELLES DE LA FENÊTRE, et le dessin les repose de son côté :
  // les réglages viennent aussi d'un fichier que le joueur peut écrire à la
  // main, et cette voie-ci ne passe pas par les curseurs.
  switch (static_cast<ShortcutBarField>(offset % kShortcutFieldsPerBar)) {
    case ShortcutBarField::kShown:
      look.shown = asked.on;
      break;
    case ShortcutBarField::kClickThrough:
      look.click_through = asked.on;
      break;
    case ShortcutBarField::kColumns:
      look.columns = static_cast<int>(asked.value);
      break;
    case ShortcutBarField::kShownSlots:
      look.shown_slots = static_cast<int>(asked.value);
      break;
    case ShortcutBarField::kSlotSide:
      look.slot_side_px = asked.value;
      break;
    case ShortcutBarField::kGap:
      look.gap_px = asked.value;
      break;
    case ShortcutBarField::kEmptyColor:
      look.empty_color = asked.color;
      break;
    case ShortcutBarField::kSkillColor:
      look.skill_color = asked.color;
      break;
    case ShortcutBarField::kItemColor:
      look.item_color = asked.color;
      break;
    case ShortcutBarField::kBorderColor:
      look.border_color = asked.color;
      break;
    case ShortcutBarField::kBorderThickness:
      look.border_thickness = asked.value;
      break;
    case ShortcutBarField::kKeyColor:
      look.key_color = asked.color;
      break;
    case ShortcutBarField::kKeyScale:
      look.key_scale = asked.value;
      break;
    case ShortcutBarField::kCountColor:
      look.count_color = asked.color;
      break;
    case ShortcutBarField::kCountScale:
      look.count_scale = asked.value;
      break;
    case ShortcutBarField::kCountdownColor:
      look.countdown_color = asked.color;
      break;
    case ShortcutBarField::kShadowColor:
      look.shadow_color = asked.color;
      break;
    case ShortcutBarField::kShadowKind:
      look.shadow_kind = asked.chosen;
      break;
    case ShortcutBarField::kShadowOffset:
      look.shadow_offset = asked.value;
      break;
    case ShortcutBarField::kCopyLook:
      // 🔴 L'APPARENCE SEULE, ET C'EST CE QUI REND LE GESTE SÛR. Recopier aussi
      // « affichée » et « cases montrées » allumerait huit barres d'un clic, ou
      // les éteindrait toutes ; ce que le joueur demande ici, c'est que ses
      // couleurs se ressemblent — pas que ses barres se confondent.
      for (ro::ui::ShortcutBarLayout& other : shortcut_bars_.bars) {
        if (&other == &look) {
          continue;
        }
        other.empty_color = look.empty_color;
        other.skill_color = look.skill_color;
        other.item_color = look.item_color;
        other.border_color = look.border_color;
        other.border_thickness = look.border_thickness;
        other.key_color = look.key_color;
        other.key_scale = look.key_scale;
        other.count_color = look.count_color;
        other.count_scale = look.count_scale;
        other.countdown_color = look.countdown_color;
        other.shadow_color = look.shadow_color;
        other.shadow_kind = look.shadow_kind;
        other.shadow_offset = look.shadow_offset;
        // ⚠ LA TAILLE DES CASES ET L'ESPACEMENT EN FONT PARTIE : ce sont des
        // faits d'apparence, et une barre deux fois plus grosse que sa voisine
        // ne se lit pas comme sa sœur. La POSITION, elle, reste à chacune.
        other.slot_side_px = look.slot_side_px;
        other.gap_px = look.gap_px;
      }
      break;
    case ShortcutBarField::kCount:
      // Jamais encodé : il ne compte les champs que pour la garde d'en-tête.
      break;
  }
  return true;
}

}  // namespace moonclient
