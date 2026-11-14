// LA RÉDUCTION TRANSITIVE DE L'ARBRE DES COMPÉTENCES.
//
// 🔴 CE QUE CE BANC TIENT, ET QU'AUCUN ÉCRAN NE MONTRERAIT VITE : la liste de
// prérequis du client porte des RACCOURCIS. Bowling Bash réclame Bash ET Magnum
// Break, alors que Magnum Break réclame déjà Bash — cent trente-six arêtes de ce
// genre ont été comptées dans `skillinfolist`. Tracées telles quelles, elles
// doubleraient le même chemin : une flèche directe par-dessus la branche qui
// disait déjà la même chose, et dans la vue en liste un coude de trop.
//
// ⚠ ET CES RACCOURCIS NE FONT PAS DE LA LISTE UNE FERMETURE TRANSITIVE : c'est
// le contraire qu'on croirait. Elle est aussi INCOMPLÈTE — Cart Termination
// réclame Cart Boost, qui réclame Pushcart 5, qui réclame Enlarge Weight Limit 5,
// laquelle n'apparaît nulle part chez Cart Termination. Les deux moitiés
// comptent, et chacune commande un traitement.

#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/skill_window.h"

using ro::ui::SkillCellView;
using ro::ui::SkillEdgeIsRedundant;
using ro::ui::SkillNeedView;

namespace {

// Les identifiants sont ceux du client, pour que l'extrait se recoupe avec ses
// tables sans avoir à les traduire.
constexpr std::uint16_t kBash = 5;          // SM_BASH
constexpr std::uint16_t kMagnumBreak = 7;   // SM_MAGNUM
constexpr std::uint16_t kBowlingBash = 62;  // KN_BOWLINGBASH
constexpr std::uint16_t kTwoHandQuicken = 60;

}  // namespace

TEST_CASE("Chaîne : le raccourci du client ne se trace pas deux fois", "[ui][grimoire]") {
  // L'arbre de l'épéiste, tel que `skillinfolist` l'écrit.
  const std::array<SkillNeedView, 1> magnum_needs = {SkillNeedView{.skill_id = kBash, .level = 5}};
  const std::array<SkillNeedView, 2> bowling_needs = {
      SkillNeedView{.skill_id = kBash, .level = 5},
      SkillNeedView{.skill_id = kMagnumBreak, .level = 3}};
  const std::vector<SkillCellView> cells = {
      SkillCellView{.skill_id = kBash},
      SkillCellView{.skill_id = kMagnumBreak, .needs = magnum_needs},
      SkillCellView{.skill_id = kBowlingBash, .needs = bowling_needs},
  };

  // 🔴 L'ARÊTE DIRECTE BASH -> BOWLING BASH EST REDONDANTE : Magnum Break, autre
  // prérequis de Bowling Bash, réclame déjà Bash. Le chemin est dit une fois.
  CHECK(SkillEdgeIsRedundant(cells, cells[2], kBash));

  // ⚠ ET LES DEUX AUTRES NE LE SONT PAS : celle qui passe par Magnum Break est
  // le chemin lui-même, et Bash -> Magnum Break en est le premier maillon.
  CHECK_FALSE(SkillEdgeIsRedundant(cells, cells[2], kMagnumBreak));
  CHECK_FALSE(SkillEdgeIsRedundant(cells, cells[1], kBash));
}

TEST_CASE("Chaîne : un prérequis absent de l'onglet ne raccourcit rien", "[ui][grimoire]") {
  // ⚠ LA RÉDUCTION NE VOIT QUE CE QUE L'ONGLET PORTE, et il le faut : les arbres
  // se fusionnent par paliers, et une compétence d'un autre onglet n'a ni case ni
  // flèche ici. La chercher ailleurs ferait sauter un trait dont rien ne
  // montrerait le remplaçant.
  const std::array<SkillNeedView, 2> bowling_needs = {
      SkillNeedView{.skill_id = kBash, .level = 5},
      SkillNeedView{.skill_id = kMagnumBreak, .level = 3}};
  const std::vector<SkillCellView> cells = {
      SkillCellView{.skill_id = kBash},
      SkillCellView{.skill_id = kBowlingBash, .needs = bowling_needs},
  };
  CHECK_FALSE(SkillEdgeIsRedundant(cells, cells[1], kBash));
}

TEST_CASE("Chaîne : deux prérequis sans rapport gardent leurs deux traits", "[ui][grimoire]") {
  // Aucun des deux ne réclame l'autre : les deux chemins sont distincts, et les
  // effacer laisserait une compétence sans origine visible.
  const std::array<SkillNeedView, 2> both = {
      SkillNeedView{.skill_id = kBash, .level = 1},
      SkillNeedView{.skill_id = kTwoHandQuicken, .level = 1}};
  const std::vector<SkillCellView> cells = {
      SkillCellView{.skill_id = kBash},
      SkillCellView{.skill_id = kTwoHandQuicken},
      SkillCellView{.skill_id = kBowlingBash, .needs = both},
  };
  CHECK_FALSE(SkillEdgeIsRedundant(cells, cells[2], kBash));
  CHECK_FALSE(SkillEdgeIsRedundant(cells, cells[2], kTwoHandQuicken));
}

TEST_CASE("Chaîne : une compétence sans prérequis n'a aucune arête", "[ui][grimoire]") {
  const std::vector<SkillCellView> cells = {SkillCellView{.skill_id = kBash}};
  CHECK_FALSE(SkillEdgeIsRedundant(cells, cells[0], kMagnumBreak));
}
