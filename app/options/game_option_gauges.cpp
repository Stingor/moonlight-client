// LES RANGÉES DES SEPT JAUGES LIBRES, un onglet chacune.
//
// ⚠ À PART DU CATALOGUE ET DES BARRES DE RACCOURCIS, et pour la même raison
// qu'elles : ceci est une BOUCLE, pas une liste de littéraux qu'on copie l'un
// sur l'autre. Les sept jauges portent les mêmes dix-huit réglages.
//
// 🔴 ET UN ONGLET CHACUNE, PARCE QUE L'APPARENCE EST DEVENUE GRANULAIRE. Dix-huit
// réglages fois sept font cent vingt-six lignes qui se ressemblent toutes :
// le joueur y chercherait « le fond de la jauge de magie » en faisant défiler,
// et en comptant. On ne compose donc que les lignes de l'onglet qu'il regarde —
// exactement le dispositif des huit barres de raccourcis, et il était déjà là.
//
// 🔴 CE QUI RESTE AU-DESSUS DES ONGLETS N'EST PAS « CE QUI SE RESSEMBLE », C'EST
// « CE QUI NE PEUT PAS DIFFÉRER » : le verrou, l'aimantation et le repère de
// pose décrivent le GESTE, et il n'y a qu'une souris.

#include <algorithm>
#include <cstddef>
#include <vector>

#include "ro/core/i18n.h"
#include "ro/ui/stat_bars.h"

#include "options/game_options.h"
#include "options/option_row_kinds.h"

