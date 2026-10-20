#include "effects/effect_arrow_shower.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string_view>
#include <vector>

#include "ro/render/effect_frame.h"
#include "ro/render/sprite_catalog.h"
#include "ro/sim/world.h"

#include "effects/effect_node_view.h"
#include "world/element_glow_of.h"

namespace moonclient {
namespace {

// LA FLÈCHE GÉNÉRIQUE, que la volée impose quel que soit le tireur.
constexpr std::string_view kShowerArrowSprite = "data\\sprite\\몬스터\\skel_archer_arrow";
// L'ATTENTE avant l'envol : dix-huit pas de vingt-quatre millisecondes.
constexpr std::uint32_t kShowerWaitMs = 432;
// LE BUT EST RELEVÉ DEUX FOIS de la hauteur d'envol : vingt unités au-dessus
// du sol, là où le départ n'en prend que dix.
constexpr float kShowerTargetLiftFactor = 2.0F;
// LES HUIT CASES VOISINES, dans l'ordre du client : la case visée n'en a pas.
constexpr std::array<std::array<float, 2>, 8> kNeighbourCells = {
    {{1, 0}, {1, 1}, {1, -1}, {-1, 0}, {-1, 1}, {-1, -1}, {0, 1}, {0, -1}}};

}  // namespace

void QueueArrowShower(std::vector<ShowerArrow>& waiting, const MapStage& stage,
                      std::uint32_t shooter_id, glm::vec3 caster_ground,
                      glm::vec3 first_target_ground, float centre_x, float centre_y,
                      std::uint32_t now_ms) {
  const std::uint32_t due = now_ms + kShowerWaitMs;
  waiting.push_back(ShowerArrow{due, shooter_id, caster_ground, first_target_ground});
  for (const std::array<float, 2>& cell : kNeighbourCells) {
    const std::array<float, 3> ground =
        GroundPositionAt(stage, std::round(centre_x) + cell[0], std::round(centre_y) + cell[1]);
    waiting.push_back(
        ShowerArrow{due, shooter_id, caster_ground, glm::vec3(ground[0], ground[1], ground[2])});
  }
}

void ShootDueShowerArrows(std::vector<ShowerArrow>& waiting, EffectNodesInPlay& nodes,
                          const ro::sim::World& world, bool element_glow, std::uint32_t now_ms) {
  std::erase_if(waiting, [&](const ShowerArrow& arrow) {
    if (static_cast<std::int32_t>(now_ms - arrow.due_ms) < 0) {
      return false;
    }
    const glm::vec3 envol{0.0F, ro::render::kArrowFlightHeight, 0.0F};
    const glm::vec3 start = arrow.from_ground + envol;
    const glm::vec3 arrivee = arrow.to_ground + kShowerTargetLiftFactor * envol;
    const glm::vec3 vol = arrivee - start;
    const float distance = glm::length(vol);
    if (distance <= 0.0F) {
      return true;
    }
    // ⚠ SOUS QUARANTE UNITÉS LA FLÈCHE NE RALENTIT PAS : cinq unités par pas.
    const float part = std::min(distance / ro::render::kArrowFullFlightDistance, 1.0F);
    const auto vol_ticks = static_cast<std::int32_t>(
        std::lround(part * ro::render::kArrowFlightMs / ro::render::kEffectTickMs));
    ro::render::ProjectileFlight flight{.image_rule = ro::render::ProjectileImageRule::kArrow};
    if (const ro::sim::Actor* shooter = world.Find(arrow.shooter_id); shooter != nullptr) {
      flight.glow_tint = ArrowGlowOf(*shooter, element_glow);
    }
    static_cast<void>(nodes.ShootSprite(kShowerArrowSprite, start, arrivee, vol_ticks, flight));
    return true;
  });
}

}  // namespace moonclient
