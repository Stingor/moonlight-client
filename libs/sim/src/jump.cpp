// Le saut cosmétique. Voir l'en-tête.

#include "ro/sim/jump.h"

#include <cstdint>

#include "ro/sim/world.h"

namespace ro::sim {
namespace {

// `4t(1 − t)` vaut zéro au départ et à l'arrivée, et un au milieu de l'arc.
constexpr float kParabolaPeakFactor = 4.0F;

}  // namespace

bool Airborne(const Actor& actor, std::uint32_t now_ms) {
  return actor.jump_since_ms.has_value() && now_ms >= *actor.jump_since_ms &&
         now_ms - *actor.jump_since_ms < kJumpDurationMs;
}

float JumpLiftAt(const Actor& actor, std::uint32_t now_ms) {
  if (!Airborne(actor, now_ms)) {
    return 0.0F;
  }
  const float progress =
      static_cast<float>(now_ms - *actor.jump_since_ms) / static_cast<float>(kJumpDurationMs);
  return kJumpCrestUnits * kParabolaPeakFactor * progress * (1.0F - progress);
}

bool World::RecordJump(std::uint32_t id, std::uint32_t since_ms) {
  const auto found = actors_.find(id);
  if (found == actors_.end() || Airborne(found->second, since_ms)) {
    return false;
  }
  found->second.jump_since_ms = since_ms;
  return true;
}

}  // namespace ro::sim
