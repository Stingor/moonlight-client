#include "effects/effect_footprint_sow.h"

#include "ro/render/effect_footprint_seed.h"
#include "ro/render/effect_life_table.h"
#include "ro/render/effect_node_pool.h"

namespace moonclient {

bool SowFootprints(ro::render::EffectNodePool& pool, std::uint32_t effect_id,
                   glm::vec3 ground_position, float azimuth_degrees,
                   const std::function<bool(const std::string&)>& has_texture) {
  bool sown = false;
  // Le bâtisseur range l'angle de son instance dans un entier : il le TRONQUE.
  const auto azimuth = static_cast<std::int16_t>(azimuth_degrees);
  for (const ro::render::EffectFootprintPattern& pattern :
       ro::render::EffectFootprintPatternsFor(effect_id)) {
    if (!has_texture(std::string(pattern.texture))) {
      continue;
    }
    pool.Add(ro::render::SowEffectFootprintNode(pattern, ground_position, azimuth,
                                                ro::render::EffectNodeLifeFor(effect_id)));
    sown = true;
  }
  return sown;
}

}  // namespace moonclient
