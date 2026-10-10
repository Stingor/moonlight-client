#include "world/item_pillar_memory.h"

#include <algorithm>
#include <utility>

namespace moonclient {

std::uint16_t ItemPillarMemory::ModeOf(const std::string& map_name, const ro::sim::Actor& item) {
  if (!item.ground_item.has_value()) {
    return 0;
  }
  const ro::sim::GroundItem& ground = *item.ground_item;
  if (ground.falling) {
    // ⚠ LE DRAPEAU SANS MODE NE PLANTE AUCUN PILIER : voir `GroundItem`.
    const std::uint16_t mode = ground.show_drop_effect ? ground.drop_effect_mode : 0;
    if (mode == 0) {
      by_floor_id_.erase(item.id);
      return 0;
    }
    Remember(item.id, Remembered{.map_name = map_name,
                                 .item_id = ground.item_id,
                                 .cell_x = item.position.x,
                                 .cell_y = item.position.y,
                                 .mode = mode});
    return mode;
  }
  const auto found = by_floor_id_.find(item.id);
  if (found == by_floor_id_.end()) {
    return 0;
  }
  const Remembered& seen = found->second;
  const bool same_object = seen.map_name == map_name && seen.item_id == ground.item_id &&
                           seen.cell_x == item.position.x && seen.cell_y == item.position.y;
  return same_object ? seen.mode : 0;
}

void ItemPillarMemory::Remember(std::uint32_t floor_id, Remembered remembered) {
  // Une chute s'annonce une fois, mais se relit à chaque image : le souvenir
  // déjà juste garde son rang.
  if (const auto found = by_floor_id_.find(floor_id); found != by_floor_id_.end()) {
    remembered.remembered_rank = found->second.remembered_rank;
    found->second = std::move(remembered);
    return;
  }
  if (by_floor_id_.size() >= kRememberedPillarsMax) {
    const auto oldest = std::ranges::min_element(
        by_floor_id_, {}, [](const auto& entry) { return entry.second.remembered_rank; });
    by_floor_id_.erase(oldest);
  }
  remembered.remembered_rank = next_rank_++;
  by_floor_id_.emplace(floor_id, std::move(remembered));
}

}  // namespace moonclient
