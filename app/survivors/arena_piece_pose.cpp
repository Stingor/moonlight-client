// LA POSE D'UN MODÈLE DANS SA PIÈCE : sa place dans l'emprise tournée, son
// angle et son échelle (`arena_ground_plan.h`).

#include <cmath>
#include <cstdint>

#include <glm/glm.hpp>

#include "ro/sim/survivors_terrain.h"

#include "survivors/arena_ground_plan.h"

namespace moonclient {
namespace {

constexpr std::uint32_t kFullTurnDegrees = 360;
constexpr float kQuarterTurnDegrees = 90.0F;

}  // namespace

float PieceModelYawDegrees(std::uint8_t quarter, float slot_yaw_degrees) {
  const auto full_turn = static_cast<float>(kFullTurnDegrees);
  const float turned =
      std::fmod(slot_yaw_degrees - static_cast<float>(quarter % ro::sim::kTerrainQuarterTurns) *
                                       kQuarterTurnDegrees,
                full_turn);
  return turned < 0.0F ? turned + full_turn : turned;
}

PlannedPose PieceSlotPose(const PieceModelSlot& slot, const GroundModelChoice& choice,
                          ro::sim::FootprintSize drawn, std::uint8_t quarter,
                          float drawn_yaw_degrees, float jitter) {
  const glm::vec2 at = PieceSlotPoint({slot.east_cells, slot.north_cells}, drawn, quarter);
  const float yaw =
      slot.any_yaw ? drawn_yaw_degrees : PieceModelYawDegrees(quarter, slot.yaw_degrees);
  const float grown = slot.any_yaw ? jitter : 1.0F;
  // L'angle du RSW tourne le modèle dans le sens des aiguilles d'une montre vu
  // du ciel : (est, nord) → (est cos θ + nord sin θ, nord cos θ − est sin θ).
  const float radians = glm::radians(yaw);
  const glm::vec2 centre(choice.centre_east_cells * grown, choice.centre_north_cells * grown);
  const glm::vec2 turned(centre.x * std::cos(radians) + centre.y * std::sin(radians),
                         centre.y * std::cos(radians) - centre.x * std::sin(radians));
  return PlannedPose{.model = choice.model,
                     .east = at.x - turned.x,
                     .north = at.y - turned.y,
                     .yaw_degrees = yaw,
                     .scale = choice.scale * grown,
                     .sink_units = choice.sink_units * choice.scale * grown};
}

glm::vec2 PieceSlotPoint(glm::vec2 point, ro::sim::FootprintSize drawn,
                         std::uint8_t quarter) noexcept {
  const auto width = static_cast<float>(drawn.width);
  const auto height = static_cast<float>(drawn.height);
  switch (quarter % ro::sim::kTerrainQuarterTurns) {
    case 1:
      return {height - point.y, point.x};
    case 2:
      return {width - point.x, height - point.y};
    case 3:
      return {point.y, width - point.x};
    default:
      return point;
  }
}

}  // namespace moonclient
