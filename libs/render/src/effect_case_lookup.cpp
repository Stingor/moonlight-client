#include <algorithm>

#include "ro/render/effect_case_table.h"

namespace ro::render {

const EffectCaseEntry* EffectCaseFor(std::uint32_t effect_id) {
  const std::span<const EffectCaseEntry> table = EffectCaseTable();
  // La table est triée par identifiant : une recherche dichotomique suffit, et
  // elle est appelée une fois par effet qui naît.
  const auto found = std::ranges::lower_bound(
      table, effect_id, {}, [](const EffectCaseEntry& entry) { return entry.effect_id; });
  if (found == table.end() || found->effect_id != effect_id) {
    return nullptr;
  }
  return &*found;
}

}  // namespace ro::render
