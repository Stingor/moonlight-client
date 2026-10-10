#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include <glm/glm.hpp>

#include "world/map_stage.h"

// LA PLUIE DE FLÈCHES D'ARROW SHOWER : neuf flèches par lancer au sol, neuf
// par cible touchée, toutes endormies quatre cent trente-deux millisecondes
// avant de partir.
namespace moonclient {

class EffectNodesInPlay;

// La compétence qui les tire.
inline constexpr std::uint16_t kArrowShowerSkill = 47;

// UNE FLÈCHE QUI ATTEND SON DÉPART : ses deux bouts au SOL, dans notre repère.
struct ShowerArrow {
  std::uint32_t due_ms = 0;
  glm::vec3 from_ground{0.0F};
  glm::vec3 to_ground{0.0F};
};

// Met en attente la volée d'une frappe ou d'un lancer : une flèche vers
// `first_target_ground`, puis une vers le centre de chacune des huit cases
// voisines de `centre_cell`.
void QueueArrowShower(std::vector<ShowerArrow>& waiting, const MapStage& stage,
                      glm::vec3 caster_ground, glm::vec3 first_target_ground, float centre_x,
                      float centre_y, std::uint32_t now_ms);

// Tire les flèches dont l'attente est finie. Chacune s'oriente d'elle-même sur
// son vol, à chaque image : voir `EffectNodesInPlay::ShootSprite`.
void ShootDueShowerArrows(std::vector<ShowerArrow>& waiting, EffectNodesInPlay& nodes,
                          std::uint32_t now_ms);

}  // namespace moonclient
