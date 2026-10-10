#include "ro/sim/world_bearers.h"

#include "ro/sim/world.h"

namespace ro::sim {

bool WorldHolds(const World& world, std::uint32_t id) noexcept {
  return world.actors().contains(id) || world.skill_units().contains(id);
}

}  // namespace ro::sim
