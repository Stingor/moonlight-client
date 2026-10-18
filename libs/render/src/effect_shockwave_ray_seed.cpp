#include "ro/render/effect_shockwave_ray_seed.h"

#include <algorithm>
#include <string>

#include "ro/core/sorted_table.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

namespace ro::render {

std::span<const EffectShockwaveRayPattern> EffectShockwaveRayPatternsFor(std::uint32_t effect_id) {
  return core::RowsWithKey(EffectShockwaveRayPatternTable(), effect_id,
                           &EffectShockwaveRayPattern::effect_id);
}

std::array<EffectTrack, kEffectTracksPerNode> SeedShockwaveRayTracks(std::int16_t base_degrees) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    EffectTrack& track = tracks[rang];
    track.active = true;
    track.ticks = 0;
    track.alpha = 0;
    track.radius_at_anchor = kShockwaveRayRadius;
    // ⚠ L'ANGLE N'EST PAS REPLIÉ : le client range la somme telle quelle, et la
    // figure la replie en l'employant.
    track.tilt_degrees = static_cast<std::int16_t>(
        base_degrees + (kShockwaveRayDegreesPerTrack * static_cast<std::int32_t>(rang)));
  }
  return tracks;
}

PooledEffectNode SowShockwaveRayNode(const EffectShockwaveRayPattern& pattern,
                                     glm::vec3 instance_position, std::int32_t effect_life_ticks) {
  PooledEffectNode pooled = PooledNodeAt(PooledNodeShape::kShockwaveRays, instance_position);
  pooled.node.life_ticks = std::max(effect_life_ticks, 1);
  pooled.node.render_word = kShockwaveRayRenderWord;
  // ⚠ LES RAIS RESTENT OÙ ILS SONT NÉS : leur instance suit son porteur, mais
  // ni leur mouvement ni leur dessin ne relisent sa place.
  pooled.stays_where_sown = true;
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.tracks = SeedShockwaveRayTracks(pattern.base_degrees);
  return pooled;
}

}  // namespace ro::render
