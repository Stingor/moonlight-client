#pragma once

#include <cstdint>
#include <optional>

#include "ro/sim/actor.h"
#include "ro/sim/combat_notice.h"
#include "ro/sim/movement.h"
#include "ro/sim/walk_path.h"
#include "ro/sim/world_fwd.h"

#include "world/hit_popup_rules.h"

namespace moonclient {

// Les côtés d'un coup tels que le monde les donne : qui frappe qui, et ce que
// l'on sait de l'attaquant. Les nombres et le bouclier du « Miss » les lisent
// de la même façon.
//
// ⚠ UNE UNITÉ AU SOL FRAPPÉE N'EST PAS UN JOUEUR : sa classe est celle d'une
// créature, et son chiffre prend la couleur et l'allure d'un monstre.
[[nodiscard]] StrikeSides SidesOfStrike(const ro::sim::World& world, const ro::sim::Strike& strike);

// VRAI QUAND UNE UNITÉ AU SOL A PORTÉ CE COUP : ce que le monde a relevé à son
// arrivée, ou l'unité encore posée pour un coup qu'il n'a pas vu passer.
//
// 🔴 JAMAIS LA SEULE TABLE DES UNITÉS : le serveur retire l'unité d'une météore
// dans le même envoi que son coup, et elle n'y est déjà plus quand on décide
// du chiffre, du son et de l'impact. Voir `Strike::by_ground_unit`.
[[nodiscard]] bool StrikeByGroundUnit(const ro::sim::World& world, const ro::sim::Strike& strike);

// CE QU'UN COUP FRAPPE N'EST PAS TOUJOURS UN ACTEUR.
//
// Un bloc d'Ice Wall, un piège : le serveur met l'identifiant de l'UNITÉ POSÉE
// AU SOL dans le paquet du coup, et le client la cherche parmi ses unités quand
// ses acteurs ne la connaissent pas. Elle reçoit alors le message de dégâts
// comme un monstre le recevrait — son chiffre, son bruit, son impact —, à sa
// case, puisqu'elle ne bouge pas.
//
// ⚠ ET ELLE SURVIT À SON RETRAIT LE TEMPS DE SON DERNIER COUP. Le client ne
// détruit pas une unité que le serveur retire tant qu'un message de dégâts
// attend encore son heure chez elle : le coup qui brise le bloc montre donc son
// chiffre à la case du bloc. C'est pourquoi le monde relève la case de l'unité
// AU COUP — `Strike::struck_unit_cell` —, et elle se garde avec ce qui attend.

// L'unité au sol que `struck_id` désigne, ou nul quand ce n'en est pas une.
[[nodiscard]] const ro::sim::SkillUnit* StruckUnit(const ro::sim::World& world,
                                                   std::uint32_t struck_id);

// Où se tient ce que `struck_id` désigne, à l'instant `at_ms` : un acteur, à la
// case où sa marche l'a mené ; une unité au sol, à sa case. Rien quand le monde
// ne connaît ni l'un ni l'autre.
[[nodiscard]] std::optional<ro::sim::FractionalCell> StruckCellAt(const ro::sim::World& world,
                                                                  ro::sim::WalkPlanner& planner,
                                                                  std::uint32_t struck_id,
                                                                  std::uint32_t at_ms);

// Où se tient la CIBLE de ce coup à l'instant `at_ms` : ce que dit
// `StruckCellAt`, et à défaut la case de l'unité frappée relevée au coup.
[[nodiscard]] std::optional<ro::sim::FractionalCell> StrikeTargetCellAt(
    const ro::sim::World& world, ro::sim::WalkPlanner& planner, const ro::sim::Strike& strike,
    std::uint32_t at_ms);

// La case d'une unité, en cases fractionnaires : celle où tout ce qui la vise
// se pose.
[[nodiscard]] ro::sim::FractionalCell CellOfUnit(ro::sim::CellPosition cell) noexcept;

// L'ANGLE QUE LE CLIENT TIENT POUR UNE UNITÉ AU SOL : zéro, celui de sa
// construction. Ni sa pose, ni un coup reçu ne la tournent — et c'est lui que
// la dérive d'un chiffre jaillissant lit sur son porteur.
inline constexpr float kGroundUnitBodyDegrees = 0.0F;

// OÙ SE TIENT LE PORTEUR D'UN CHIFFRE, et l'angle que le client lui tient.
struct PopupCarrierPlace {
  ro::sim::FractionalCell cell;
  float client_body_degrees = 0.0F;
};

// Le porteur `carrier_id` à l'instant `now_ms` : un acteur, là où sa marche l'a
// mené et tourné comme il se montre ; une unité au sol, à sa case, angle nul.
//
// ⚠ `kept_unit_cell` EST LA CASE DE L'UNITÉ RELEVÉE AU COUP : l'unité que le
// serveur retire avant que le coup ne touche garde son chiffre là — voir plus
// haut. Rien quand aucun des trois ne répond.
[[nodiscard]] std::optional<PopupCarrierPlace> PopupCarrierAt(
    const ro::sim::World& world, ro::sim::WalkPlanner& planner, std::uint32_t carrier_id,
    std::uint32_t now_ms, const std::optional<ro::sim::CellPosition>& kept_unit_cell);

}  // namespace moonclient
