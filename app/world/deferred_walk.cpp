#include "world/deferred_walk.h"

#include "ro/render/strike_play.h"
#include "ro/sim/movement.h"
#include "ro/sim/world.h"

namespace moonclient {

bool GestureHoldsWalk(const PlayerGesture& gesture) noexcept {
  switch (gesture.motion) {
    case ro::render::Motion::kAttack:
      // Un coup sans état posé est l'attaque ordinaire qu'un enchaînement
      // rejoue.
      return gesture.action_state.value_or(ro::render::kStrikeAttackState) ==
             ro::render::kStrikeAttackState;
    case ro::render::Motion::kPickup:
    case ro::render::Motion::kSit:
      return true;
    default:
      return false;
  }
}

std::uint32_t MotionOriginMs(const ro::sim::Actor& self, ro::render::Motion motion) {
  switch (motion) {
    case ro::render::Motion::kAttack:
      return self.attack.has_value() ? self.attack->started_ms : 0;
    case ro::render::Motion::kPickup:
      return self.pickup_since_ms.value_or(0);
    default:
      return self.posture_since_ms;
  }
}

bool StandsOn(const ro::sim::World& world, ro::sim::CellPosition cell) {
  const ro::sim::Actor* self = world.player();
  if (self == nullptr) {
    return false;
  }
  const ro::sim::CellPosition standing = ro::sim::StandingCell(*self);
  return standing.x == cell.x && standing.y == cell.y;
}

std::optional<ro::sim::CellPosition> GroundWalkDue(const ro::sim::World& world,
                                                   ro::sim::ApproachWalkClock& clock,
                                                   DeferredWalk* deferred,
                                                   std::optional<PlayerGesture> player_gesture,
                                                   ro::sim::CellPosition destination,
                                                   std::uint32_t now_ms) {
  const ro::sim::Actor* self = world.player();
  if (deferred != nullptr && self != nullptr && player_gesture.has_value() &&
      GestureHoldsWalk(*player_gesture)) {
    *deferred = DeferredWalk{.destination = destination,
                             .held_gesture = *player_gesture,
                             .held_since_ms = MotionOriginMs(*self, player_gesture->motion),
                             .map_entry = world.map_entries()};
    return std::nullopt;
  }
  if (deferred != nullptr) {
    deferred->destination.reset();
  }
  if (!ro::sim::AdmitWalkRequest(world, clock, now_ms)) {
    return std::nullopt;
  }
  return destination;
}

std::optional<ro::sim::CellPosition> ReleasedWalk(const ro::sim::World& world,
                                                  ro::sim::ApproachWalkClock& clock,
                                                  DeferredWalk& deferred,
                                                  std::optional<PlayerGesture> player_gesture,
                                                  std::uint32_t now_ms) {
  if (!deferred.destination.has_value()) {
    return std::nullopt;
  }
  // Une autre carte, un autre personnage : la case gardée n'y veut plus rien dire.
  const ro::sim::Actor* self = world.player();
  if (deferred.map_entry != world.map_entries() || self == nullptr) {
    deferred.destination.reset();
    return std::nullopt;
  }
  // 🔴 LE MÊME GESTE, RECOMMENCÉ, LIBÈRE AUSSI : c'est tout changement de motion
  // qui rend la marche, et un second coup en est un.
  const PlayerGesture shown = player_gesture.value_or(PlayerGesture{});
  if (shown == deferred.held_gesture &&
      MotionOriginMs(*self, shown.motion) == deferred.held_since_ms) {
    return std::nullopt;
  }
  const ro::sim::CellPosition destination = *deferred.destination;
  deferred.destination.reset();
  if (!ro::sim::AdmitWalkRequest(world, clock, now_ms)) {
    return std::nullopt;
  }
  return destination;
}

}  // namespace moonclient
