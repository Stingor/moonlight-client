#include "world/native_marker.h"

#include <cmath>

#include "ro/sim/world.h"

#include "world/follow_rules.h"
#include "world/targeting.h"

namespace moonclient {
namespace {

constexpr float kHalf = 0.5F;
// Le centre d'une cellule, depuis son coin.
constexpr float kCellMiddle = 0.5F;

}  // namespace

const ro::sim::Actor* EngagedActor(const ro::sim::World& world, std::uint32_t id) {
  if (id == 0 || id == world.player_id()) {
    return nullptr;
  }
  return world.Find(id);
}

MarkerCall MarkerOver(const ro::sim::Actor& actor) {
  if (actor.look.job == ro::sim::kPortalJob) {
    return {MarkerOrder::kHide, nullptr};
  }
  if (!actor.player_sprite || !TargetIsConcealed(actor)) {
    return {MarkerOrder::kShow, &actor};
  }
  return {MarkerOrder::kHide, nullptr};
}

MarkerCall NativeMarkerCall(const MouseEngagement& engagement, const ro::sim::World& world,
                            std::uint32_t followed_id) {
  if (followed_id != 0) {
    const ro::sim::Actor* followed = FollowedPlayer(world, followed_id);
    return followed != nullptr ? MarkerOver(*followed) : MarkerCall{};
  }
  if (!engagement.engaged) {
    return {MarkerOrder::kHide, nullptr};
  }
  const ro::sim::Actor* actor = EngagedActor(world, engagement.clicked_id);
  if (actor == nullptr) {
    return {};
  }
  return MarkerOver(*actor);
}

MarkerCall UnderTargetingMode(MarkerCall native, const ro::sim::Actor* hud_marked) {
  if (native.order == MarkerOrder::kHide && hud_marked != nullptr) {
    return {MarkerOrder::kShow, hud_marked};
  }
  return native;
}

ro::sim::FractionalCell MarkerCell(ro::sim::FractionalCell actor_cell, int map_width_cells,
                                   int map_height_cells, float cell_units) {
  // Une cellule se compte par son centre ; l'origine est au centre de la carte.
  const float to_centre_x = kCellMiddle - static_cast<float>(map_width_cells) * kHalf;
  const float to_centre_y = kCellMiddle - static_cast<float>(map_height_cells) * kHalf;
  const float across = std::trunc((actor_cell.x + to_centre_x) * cell_units);
  const float along = std::trunc((actor_cell.y + to_centre_y) * cell_units);
  return {across / cell_units - to_centre_x, along / cell_units - to_centre_y};
}

std::uint32_t MarkerSampleMs(std::optional<std::uint32_t> previous_frame_ms, std::uint32_t now_ms) {
  return previous_frame_ms.value_or(now_ms);
}

void ApplyMarker(MarkerPose& pose, MarkerOrder order, const std::array<float, 3>& ground) {
  switch (order) {
    case MarkerOrder::kKeep:
      return;
    case MarkerOrder::kHide:
      pose.shown = false;
      return;
    case MarkerOrder::kShow:
      pose.shown = true;
      pose.ground = ground;
      return;
  }
}

}  // namespace moonclient
