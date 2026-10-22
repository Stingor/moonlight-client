#include "ro/sim/store_approach.h"

#include <algorithm>
#include <cstddef>

#include "ro/sim/long_walk.h"
#include "ro/sim/movement.h"
#include "ro/sim/world.h"

namespace ro::sim {
namespace {

bool OpensStore(const Actor& merchant) {
  return merchant.signboard.has_value() &&
         (merchant.signboard->kind == SignboardKind::kVendingShop ||
          merchant.signboard->kind == SignboardKind::kBuyingStore);
}

}  // namespace

std::uint16_t StoreApproachRangeCells(const World& world) {
  return static_cast<std::uint16_t>(
      std::max(1, world.session().GameAreaCells() - kStoreReachMarginCells));
}

bool StoreWithinReach(const World& world, const Actor& merchant) {
  const Actor* self = world.player();
  if (self == nullptr) {
    return false;
  }
  const auto reach = static_cast<std::size_t>(std::max(0, world.session().GameAreaCells()));
  return StepsApart(self->position, merchant.position) <= reach &&
         StepsApart(StandingCell(*self), merchant.position) <= reach;
}

std::optional<PendingAction> ArmFarStoreVisit(const World& world, const Actor& merchant) {
  if (world.player() == nullptr || !OpensStore(merchant) || StoreWithinReach(world, merchant)) {
    return std::nullopt;
  }
  PendingAction action;
  action.target_id = merchant.id;
  action.deed = PendingDeed::kVisitStore;
  action.store_kind = merchant.signboard->kind;
  action.range_cells = StoreApproachRangeCells(world);
  return action;
}

bool DropErrandApproach(std::optional<PendingAction>& action) noexcept {
  if (!action.has_value() || action->deed == PendingDeed::kStrike) {
    return false;
  }
  action.reset();
  return true;
}

}  // namespace ro::sim
