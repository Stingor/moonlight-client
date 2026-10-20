#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include <glm/glm.hpp>

#include "ro/sim/world_fwd.h"

#include "world/map_stage.h"

// LA PLUIE DE FLÈCHES D'ARROW SHOWER : neuf flèches par lancer au sol, neuf
// par cible touchée, toutes endormies quatre cent trente-deux millisecondes
// avant de partir.
namespace moonclient {

class EffectNodesInPlay;

// La compétence qui les tire.
inline constexpr std::uint16_t kArrowShowerSkill = 47;

// UNE FLÈCHE QUI ATTEND SON DÉPART : qui la tire, et ses deux bouts au SOL,
// dans notre repère.
struct ShowerArrow {
  std::uint32_t due_ms = 0;
  std::uint32_t shooter_id = 0;
  glm::vec3 from_ground{0.0F};
  glm::vec3 to_ground{0.0F};
};

// Met en attente la volée d'une frappe ou d'un lancer de `shooter_id` : une
// flèche vers `first_target_ground`, puis une vers le centre de chacune des huit
// cases voisines de `centre_cell`.
void QueueArrowShower(std::vector<ShowerArrow>& waiting, const MapStage& stage,
                      std::uint32_t shooter_id, glm::vec3 caster_ground,
                      glm::vec3 first_target_ground, float centre_x, float centre_y,
                      std::uint32_t now_ms);

// Tire les flèches dont l'attente est finie. Chacune s'oriente d'elle-même sur
// son vol, à chaque image : voir `EffectNodesInPlay::ShootSprite`. Chacune
// porte au départ la lueur élémentaire de son tireur tel que `world` le dit,
// si `element_glow` la montre.
void ShootDueShowerArrows(std::vector<ShowerArrow>& waiting, EffectNodesInPlay& nodes,
                          const ro::sim::World& world, bool element_glow, std::uint32_t now_ms);

}  // namespace moonclient
