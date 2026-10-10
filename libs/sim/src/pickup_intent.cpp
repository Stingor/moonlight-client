#include "ro/sim/pickup_intent.h"

#include <algorithm>
#include <cstdlib>

#include "ro/sim/movement.h"
#include "ro/sim/world.h"

namespace ro::sim {

bool WithinPickupReach(CellPosition actor, CellPosition item) noexcept {
  const int delta_x = std::abs(static_cast<int>(actor.x) - static_cast<int>(item.x));
  const int delta_y = std::abs(static_cast<int>(actor.y) - static_cast<int>(item.y));
  return std::max(delta_x, delta_y) <= kPickupReachCells;
}

PickupStep AdvancePickup(const World& world, const WalkPath& player_path,
                         const PickupIntent& intent, std::uint32_t now_ms) {
  const Actor* item = world.Find(intent.item_id);
  if (item == nullptr || item->kind != ActorKind::kItem) {
    return PickupStep::kGiveUp;
  }
  const Actor* player = world.player();
  if (player == nullptr) {
    return PickupStep::kGiveUp;
  }
  if (IsWalking(*player, player_path, now_ms)) {
    return PickupStep::kWait;
  }
  if (WithinPickupReach(StandingCell(*player), item->position)) {
    return PickupStep::kPickUp;
  }
  // Arrêté hors de portée. Si une marche a commencé depuis le clic, elle a
  // fini ailleurs que prévu ; sinon, le serveur a peut-être encore à répondre.
  const bool walked_since_click =
      player->movement.has_value() && player->movement_started_ms >= intent.since_ms;
  if (walked_since_click) {
    return PickupStep::kGiveUp;
  }
  // Une marche que le plancher retient encore n'a pas eu de réponse à attendre.
  if (intent.walk_toward.has_value()) {
    return PickupStep::kWait;
  }
  return now_ms >= intent.since_ms + kWalkAnswerGraceMs ? PickupStep::kGiveUp : PickupStep::kWait;
}

std::optional<CellPosition> DuePickupWalk(const World& world, PickupIntent& intent,
                                          ApproachWalkClock& clock, std::uint32_t now_ms) {
  if (!intent.walk_toward.has_value() || !AdmitWalkRequest(world, clock, now_ms)) {
    return std::nullopt;
  }
  const CellPosition toward = *intent.walk_toward;
  intent.walk_toward.reset();
  // Le délai de réponse du serveur compte depuis la marche, pas depuis le clic.
  intent.since_ms = now_ms;
  return toward;
}

}  // namespace ro::sim
