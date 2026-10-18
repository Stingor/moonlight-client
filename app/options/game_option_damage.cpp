// LES RANGÉES DES DÉGÂTS FLOTTANTS, et ce qu'une de ces rangées change.
//
// ⚠ À PART DU CATALOGUE, pour le plafond — il touche déjà les sept cents lignes
// — et parce que la section a une forme que les autres n'ont pas : trois
// ONGLETS pour un seul objet. Trente réglages en colonne ne se lisent pas ;
// trois pages de dix, sous « ce qui se montre », « l'allure » et « les
// couleurs », se parcourent.
//
// 🔴 CHAQUE DÉFAUT EST LE CLIENT, et le bouton qui y ramène ne recopie rien : il
// pose `HitPopupLook{}`, dont les initialisateurs sont la seule source. Voir
// `hit_popup_look.h`.
//
// 🔴 LES LIGNES SE REFONT À CHAQUE IMAGE, panneau ouvert : c'est ce qui fait
// qu'un changement de langue se voit à l'image suivante.

#include <algorithm>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ro/core/i18n.h"
#include "ro/render/number_look.h"
#include "ro/ui/system_fonts.h"

#include "options/damage_page_names.h"
#include "options/damage_path_anchors.h"
#include "options/damage_path_rows.h"
#include "options/game_options.h"
#include "options/option_row_kinds.h"
#include "world/hit_popup_look.h"

