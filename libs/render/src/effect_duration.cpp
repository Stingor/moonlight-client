#include "ro/render/effect_duration.h"

#include <algorithm>
#include <ranges>

namespace ro::render {
namespace {

[[nodiscard]] const EffectDurationEntry* Trouver(std::uint32_t effect_id) {
  const std::span<const EffectDurationEntry> table = EffectDurationTable();
  const auto found =
      std::ranges::lower_bound(table, effect_id, {}, &EffectDurationEntry::effect_id);
  if (found == table.end() || found->effect_id != effect_id) {
    return nullptr;
  }
  return &*found;
}

}  // namespace

bool EffectDurationIsKnown(std::uint32_t effect_id) { return Trouver(effect_id) != nullptr; }

std::int32_t EffectDurationTicks(std::uint32_t effect_id) {
  const EffectDurationEntry* found = Trouver(effect_id);
  return found != nullptr ? found->duration_ticks : 0;
}

std::uint32_t SkillUnitRespawnMs(std::uint32_t effect_id, std::uint8_t withhold_ticks) {
  const EffectDurationEntry* found = Trouver(effect_id);
  if (found == nullptr) {
    return 0;
  }
  const std::int32_t ticks = found->duration_ticks - static_cast<std::int32_t>(withhold_ticks);
  if (ticks <= 0) {
    return 0;
  }
  return static_cast<std::uint32_t>(ticks) * static_cast<std::uint32_t>(kEffectTickMs);
}

}  // namespace ro::render
