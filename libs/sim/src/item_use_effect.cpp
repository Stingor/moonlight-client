#include "ro/sim/item_use_effect.h"

#include <algorithm>

namespace ro::sim {

const ItemUseEffect* FindItemUseEffect(std::uint32_t item_id) {
  const std::span<const ItemUseEffect> table = ItemUseEffects();
  const auto found = std::ranges::lower_bound(table, item_id, {}, &ItemUseEffect::item_id);
  if (found == table.end() || found->item_id != item_id) {
    return nullptr;
  }
  return &*found;
}

}  // namespace ro::sim