namespace moonclient {
namespace {

using ro::core::Tr;
using ro::ui::OptionKind;
using ro::ui::OptionRowView;

// ⚠ `kPercent` VIENT DE `option_row_kinds.h` : les facteurs se gardent de zéro à
// un — l'unité de la composition — et se montrent en pourcentage, et cette
// conversion-là est la même partout. Trois copies vivaient ici, dans les rangées
// et dans le sélecteur ; la première touchée aurait divergé sans un mot.

// LES ONGLETS ET LES LISTES DÉROULANTES, REFAITS À CHAQUE IMAGE.
//
// 🔴 ET SURTOUT PAS DANS UN `static` : le pointeur que `Tr` rend appartient au
// catalogue et MEURT au changement de langue. ⚠ L'ORDRE DU PARTAGE ET DES
// ALLURES EST CELUI DES ÉNUMÉRATIONS DE `hit_popup_look.h`, qui est celui du
// fichier de réglages : un libellé intercalé décalerait ce qu'un joueur a déjà
// choisi. Les polices, elles, s'écrivent par leur NOM.
void FillDamageChoices(DamagePageNames& names) {
  names.tabs.clear();
  names.tabs.push_back(Tr("Ce qui se montre"));
  names.tabs.push_back(Tr("Allure"));
  names.tabs.push_back(Tr("Couleurs"));

  names.splits.clear();
  names.splits.push_back(Tr("Coup par coup, en cumulant"));
  names.splits.push_back(Tr("Coup par coup, chaque part"));
  names.splits.push_back(Tr("Le total, une fois"));
  names.splits.push_back(Tr("Chaque part, plus le total"));

  names.paths.clear();
  names.paths.push_back(Tr("Coup ordinaire sur un monstre"));
  names.paths.push_back(Tr("Skill sur un monstre"));
  names.paths.push_back(Tr("Sur un joueur"));
  names.paths.push_back(Tr("Critique"));
  names.paths.push_back(Tr("Le total, à côté des parts"));
  names.paths.push_back(Tr("Soin"));
  names.paths.push_back(Tr("Les mots « Miss » et « LUCKY!! »"));

  // ⚠ LES FAMILLES EN LITTÉRAL MARQUÉ, comme dans la liste de l'interface : la
  // liste déroulante traduit elle-même ce qu'on lui donne.
  names.fonts.clear();
  names.fonts.push_back(Tr("Celle de l'interface"));
  for (const int family : ro::ui::InstalledFontFamilies()) {
    names.fonts.push_back(
        ro::ui::SystemFontFamilies()[static_cast<std::size_t>(family)].label.data());
  }
  for (const std::string& loose : names.loose_fonts) {
    names.fonts.push_back(loose.c_str());
  }
}

// LE NOM DE POLICE AU RANG `chosen` DE LA LISTE, ou vide pour celle de
// l'interface — l'ordre de `DamagePageNames::fonts`.
[[nodiscard]] std::string DamageFontNameAt(int chosen, const DamagePageNames& names) {
  if (chosen <= 0) {
    return {};
  }
  const std::span<const int> installed = ro::ui::InstalledFontFamilies();
  auto rank = static_cast<std::size_t>(chosen - 1);
  if (rank < installed.size()) {
    return std::string(
        ro::ui::SystemFontFamilies()[static_cast<std::size_t>(installed[rank])].label);
  }
  rank -= installed.size();
  return rank < names.loose_fonts.size() ? names.loose_fonts[rank] : std::string();
}

// Et le rang d'un nom, zéro — celle de l'interface — quand la liste ne le porte
// plus : une police retirée du dossier se dessine alors comme celle-là.
[[nodiscard]] int DamageFontRankOf(std::string_view name, const DamagePageNames& names) {
  if (name.empty()) {
    return 0;
  }
  const std::span<const int> installed = ro::ui::InstalledFontFamilies();
  for (std::size_t rank = 0; rank < installed.size(); ++rank) {
    if (ro::ui::SystemFontFamilies()[static_cast<std::size_t>(installed[rank])].label == name) {
      return static_cast<int>(rank) + 1;
    }
  }
  for (std::size_t rank = 0; rank < names.loose_fonts.size(); ++rank) {
    if (names.loose_fonts[rank] == name) {
      return static_cast<int>(installed.size() + rank) + 1;
    }
  }
  return 0;
}

// Vrai pour une police du dossier du jeu, qui n'a qu'un dessin.
[[nodiscard]] bool IsLooseFont(std::string_view name, const DamagePageNames& names) {
  return std::ranges::find(names.loose_fonts, name) != names.loose_fonts.end();
}

// Un facteur montré en pour cent.
[[nodiscard]] OptionRowView Factor(GameOption which, const char* label, const char* help,
                                   float value, float lowest, float highest) {
  return OptionRowView{.id = static_cast<int>(which),
                       .label = label,
                       .help = help,
                       .kind = OptionKind::kSlider,
                       .value = value * kPercent,
                       .lowest = lowest * kPercent,
                       .highest = highest * kPercent,
                       .format = "%.0f %%"};
}

// ── CE QUI SE MONTRE ──────────────────────────────────────────────────────────
void ShownRows(const HitPopupLook& look, std::span<const char* const> splits,
               std::vector<OptionRowView>& rows) {
  const bool off = !look.shown;
  rows.push_back(Toggle(GameOption::kDamageOwnDealt, Tr("Mes coups"),
                        Tr("Ce que j'inflige — à un monstre comme à un joueur."), nullptr,
                        look.own_dealt));
  rows.back().heading = Tr("Les coups");
  rows.back().disabled = off;
  rows.push_back(Toggle(GameOption::kDamageOwnTaken, Tr("Les coups que je reçois"),
                        Tr("Ce qu'on m'inflige — et ce que je m'inflige."), nullptr,
                        look.own_taken));
  rows.back().disabled = off;
  rows.push_back(
      Toggle(GameOption::kDamageOthers, Tr("Les coups des autres"),
             Tr("Ceux qui ne me concernent pas : un groupe qui chasse, un duel qu'on regarde. "
                "Les taire laisse lire les siens dans la mêlée."),
             nullptr, look.others));
  rows.back().disabled = off;

  rows.push_back(Toggle(GameOption::kDamageMiss, Tr("Le mot « Miss »"),
                        Tr("Ce qu'un coup qui n'entame pas écrit à la place d'un nombre. Il "
                           "paraît sur CELUI QUI A RATÉ, et non sur sa cible."),
                        nullptr, look.miss));
  rows.back().heading = Tr("Les mots");
  rows.back().disabled = off;
  rows.push_back(Toggle(GameOption::kDamageLucky, Tr("La bulle « LUCKY!! »"),
                        Tr("Une esquive chanceuse. Elle ne montre ni nombre ni « Miss » : c'est "
                           "la bulle, ou rien."),
                        nullptr, look.lucky));
  rows.back().disabled = off;
  rows.push_back(Toggle(GameOption::kDamageCriticalStar, Tr("L'étoile d'un critique"),
                        Tr("L'impact peint derrière le nombre d'un coup critique. Sans elle, le "
                           "nombre garde sa couleur et son allure de critique."),
                        nullptr, look.critical_star));
  rows.back().disabled = off;

  rows.push_back(Toggle(GameOption::kDamageHeals, Tr("Les soins"),
                        Tr("Le soin et la magie rendue, quand un skill les lance. Les centaines "
                           "d'autres skills sans dégâts ne montrent que leur effet."),
                        nullptr, look.heals));
  rows.back().heading = Tr("Le reste");
  rows.back().disabled = off;
  rows.push_back(
      Toggle(GameOption::kDamageEffectNumbers, Tr("Les nombres des effets"),
             Tr("La vie et la magie rendues au fil du temps, et les chiffres colorés que les "
                "scripts du serveur font paraître. L'effet lui-même se joue quoi qu'il "
                "arrive."),
             nullptr, look.effect_numbers));
  rows.back().disabled = off;

  rows.push_back(
      Choice(GameOption::kDamageSplit, Tr("Coups multiples"),
             Tr("Le serveur n'annonce qu'un total et le nombre de coups qui l'ont fait. Le "
                "client fait naître un nombre par coup, chacun ne portant que sa part — et, "
                "sur un monstre, fait monter à côté le total des frappes déjà portées, en "
                "jaune. Un sort de zone n'en annonce qu'une : son total vaut alors la "
                "part, et les deux nombres se répètent."),
             splits, look.split));
  rows.back().heading = Tr("Les coups multiples");
  rows.back().disabled = off;
  rows.push_back(
      Measure(GameOption::kDamageHitInterval, Tr("Intervalle entre deux coups"),
              Tr("Le client attend deux cents millisecondes entre deux nombres du même coup."),
              look.hit_interval_ms, kShortestHitIntervalMs, kLongestHitIntervalMs, "%.0f ms"));
  rows.back().indented = true;
  rows.back().disabled = off || HitSplitOf(look.split) == HitSplit::kTotalOnce;

  const bool without_total = HitSplitOf(look.split) != HitSplit::kPartsAndTotal;
  rows.push_back(
      Toggle(GameOption::kDamageTotalCarries, Tr("Un vrai cumul, sur la cible"),
             Tr("Le client repart de zéro à chaque annonce : son total ne réunit que les "
                "frappes d'un même coup. Allumé, le nombre à côté des parts continue "
                "d'additionner ce que la même cible encaisse, tant que les coups "
                "s'enchaînent — ce qu'un sort de zone ne dit autrement jamais."),
             nullptr, look.total_carries));
  rows.back().disabled = off || without_total;
  rows.push_back(Measure(GameOption::kDamageTotalForget, Tr("Oubli du cumul"),
                         Tr("Le temps sans un coup au bout duquel la cible est oubliée et "
                            "la somme repart de zéro."),
                         look.total_forget_ms, kShortestTotalForgetMs, kLongestTotalForgetMs,
                         "%.0f ms"));
  rows.back().indented = true;
  rows.back().disabled = off || without_total || !look.total_carries;
}

// ── L'ALLURE ──────────────────────────────────────────────────────────────────
void MotionRows(const HitPopupLook& look, const DamagePageNames& names,
                std::vector<OptionRowView>& rows) {
  const bool off = !look.shown;
  const ro::render::NumberLook& motion = look.motion;
  rows.push_back(Toggle(GameOption::kDamageFontText, Tr("Écrire avec une police"),
                        Tr("Les nombres s'écrivent avec une police au lieu des chiffres dessinés "
                           "du client. Même place, même taille, mêmes couleurs, mêmes "
                           "mouvements. Les mots « Miss » et « LUCKY!! », et l'étoile d'un "
                           "critique, gardent leur image."),
                        nullptr, look.font_text));
  rows.back().heading = Tr("L'écriture");
  rows.back().disabled = off;
  rows.push_back(Choice(GameOption::kDamageFontName, Tr("Police"),
                        Tr("Celle de l'interface suit la police choisie pour les fenêtres ; "
                           "viennent ensuite celles de Windows, puis celles déposées dans le "
                           "dossier « fonts » du jeu — fichiers .ttf ou .otf, lus au lancement "
                           "du client. Une police qui ne dessine pas de chiffres laisse la place "
                           "à celle de l'interface."),
                        names.fonts, DamageFontRankOf(look.font_name, names)));
  rows.back().indented = true;
  rows.back().disabled = off || !look.font_text;
  rows.push_back(Toggle(GameOption::kDamageFontBold, Tr("En gras"),
                        Tr("Pour une police qui en livre un. Celles du dossier du jeu n'ont qu'un "
                           "dessin."),
                        nullptr, look.font_bold));
  rows.back().indented = true;
  rows.back().disabled = off || !look.font_text || IsLooseFont(look.font_name, names);
  rows.push_back(Measure(GameOption::kDamageFontOutline, Tr("Épaisseur du contour"),
                         Tr("Le contour noir de chaque chiffre, en pixels à la taille normale : "
                            "il grossit avec le nombre. À zéro, pas de contour."),
                         look.font_outline_pixels, 0.0F, kLargestFontOutlinePixels, "%.1f px"));
  rows.back().indented = true;
  rows.back().disabled = off || !look.font_text;

  rows.push_back(Factor(GameOption::kDamageScale, Tr("Taille"),
                        Tr("En facteur sur la taille que la vie de chaque type calcule à chaque "
                           "instant : un nombre qui grossit grossit encore, et de la même façon."),
                        motion.scale, ro::render::kSmallestNumberScale,
                        ro::render::kLargestNumberScale));
  rows.back().heading = Tr("Tous les nombres");
  rows.back().disabled = off;
  rows.push_back(Measure(GameOption::kDamageDigitPitch, Tr("Écart entre les chiffres"),
                         Tr("D'un centre au suivant, en pixels avant échelle. Le client en met "
                            "huit."),
                         motion.digit_pitch_pixels, ro::render::kSmallestDigitPitch,
                         ro::render::kLargestDigitPitch, "%.0f px"));
  rows.back().disabled = off;
  rows.push_back(Factor(GameOption::kDamageOpacity, Tr("Opacité"), nullptr, motion.opacity,
                        ro::render::kFaintestNumberOpacity, 1.0F));
  rows.back().disabled = off;
  rows.push_back(
      Toggle(GameOption::kDamageAbbreviate, Tr("Abréger en K et M"),
             Tr("D'un million à cent millions, le montant en milliers suivi de K ; au-delà, en "
                "millions suivi de M — le « Damage Display: Expand » du client. Sans lui, tout "
                "se borne à 999 999."),
             nullptr, motion.abbreviate));
  rows.back().disabled = off;
}

// ── LES COULEURS ──────────────────────────────────────────────────────────────
void ColorRows(const HitPopupLook& look, std::vector<OptionRowView>& rows) {
  const bool off = !look.shown;
  rows.push_back(
      Swatch(GameOption::kDamageColorOwnDealt, Tr("Mes coups"),
             Tr("Ce que j'inflige à un monstre. Sur un joueur, c'est la couleur des joueurs "
                "qui prime — comme au client, où la cible décide.\n\nLa même que « sur un "
                "monstre » par défaut : les distinguer fait ressortir les siens dans un "
                "groupe.\n\nTransparence comprise, pour toutes : elle voile cette famille de "
                "nombres, par-dessus l'opacité de l'onglet d'à côté."),
             look.color_own_dealt));
  rows.back().heading = Tr("Les coups");
  rows.back().disabled = off;
  rows.push_back(Swatch(GameOption::kDamageColorMonster, Tr("Sur un monstre"),
                        Tr("Les coups des autres sur un monstre ou un PNJ. Blanc chez le client."),
                        look.color_on_monster));
  rows.back().disabled = off;
  rows.push_back(Swatch(GameOption::kDamageColorPlayer, Tr("Sur un joueur"),
                        Tr("Tout coup porté à un autre joueur. Rouge chez le client."),
                        look.color_on_player));
  rows.back().disabled = off;
  rows.push_back(Swatch(GameOption::kDamageColorSelf, Tr("Sur moi"),
                        Tr("Tout coup que je reçois. Rouge chez le client, comme sur les autres "
                           "joueurs."),
                        look.color_on_self));
  rows.back().disabled = off;

  rows.push_back(Swatch(GameOption::kDamageColorCritical, Tr("Critique"),
                        Tr("L'emporte sur la cible : jaune chez le client, même sur un monstre."),
                        look.color_critical));
  rows.back().heading = Tr("Ce qui l'emporte");
  rows.back().disabled = off;
  rows.push_back(Swatch(GameOption::kDamageColorSp, Tr("Dégâts de magie"),
                        Tr("Quand ce sont les points de magie qu'on entame. L'emporte même sur "
                           "le critique."),
                        look.color_sp));
  rows.back().disabled = off;
  rows.push_back(Swatch(GameOption::kDamageColorTotal, Tr("Le total, à côté des parts"),
                        Tr("Le second nombre qui monte à côté des parts, sur un monstre. Jaune "
                           "chez le client, comme un critique ; la magie l'emporte ici aussi."),
                        look.color_total));
  rows.back().disabled = off;

  rows.push_back(Swatch(GameOption::kDamageColorHeal, Tr("Soin"),
                        Tr("Vert chez le client. Vaut aussi pour la vie rendue au fil du temps."),
                        look.color_heal));
  rows.back().heading = Tr("Ce qui rend");
  rows.back().disabled = off;
  rows.push_back(Swatch(GameOption::kDamageColorSpRecovery, Tr("Magie rendue"),
                        Tr("Bleu chez le client. Vaut aussi pour la magie rendue au fil du temps."),
                        look.color_sp_recovery));
  rows.back().disabled = off;
}

}  // namespace

void GameOptions::SetDamageFonts(std::vector<std::string> loose) {
  damage_names_.loose_fonts = std::move(loose);
}

void GameOptions::ComposeDamageRows(int shown_tab) {
  FillDamageChoices(damage_names_);

  // CE QUI VAUT POUR LES TROIS ONGLETS, dessiné au-dessus d'eux : l'interrupteur
  // général, et le retour au client. ⚠ C'EST LA PLACE QUI LE DIT : posés sous
  // un onglet, ils se liraient comme des réglages de cet onglet-là.
  damage_shared_rows_.clear();
  damage_shared_rows_.push_back(
      Toggle(GameOption::kDamageShown, Tr("Montrer les dégâts"),
             Tr("Les nombres qui montent au-dessus de ce qui est frappé, soigné ou manqué. "
                "Éteint, rien ne se montre — et rien ne se rejoue en rallumant."),
             nullptr, damage_.shown));
  damage_shared_rows_.push_back(
      Toggle(GameOption::kDamagePreview, Tr("Aperçu en boucle"),
             Tr("Des coups factices joués sur le personnage tant que cette fenêtre est "
                "ouverte : un coup sur un monstre, un skill, un coup multiple, un "
                "critique, un coup reçu, un coup des autres, « Miss », « LUCKY!! », la "
                "magie, un soin, la magie rendue — puis à nouveau. Ils suivent tous les "
                "réglages de la section."),
             nullptr, damage_.preview));
  damage_shared_rows_.push_back(OptionRowView{
      .id = static_cast<int>(GameOption::kDamageClientLook),
      .label = Tr("Affichage du client"),
      .help = Tr("Rend à toute la section — ce qui se montre, l'allure, les couleurs — "
                 "l'affichage du client d'origine."),
      .kind = OptionKind::kAction,
      .confirm = Tr("Rendre aux dégâts l'affichage du client ? Tous les réglages de la "
                    "section repartent.")});

  damage_rows_.clear();
  pages_.damage_side_rows.clear();
  const int last_tab = static_cast<int>(DamageTab::kCount) - 1;
  switch (static_cast<DamageTab>(std::clamp(shown_tab, 0, last_tab))) {
    case DamageTab::kShown:
      ShownRows(damage_, damage_names_.splits, damage_rows_);
      break;
    case DamageTab::kMotion:
      // LE TYPE MONTRÉ D'ABORD — son chemin dans le volet, sa vie ici —, puis ce
      // qui vaut pour tous les nombres.
      PathPreviewRows(damage_, damage_names_, &pages_.damage_path_sample, true,
                      pages_.damage_side_rows);
      PathTypeRows(damage_, damage_rows_);
      MotionRows(damage_, damage_names_, damage_rows_);
      break;
    case DamageTab::kColors:
    case DamageTab::kCount:
      ColorRows(damage_, damage_rows_);
      // LES COULEURS SE RÈGLENT EN REGARDANT LES NOMBRES : le volet les montre
      // aussi, sans les ancres.
      PathPreviewRows(damage_, damage_names_, &pages_.damage_path_sample, false,
                      pages_.damage_side_rows);
      break;
  }
}

// CE QU'UNE LIGNE DE DÉGÂTS CHANGE.
//
// ⚠ RIEN À POUSSER ENSUITE : `HitPopups` relit le réglage à chaque image, et une
// copie qu'on lui enverrait serait une seconde source à tenir à jour. C'est
// l'aiguillage général qui appelle, puis enregistre.
void GameOptions::ChangeDamage(GameOption which, const ro::ui::OptionsWindowAction& asked) {
  HitPopupLook& look = damage_;
  // LA TRAJECTOIRE DU TYPE MONTRÉ, curseurs et ancres : voir `ChangeDamagePath`.
  if (ChangeDamagePath(look, which, asked)) {
    return;
  }
  switch (which) {
    case GameOption::kDamageShown:
      look.shown = asked.on;
      break;
    case GameOption::kDamageOwnDealt:
      look.own_dealt = asked.on;
      break;
    case GameOption::kDamageOwnTaken:
      look.own_taken = asked.on;
      break;
    case GameOption::kDamageOthers:
      look.others = asked.on;
      break;
    case GameOption::kDamageMiss:
      look.miss = asked.on;
      break;
    case GameOption::kDamageLucky:
      look.lucky = asked.on;
      break;
    case GameOption::kDamageCriticalStar:
      look.critical_star = asked.on;
      break;
    case GameOption::kDamageHeals:
      look.heals = asked.on;
      break;
    case GameOption::kDamageEffectNumbers:
      look.effect_numbers = asked.on;
      break;
    case GameOption::kDamageSplit:
      // ⚠ `chosen` ET NON `value` : c'est une liste déroulante.
      look.split = asked.chosen;
      break;
    case GameOption::kDamageHitInterval:
      look.hit_interval_ms = asked.value;
      break;
    case GameOption::kDamageTotalCarries:
      look.total_carries = asked.on;
      break;
    case GameOption::kDamageTotalForget:
      look.total_forget_ms = asked.value;
      break;
    // ⚠ MONTRÉS EN POUR CENT, GARDÉS DE ZÉRO À UN — l'unité de la composition,
    // comme pour les volumes.
    case GameOption::kDamageScale:
      look.motion.scale = asked.value / kPercent;
      break;
    case GameOption::kDamageFontText:
      look.font_text = asked.on;
      break;
    case GameOption::kDamageFontName:
      // ⚠ LE NOM, PAS LE RANG : voir `HitPopupLook::font_name`.
      look.font_name = DamageFontNameAt(asked.chosen, damage_names_);
      break;
    case GameOption::kDamageFontBold:
      look.font_bold = asked.on;
      break;
    case GameOption::kDamageFontOutline:
      look.font_outline_pixels = asked.value;
      break;
    case GameOption::kDamageDigitPitch:
      look.motion.digit_pitch_pixels = asked.value;
      break;
    case GameOption::kDamageOpacity:
      look.motion.opacity = asked.value / kPercent;
      break;
    case GameOption::kDamageAbbreviate:
      look.motion.abbreviate = asked.on;
      break;
    case GameOption::kDamageColorOwnDealt:
      look.color_own_dealt = asked.color;
      break;
    case GameOption::kDamageColorMonster:
      look.color_on_monster = asked.color;
      break;
    case GameOption::kDamageColorPlayer:
      look.color_on_player = asked.color;
      break;
    case GameOption::kDamageColorSelf:
      look.color_on_self = asked.color;
      break;
    case GameOption::kDamageColorCritical:
      look.color_critical = asked.color;
      break;
    case GameOption::kDamageColorTotal:
      look.color_total = asked.color;
      break;
    case GameOption::kDamagePreview:
      look.preview = asked.on;
      break;
    case GameOption::kDamageColorSp:
      look.color_sp = asked.color;
      break;
    case GameOption::kDamageColorHeal:
      look.color_heal = asked.color;
      break;
    case GameOption::kDamageColorSpRecovery:
      look.color_sp_recovery = asked.color;
      break;
    case GameOption::kDamageClientLook:
      // 🔴 LA STRUCTURE VIDE, ET RIEN D'AUTRE : ses initialisateurs sont la
      // seule source des défauts, et les recopier ici en ferait une seconde.
      // ⚠ L'APERÇU SURVIT AU RETOUR : c'est pendant qu'on règle qu'on y revient,
      // et l'éteindre au passage couperait ce qu'on est en train de regarder.
      {
        const bool preview = look.preview;
        look = HitPopupLook{};
        look.preview = preview;
      }
      break;
    default:
      // Pas une ligne de dégâts : l'aiguillage général ne devrait pas nous
      // l'avoir donnée, et il n'y a rien à en faire.
      break;
  }
}

}  // namespace moonclient
