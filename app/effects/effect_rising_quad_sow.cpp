#include "effects/effect_rising_quad_sow.h"

#include <functional>
#include <string>

#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_rising_quad.h"

namespace moonclient {

bool SowRisingQuads(ro::render::EffectNodePool& pool, std::uint32_t effect_id,
                    glm::vec3 ground_position, std::int32_t effect_life_ticks,
                    std::uint32_t generator,
                    const std::function<bool(const std::string&)>& has_texture) {
  bool sown = false;
  for (const ro::render::EffectRisingQuadPattern& pattern :
       ro::render::EffectRisingQuadPatternsFor(effect_id)) {
    if (!has_texture(std::string(pattern.texture))) {
      continue;
    }
    pool.Add(ro::render::SowEffectRisingQuadNode(pattern, ground_position, effect_life_ticks,
                                                 generator));
    sown = true;
  }
  return sown;
}

}  // namespace moonclient
