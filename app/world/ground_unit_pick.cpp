#include "world/ground_unit_pick.h"

#include <algorithm>
#include <array>

#include "ro/sim/actor.h"
#include "ro/sim/world.h"

namespace moonclient {
namespace {

// Les unités qui portent une emprise de clic.
constexpr std::array<std::uint32_t, 20> kPickedUnits = {138, 141, 143,   144,   145,   147,  148,
                                                        149, 150, 151,   152,   153,   218,  222,
                                                        229, 230, 20852, 20853, 20854, 20855};
// Celles que le clic frappe et que l'épée désigne, hors siège.
constexpr std::array<std::uint32_t, 10> kAttackedUnits = {141, 143,   152,   218,   222,
                                                          230, 20852, 20853, 20854, 20855};
// Celles que l'épée désigne en siège, en plus.
constexpr std::array<std::uint32_t, 34> kSiegeSwordUnits = {
    141, 143, 144, 145, 147, 148, 149, 150, 151, 152, 153, 190, 191, 192, 193, 194, 203,
    210, 211, 212, 213, 214, 215, 216, 217, 218, 222, 229, 230, 231, 232, 233, 240, 264};
constexpr std::uint32_t kFirstSiegeGuardianUnit = 20852;
constexpr std::uint32_t kLastSiegeGuardianUnit = 20855;

}  // namespace

bool GroundUnitTakesPick(std::uint32_t unit_id) {
  return std::ranges::find(kPickedUnits, unit_id) != kPickedUnits.end();
}

bool GroundUnitClickAttacks(std::uint32_t unit_id) {
  return std::ranges::find(kAttackedUnits, unit_id) != kAttackedUnits.end();
}

bool GroundUnitShowsSword(std::uint32_t unit_id, bool siege_map) {
  if (!siege_map) {
    return GroundUnitClickAttacks(unit_id);
  }
  return std::ranges::find(kSiegeSwordUnits, unit_id) != kSiegeSwordUnits.end() ||
         (unit_id >= kFirstSiegeGuardianUnit && unit_id <= kLastSiegeGuardianUnit);
}

bool GroundUnitOffersPick(const ro::sim::World& world, std::uint32_t id) {
  const auto unit = world.skill_units().find(id);
  return unit != world.skill_units().end() && unit->second.visible &&
         GroundUnitTakesPick(unit->second.unit_id);
}

std::optional<std::uint32_t> PickGroundUnit(const ro::render::Camera& camera, float aspect_ratio,
                                            std::span<const GroundUnitPick> picks, float screen_x,
                                            float screen_y, float floor_pixels,
                                            float viewport_width, float viewport_height) {
  const glm::vec3 eye = camera.Position();
  std::optional<std::uint32_t> nearest;
  float nearest_distance = 0.0F;
  for (const GroundUnitPick& pick : picks) {
    const std::optional<ro::render::ScreenRect> rect = ro::render::GroundUnitScreenRect(
        camera, aspect_ratio, pick.ground, floor_pixels, viewport_width, viewport_height);
    if (!rect.has_value() || !rect->Contains(glm::vec2{screen_x, screen_y})) {
      continue;
    }
    const float distance = glm::length(pick.ground - eye);
    if (!nearest.has_value() || distance < nearest_distance) {
      nearest = pick.id;
      nearest_distance = distance;
    }
  }
  return nearest;
}

bool GroundUnitOutranks(const ro::sim::Actor* hovered_actor) {
  return hovered_actor == nullptr || hovered_actor->kind != ro::sim::ActorKind::kMonster;
}

GroundUnitClick GroundUnitClickOf(std::optional<std::uint32_t> picked_unit_id,
                                  const ro::sim::Actor* hovered_actor, SkillAim aim) {
  if (!picked_unit_id.has_value() || !GroundUnitOutranks(hovered_actor)) {
    return GroundUnitClick::kNotTaken;
  }
  if (aim == SkillAim::kTrap) {
    return GroundUnitClick::kCastSkill;
  }
  if (aim != SkillAim::kNothing) {
    return GroundUnitClick::kNotTaken;
  }
  return GroundUnitClickAttacks(*picked_unit_id) ? GroundUnitClick::kAttack
                                                 : GroundUnitClick::kSwallowed;
}

std::optional<ro::sim::CellPosition> CellCursorOf(
    const std::optional<ro::sim::CellPosition>& hovered_cell, bool over_unit,
    std::optional<ro::sim::CellPosition>& held) {
  if (!over_unit) {
    held = hovered_cell;
  }
  return held;
}

}  // namespace moonclient
