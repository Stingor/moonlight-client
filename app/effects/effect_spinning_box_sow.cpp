#include "effects/effect_spinning_box_sow.h"

#include <functional>
#include <string>

#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_spinning_box_seed.h"

namespace moonclient {

bool SowSpinningBoxes(ro::render::EffectNodePool& pool, std::uint32_t effect_id,
                      glm::vec3 ground_position, std::uint32_t generator,
                      const std::function<bool(const std::string&)>& has_texture) {
  bool sown = false;
  for (const ro::render::EffectSpinningBoxPattern& pattern :
       ro::render::EffectSpinningBoxPatternsFor(effect_id)) {
    if (!has_texture(std::string(pattern.texture))) {
      continue;
    }
    pool.Add(ro::render::SowEffectSpinningBoxNode(pattern, ground_position, generator));
    sown = true;
  }
  return sown;
}

}  // namespace moonclient
