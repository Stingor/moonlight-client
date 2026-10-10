#include "world/item_dropper.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <string>
#include <utility>

#include "ro/render/item_drop.h"
#include "ro/sim/movement.h"
#include "ro/sim/world.h"

namespace moonclient {

const ro::sim::Actor* DropperNear(const ro::sim::World& world, const ro::sim::Actor& item,
                                  std::uint32_t now_ms) {
  const ro::sim::FractionalCell landed = ro::sim::InterpolatedCell(item, now_ms);
  // Le carré de la distance : aucune racine à tirer pour un simple classement.
  float nearest = kItemArcSourceRangeCells * kItemArcSourceRangeCells;
  const ro::sim::Actor* dropper = nullptr;
  for (const auto& [id, actor] : world.actors()) {
    if (actor.kind == ro::sim::ActorKind::kItem) {
      continue;
    }
    const ro::sim::FractionalCell cell = ro::sim::InterpolatedCell(actor, now_ms);
    const float dx = cell.x - landed.x;
    const float dy = cell.y - landed.y;
    const float distance = dx * dx + dy * dy;
    if (distance >= nearest) {
      continue;
    }
    nearest = distance;
    dropper = &actor;
  }
  return dropper;
}

std::string GroundItemAmount(const ro::sim::Actor& actor) {
  if (actor.kind != ro::sim::ActorKind::kItem || !actor.ground_item.has_value() ||
      actor.ground_item->amount <= 1) {
    return {};
  }
  return std::to_string(actor.ground_item->amount);
}

std::size_t SowFakeLoot(ro::sim::World& world, std::size_t count, std::uint16_t item_id,
                        std::uint16_t drop_effect_mode, std::uint32_t now_ms) {
  // Les leurres du tour précédent s'en vont, quel que soit leur nombre : une
  // commande plus courte que la dernière en laisserait traîner.
  for (std::size_t rank = 0; rank < kFakeLootCount; ++rank) {
    world.Remove(kFakeLootFirstId + static_cast<std::uint32_t>(rank));
  }
  const ro::sim::Actor* self = world.player();
  if (self == nullptr || count == 0) {
    return 0;
  }
  const ro::sim::FractionalCell here = ro::sim::InterpolatedCell(*self, now_ms);
  const std::size_t sown = std::min(count, kFakeLootCount);
  for (std::size_t rank = 0; rank < sown; ++rank) {
    // Un tour complet réparti sur le compte demandé, à UNE case du joueur : la
    // distance à laquelle le serveur pose ses butins.
    constexpr float kTurn = 2.0F * std::numbers::pi_v<float>;
    constexpr float kRingCells = 1.0F;
    const float angle = kTurn * static_cast<float>(rank) / static_cast<float>(sown);
    ro::sim::Actor loot;
    loot.id = kFakeLootFirstId + static_cast<std::uint32_t>(rank);
    loot.kind = ro::sim::ActorKind::kItem;
    loot.position = ro::sim::CellPosition{
        static_cast<std::uint16_t>(std::lround(here.x + std::cos(angle) * kRingCells)),
        static_cast<std::uint16_t>(std::lround(here.y + std::sin(angle) * kRingCells)), 0};
    // ⚠ LE MODE TOURNANT COMMENCE À UN : zéro ne plante aucun pilier, et le
    // premier butin d'une salve n'aurait alors rien à montrer.
    const std::uint16_t mode =
        drop_effect_mode == kFakeLootCyclingMode
            ? static_cast<std::uint16_t>(rank % ro::render::kItemDropPillarCount + 1)
            : drop_effect_mode;
    loot.ground_item = ro::sim::GroundItem{.since_ms = now_ms,
                                           .falling = true,
                                           .show_drop_effect = mode != 0,
                                           .drop_effect_mode = mode,
                                           .item_id = item_id};
    world.Spawn(std::move(loot));
  }
  return sown;
}

}  // namespace moonclient
