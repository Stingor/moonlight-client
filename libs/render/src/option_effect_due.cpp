#include "ro/render/effect_frame.h"
#include "ro/render/option_effect_table.h"

namespace ro::render {

const OptionEffect* OptionEffectDue(std::uint32_t option,
                                    std::optional<std::uint32_t> since_sown_ms) {
  for (const OptionEffect& state : OptionEffectTable()) {
    if ((option & state.option_mask) == 0) {
      continue;
    }
    const auto period_ms = static_cast<std::uint32_t>(state.respawn_ticks * kEffectTickMs);
    if (!since_sown_ms.has_value() || *since_sown_ms >= period_ms) {
      return &state;
    }
  }
  return nullptr;
}

}  // namespace ro::render
