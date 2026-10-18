#include "ro/render/str_effect_setup.h"

#include "ro/core/sorted_table.h"
#include "ro/render/effect_class_registry.h"
#include "ro/render/effect_render_word.h"

namespace ro::render {

const StrEffectSetup* StrEffectSetupFor(std::uint32_t effect_id) {
  return core::RowWithKey(StrEffectSetupTable(), effect_id, &StrEffectSetup::effect_id);
}

float StrEffectLevelScale(const StrEffectSetup& setup, std::int32_t level) {
  for (const StrLevelScale& range : setup.level_scales) {
    if (level >= range.level_min && level <= range.level_max) {
      return setup.scale * range.scale;
    }
  }
  return setup.scale;
}

bool StrPaintsBehindActors(const StrEffectSetup& setup) {
  return (setup.render_word & kEffectRenderWordEarly) != 0;
}

bool StrPaintsOverEverything(const StrEffectSetup& setup) {
  return (setup.render_word & kEffectRenderWordOverEverything) != 0;
}

bool StrDiesWithBearer(std::uint32_t effect_id) {
  if (!ClientRegistersEffectClass(effect_id)) {
    return true;
  }
  const StrEffectSetup* setup = StrEffectSetupFor(effect_id);
  return setup != nullptr && setup->lives_with_owner;
}

}  // namespace ro::render