namespace moonclient {
namespace {

using ro::core::Tr;
using ro::ui::OptionKind;
using ro::ui::OptionRowView;

// LE NOM D'UNE JAUGE, COURT : c'est un onglet, et un onglet large chasse ses
// voisins hors de la barre.
//
// ⚠ UN AIGUILLAGE DE LITTÉRAUX, et non une table : `Tr` veut voir sa chaîne à
// l'appel, et le pointeur qu'il rend meurt au changement de langue.
[[nodiscard]] const char* StatBarLabel(std::size_t bar) {
  switch (bar) {
    case 0:
      return Tr("Vie");
    case 1:
      return Tr("Magie");
    case 2:
      return Tr("Exp. base");
    case 3:
      return Tr("Exp. job");
    case 4:
      return Tr("Poids");
    case 5:
      return Tr("Bourse");
    default:
      return Tr("Cast");
  }
}

}  // namespace

// CE QUI VAUT POUR LES SEPT, puis les onglets.
void GameOptions::BuildStatBarShared() {
  const bool bars_off = !stat_bars_.shown;
  stat_bar_shared_rows_.clear();
  stat_bar_shared_rows_.push_back(
      Toggle(GameOption::kStatBarsShown, Tr("Montrer les jauges"),
             Tr("Sept jauges libres — vie, magie, les deux expériences, le poids, la "
                "bourse et l'incantation — que l'on pose où l'on veut sur l'écran."),
             nullptr, stat_bars_.shown));
  stat_bar_shared_rows_.push_back(
      Toggle(GameOption::kStatBarsLocked, Tr("Verrouiller"),
             Tr("Verrouillées, les jauges se figent et laissent passer les clics au jeu. "
                "Déverrouillées, on les déplace au glisser et on les retaille par un bord."),
             nullptr, stat_bars_.locked));
  stat_bar_shared_rows_.back().disabled = bars_off;
  stat_bar_shared_rows_.push_back(
      Toggle(GameOption::kStatBarsSticky, Tr("Aimanter entre elles"),
             Tr("Les bords d'une jauge qu'on déplace s'alignent sur ceux des autres. Deux "
                "jauges qui se touchent partent ensemble au CTRL."),
             nullptr, stat_bars_.sticky));
  stat_bar_shared_rows_.back().disabled = bars_off;
  stat_bar_shared_rows_.push_back(
      Toggle(GameOption::kStatBarsCenterMark, Tr("Repère de centre"),
             Tr("Une croix au milieu de chaque jauge déverrouillée, pour poser son CENTRE "
                "sur une ligne de la grille — l'aimant, lui, ne prend que les bords.\n\n"
                "Outil de pose : il disparaît dès qu'on reverrouille."),
             nullptr, stat_bars_.center_mark));
  stat_bar_shared_rows_.back().disabled = bars_off || stat_bars_.locked;
  stat_bar_shared_rows_.push_back(Toggle(
      GameOption::kStatBarsKeepMaxedExp, Tr("Ne pas masquer les barres d'EXP au niveau maximum"),
      Tr("Au niveau maximum, la jauge d'EXP de base — ou celle du métier au niveau "
         "de métier maximum — s'efface d'elle-même. Cochée, elle reste."),
      nullptr, stat_bars_.keep_maxed_exp));
  stat_bar_shared_rows_.back().disabled = bars_off;
  stat_bar_shared_rows_.push_back(
      Toggle(GameOption::kStatBarsTuning, Tr("Mode réglage"),
             Tr("Les sept jauges se mettent à varier, chacune à son rythme, du vide au plein : "
                "on voit en direct ce qu'un réglage change sur une jauge qui bouge. La jauge "
                "« Cast » rejoue une incantation, le poids franchit le seuil de surcharge.\n\n"
                "Les valeurs sont factices, et ne s'affichent que tant que ce panneau est "
                "ouvert."),
             nullptr, stat_bars_.tuning));
  stat_bar_shared_rows_.back().disabled = bars_off;

  // LES ONGLETS. ⚠ REFAITS À CHAQUE IMAGE comme tout le reste : ces noms se
  // traduisent, et un nom mis en cache resterait dans la langue d'avant.
  stat_bar_tabs_.clear();
  stat_bar_tabs_.reserve(ro::ui::kStatBarCount);
  for (std::size_t bar = 0; bar < ro::ui::kStatBarCount; ++bar) {
    stat_bar_tabs_.push_back(StatBarLabel(bar));
  }
}

// LES DIX-HUIT RÉGLAGES D'UNE JAUGE.
//
// 🔴 CE QUI NE S'APPLIQUE PAS À LA FORME CHOISIE EST GRISÉ, JAMAIS RETIRÉ. Un
// réglage qui disparaît quand on change de forme fait sauter la mise en page, et
// le joueur cherche ce qu'il vient de perdre. Grisé, il dit « pas pour cette
// forme-là », et OÙ il est.
void GameOptions::BuildStatBarRows(int shown_bar) {
  BuildStatBarShared();

  const bool bars_off = !stat_bars_.shown;
  stat_bar_rows_.clear();
  const auto bar = static_cast<std::size_t>(
      std::clamp(shown_bar, 0, static_cast<int>(ro::ui::kStatBarCount) - 1));
  const ro::ui::StatBarLook& look = stat_bars_.bars[bar];

  // 🔴 NI UN `static`, NI UN LOCAL. Une rangée ne COPIE pas ses choix, elle en
  // garde un `span` : un `static` les capture UNE fois et le catalogue qui les
  // porte est remplacé au premier changement de langue ; un tableau local meurt
  // avant le dessin. Les deux mènent au même écran noir — c'est un plantage qui
  // l'a dit.
  stat_bar_text_names_.clear();
  stat_bar_text_names_.push_back(Tr("Rien"));
  stat_bar_text_names_.push_back(Tr("Pour cent"));
  stat_bar_text_names_.push_back(Tr("Valeurs"));
  stat_bar_text_names_.push_back(Tr("Les deux"));
  stat_bar_text_names_.push_back(Tr("Les deux, sur deux lignes"));
  stat_bar_shape_names_.clear();
  stat_bar_shape_names_.push_back(Tr("Barre"));
  stat_bar_shape_names_.push_back(Tr("Orbe"));
  stat_bar_shape_names_.push_back(Tr("Anneau"));

  const bool off = bars_off || !look.shown;
  const bool is_bar = look.shape == ro::ui::StatBarShape::kBar;
  const bool is_ring = look.shape == ro::ui::StatBarShape::kRing;
  const bool is_orb = look.shape == ro::ui::StatBarShape::kOrb;
  const bool mute = look.text == ro::ui::StatBarText::kNothing;
  const bool is_cast = bar == static_cast<std::size_t>(ro::ui::StatBar::kCast);

  const auto push = [this](OptionRowView row) { stat_bar_rows_.push_back(row); };

  // ⚠ L'INCANTATION DIT QUAND ELLE PARAÎT : décochée ou pas, elle est absente
  // de l'écran hors incantation, et le joueur qui vient de l'allumer la cherche.
  push(OptionRowView{.id = StatBarRow(bar, StatBarField::kShown),
                     .label = Tr("Affichée"),
                     .help = is_cast ? Tr("Elle ne paraît que pendant une incantation : son "
                                          "sort et le temps restant, là où on l'a posée.")
                                     : nullptr,
                     .kind = OptionKind::kToggle,
                     .on = look.shown,
                     .disabled = bars_off});
  push(OptionRowView{.id = StatBarRow(bar, StatBarField::kShape),
                     .label = Tr("Forme"),
                     .help = Tr("Une barre, un orbe à la manière de Diablo, ou un anneau.\n\n"
                                "Les réglages qui ne valent pas pour la forme choisie se "
                                "grisent."),
                     .kind = OptionKind::kChoice,
                     .chosen = static_cast<int>(look.shape),
                     .choices = stat_bar_shape_names_,
                     .heading = Tr("Forme"),
                     .disabled = off});
  push(OptionRowView{
      .id = StatBarRow(bar, StatBarField::kReversed),
      .label = Tr("Sens inversé"),
      .help = Tr("La barre se remplit depuis la droite (ou depuis le haut), l'anneau "
                 "tourne à l'envers. Pour une disposition en miroir.\n\nUn orbe l'ignore : "
                 "il se remplit toujours par le bas."),
      .kind = OptionKind::kToggle,
      .on = look.reversed,
      .indented = true,
      .disabled = off || is_orb});
  push(OptionRowView{.id = StatBarRow(bar, StatBarField::kVertical),
                     .label = Tr("Remplissage vertical"),
                     .help = Tr("La barre se remplit de bas en haut au lieu d'aller vers la "
                                "droite. Ne vaut que pour une barre."),
                     .kind = OptionKind::kToggle,
                     .on = look.vertical,
                     .indented = true,
                     .disabled = off || !is_bar});
  push(OptionRowView{.id = StatBarRow(bar, StatBarField::kRounding),
                     .label = Tr("Arrondi"),
                     .help = Tr("L'arrondi des coins. Ne vaut que pour une barre : un disque "
                                "n'a pas de coin."),
                     .kind = OptionKind::kSlider,
                     .value = look.rounding,
                     .lowest = 0.0F,
                     .highest = 16.0F,
                     .format = "%.0f px",
                     .indented = true,
                     .disabled = off || !is_bar});
  push(OptionRowView{.id = StatBarRow(bar, StatBarField::kRingThickness),
                     .label = Tr("Épaisseur de l'anneau"),
                     .help = Tr("En part du rayon. Au-delà de la moitié il n'y a plus de "
                                "trou, et l'anneau n'est qu'un camembert."),
                     .kind = OptionKind::kSlider,
                     .value = look.ring_thickness,
                     .lowest = ro::ui::kThinnestRing,
                     .highest = ro::ui::kThickestRing,
                     .format = "%.2f",
                     .indented = true,
                     .disabled = off || !is_ring});

  push(OptionRowView{.id = StatBarRow(bar, StatBarField::kFill),
                     .label = Tr("Remplissage"),
                     .kind = OptionKind::kColor,
                     .color = look.fill,
                     .heading = Tr("Couleurs"),
                     .disabled = off});
  push(OptionRowView{.id = StatBarRow(bar, StatBarField::kBackground),
                     .label = Tr("Fond"),
                     .help = Tr("Transparence comprise. C'est lui qui montre ce qui MANQUE."),
                     .kind = OptionKind::kColor,
                     .color = look.background,
                     .disabled = off});
  // ⚠ COMPOSÉES À LA MAIN ET NON PAR `Toggle` : ce raccourci prend un
  // `GameOption`, or l'identifiant d'une ligne de jauge n'en est pas un — il est
  // encodé au-dessus de l'énumération. L'y faire entrer par un cast donnerait
  // une valeur qui ne nomme rien.
  push(OptionRowView{
      .id = StatBarRow(bar, StatBarField::kGlossy),
      .label = Tr("Relief"),
      .help = Tr("Un liquide plus clair à sa surface, un trait de niveau, un bord qui "
                 "s'assombrit et un reflet sur un orbe ; un dégradé et un fil de lumière sur "
                 "une barre ; un biseau sur un anneau.\n\nSans lui, un orbe se lit comme une "
                 "pastille plate : c'est le relief, pas la rondeur, qui le fait reconnaître."),
      .kind = OptionKind::kToggle,
      .on = look.glossy,
      .disabled = off});
  push(OptionRowView{.id = StatBarRow(bar, StatBarField::kBorder),
                     .label = Tr("Filet"),
                     .help = Tr("Un liseré sombre d'un pixel qui suit la forme."),
                     .kind = OptionKind::kToggle,
                     .on = look.border,
                     .disabled = off});
  push(OptionRowView{.id = StatBarRow(bar, StatBarField::kBorderColor),
                     .label = Tr("Couleur du filet"),
                     .kind = OptionKind::kColor,
                     .color = look.border_color,
                     .indented = true,
                     .disabled = off || !look.border});

  push(OptionRowView{.id = StatBarRow(bar, StatBarField::kText),
                     .label = Tr("Texte"),
                     .help = Tr("Ce que cette jauge écrit.\n\nLa bourse fait exception : elle "
                                "donne toujours son montant, une fraction n'ayant aucun sens "
                                "pour elle. L'incantation aussi : elle écrit le sort et le "
                                "temps restant."),
                     .kind = OptionKind::kChoice,
                     .chosen = static_cast<int>(look.text),
                     .choices = stat_bar_text_names_,
                     .heading = Tr("Texte"),
                     .disabled = off});
  push(OptionRowView{
      .id = StatBarRow(bar, StatBarField::kLabel),
      .label = Tr("Nom court devant le texte"),
      .help = Tr("« HP 45 % » plutôt que « 45 % ». La bourse n'en a pas : son « z » dit déjà "
                 "ce qu'elle compte. L'incantation non plus : elle écrit le nom du sort."),
      .kind = OptionKind::kToggle,
      .on = look.label,
      .indented = true,
      .disabled =
          off || mute || is_cast || bar == static_cast<std::size_t>(ro::ui::StatBar::kZeny)});
  push(OptionRowView{.id = StatBarRow(bar, StatBarField::kFontPixels),
                     .label = Tr("Taille du texte"),
                     .kind = OptionKind::kSlider,
                     .value = look.font_pixels,
                     .lowest = 8.0F,
                     .highest = 32.0F,
                     .format = "%.0f px",
                     .indented = true,
                     .disabled = off || mute});
  push(OptionRowView{.id = StatBarRow(bar, StatBarField::kTextColor),
                     .label = Tr("Couleur du texte"),
                     .kind = OptionKind::kColor,
                     .color = look.text_color,
                     .indented = true,
                     .disabled = off || mute});
  // ⚠ EN TAILLES DE JAUGE, ET NON EN PIXELS : le texte suit la jauge qu'on
  // retaille. Voir `StatBarLook::text_shift_x`.
  push(OptionRowView{.id = StatBarRow(bar, StatBarField::kTextShiftX),
                     .label = Tr("Décalage du texte, horizontal"),
                     .help = Tr("En largeurs de la jauge : zéro laisse le texte au centre, 1 le "
                                "pousse d'une largeur entière vers la droite, -1 vers la "
                                "gauche."),
                     .kind = OptionKind::kSlider,
                     .value = look.text_shift_x,
                     .lowest = -ro::ui::kLargestTextShiftShare,
                     .highest = ro::ui::kLargestTextShiftShare,
                     .format = "%+.2f",
                     .indented = true,
                     .disabled = off || mute});
  push(OptionRowView{.id = StatBarRow(bar, StatBarField::kTextShiftY),
                     .label = Tr("Décalage du texte, vertical"),
                     .help = Tr("En hauteurs de la jauge : zéro laisse le texte au centre, 1 le "
                                "pousse d'une hauteur entière vers le bas, -1 vers le haut."),
                     .kind = OptionKind::kSlider,
                     .value = look.text_shift_y,
                     .lowest = -ro::ui::kLargestTextShiftShare,
                     .highest = ro::ui::kLargestTextShiftShare,
                     .format = "%+.2f",
                     .indented = true,
                     .disabled = off || mute});

  // ── CE QUI N'APPARTIENT QU'À UNE JAUGE ────────────────────────────────────
  //
  // 🔴 LEUR PLACE DIT À QUI ILS S'APPLIQUENT. Le seuil de surcharge ne veut rien
  // dire pour la vie, et le repère de bourse ne veut rien dire pour le poids :
  // posés en commun, ils auraient demandé au joueur de deviner laquelle des six
  // ils concernent.
  if (bar == static_cast<std::size_t>(ro::ui::StatBar::kWeight)) {
    stat_bar_rows_.push_back(
        Toggle(GameOption::kStatBarsWarnOverweight, Tr("Alerter à la surcharge"),
               Tr("La jauge rougit au-delà du seuil que le SERVEUR annonce — celui où la "
                  "régénération naturelle s'arrête.\n\nSans cette alerte il faut ouvrir "
                  "l'inventory pour savoir où le jeu bascule, ce que la jauge existe pour éviter."),
               nullptr, stat_bars_.warn_overweight));
    stat_bar_rows_.back().heading = Tr("Le poids seul");
    stat_bar_rows_.back().disabled = off;
  }
  if (bar == static_cast<std::size_t>(ro::ui::StatBar::kZeny)) {
    stat_bar_rows_.push_back(
        Measure(GameOption::kStatBarsZenyReference, Tr("Bourse pleine à"),
                Tr("Ce que « bourse pleine » veut dire.\n\nUne bourse n'a pas de maximum : "
                   "sans ce repère, sa jauge se remplirait contre le plafond du serveur — "
                   "deux milliards — et ne bougerait jamais.\n\nLe montant écrit, lui, reste "
                   "le vrai."),
                static_cast<float>(stat_bars_.zeny_reference),
                static_cast<float>(ro::ui::kSmallestZenyReference),
                static_cast<float>(ro::ui::kLargestZenyReference), "%.0f z"));
    stat_bar_rows_.back().heading = Tr("La bourse seule");
    stat_bar_rows_.back().disabled = off;
  }

  push(OptionRowView{
      .id = StatBarRow(bar, StatBarField::kCopyLook),
      .label = Tr("Donner ce style aux six autres"),
      .help = Tr("Le STYLE seulement : le texte, le fond, le filet, le relief, l'arrondi.\n\n"
                 "La place, la forme, la couleur et le sens restent à chacune — sans quoi ce "
                 "bouton ferait sept jauges identiques, et il faudrait lire leur chiffre pour "
                 "savoir laquelle est laquelle."),
      .kind = OptionKind::kAction,
      .disabled = bars_off});
}

// CE QU'UNE LIGNE DE JAUGE CHANGE.
//
// ⚠ MÊME MÉCANIQUE QUE LES BARRES DE RACCOURCIS, et elle rejette les leurs :
// leurs identifiants sont sous `kStatBarRowBase`, donc l'ordre d'appel des deux
// n'a pas d'importance.
bool GameOptions::ChangeStatBar(const ro::ui::OptionsWindowAction& asked) {
  if (asked.changed < kStatBarRowBase) {
    return false;
  }
  const int offset = asked.changed - kStatBarRowBase;
  const auto bar = static_cast<std::size_t>(offset / kStatBarFieldsPerBar);
  if (bar >= ro::ui::kStatBarCount) {
    return false;
  }
  ro::ui::StatBarLook& look = stat_bars_.bars[bar];
  switch (static_cast<StatBarField>(offset % kStatBarFieldsPerBar)) {
    case StatBarField::kShown:
      look.shown = asked.on;
      break;
    case StatBarField::kShape:
      look.shape = ro::ui::StatBarShapeOf(asked.chosen);
      break;
    case StatBarField::kReversed:
      look.reversed = asked.on;
      break;
    case StatBarField::kVertical:
      look.vertical = asked.on;
      break;
    case StatBarField::kRounding:
      look.rounding = asked.value;
      break;
    case StatBarField::kRingThickness:
      look.ring_thickness = asked.value;
      break;
    case StatBarField::kFill:
      look.fill = asked.color;
      break;
    case StatBarField::kBackground:
      look.background = asked.color;
      break;
    case StatBarField::kGlossy:
      look.glossy = asked.on;
      break;
    case StatBarField::kBorder:
      look.border = asked.on;
      break;
    case StatBarField::kBorderColor:
      look.border_color = asked.color;
      break;
    case StatBarField::kText:
      look.text = ro::ui::StatBarTextOf(asked.chosen);
      break;
    case StatBarField::kLabel:
      look.label = asked.on;
      break;
    case StatBarField::kFontPixels:
      look.font_pixels = asked.value;
      break;
    case StatBarField::kTextColor:
      look.text_color = asked.color;
      break;
    case StatBarField::kTextShiftX:
      look.text_shift_x = asked.value;
      break;
    case StatBarField::kTextShiftY:
      look.text_shift_y = asked.value;
      break;
    case StatBarField::kCopyLook:
      // ⚠ LE STYLE SEUL. Voir `CopyStatBarStyle`, qui dit ce qui part et ce qui
      // reste — et pourquoi la forme et la couleur ne partent pas.
      ro::ui::CopyStatBarStyle(stat_bars_.bars, bar);
      break;
    case StatBarField::kCount:
      // Jamais encodé : il ne compte les champs que pour la garde d'en-tête.
      break;
  }
  return true;
}

}  // namespace moonclient
