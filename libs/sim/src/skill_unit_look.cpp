#include "ro/sim/skill_unit_look.h"

#include "ro/sim/actor.h"
#include "ro/sim/world.h"

namespace ro::sim {

bool ChangeSkillUnitLook(World& world, std::uint32_t id, std::uint32_t unit_id,
                         std::uint32_t now_ms) {
  const auto found = world.skill_units().find(id);
  if (found == world.skill_units().end()) {
    return false;
  }
  SkillUnit changed = found->second;
  if (changed.visible) {
    changed.unit_id = unit_id;
  }
  changed.look_since_ms = now_ms;
  changed.sprung_ms = now_ms;
  world.PlaceSkillUnit(changed);
  return true;
}

}  // namespace ro::sim
