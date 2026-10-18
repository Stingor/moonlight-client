#include "ro/render/effect_lightning_fan_seed.h"

#include <cstddef>
#include <string>

#include "ro/core/sorted_table.h"
#include "ro/render/effect_lightning_fan.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

#include "effect_draw_below.h"

namespace ro::render {

std::span<const EffectLightningFanPattern> EffectLightningFanPatternsFor(std::uint32_t effect_id) {
  return core::RowsWithKey(EffectLightningFanPatternTable(), effect_id,
                           &EffectLightningFanPattern::effect_id);
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectLightningFanTracks(
    const EffectLightningFanPattern& pattern, glm::vec3 anchor, std::uint32_t& generator) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    EffectTrack& track = tracks[rang];
    track.active = true;
    track.ticks = 0;
    track.alpha = 0;
    track.lift_key = kLightningFanTubeRadius;
    // ⚠ LE RAYON NAÎT NUL : c'est lui qui enfle, et c'est son enflure qui finit
    // par éteindre l'éclair.
    track.radius_at_anchor = 0.0F;
    track.tilt_degrees = static_cast<std::int16_t>(
        pattern.base_tilt_degrees + kLightningFanTiltPerTrack * static_cast<std::int16_t>(rang));
    track.degrees = pattern.sweep;
    // ⚠ UN SEUL TIRAGE PAR PISTE : le facteur de bombement.
    track.tail[kLightningFanTailOfBow] =
        static_cast<float>(DrawBelow(generator, kLightningFanBowSpread)) * kLightningFanBowStep;
    track.tail[kLightningFanTailOfSegments] = kLightningFanSegments;
    track.anchor = anchor;
    if (pattern.raises_anchor) {
      track.anchor.y += kLightningFanRaisedAnchor;
    }
  }
  return tracks;
}

PooledEffectNode SowEffectLightningFanNode(const EffectLightningFanPattern& pattern,
                                           glm::vec3 anchor, std::int32_t effect_life_ticks,
                                           std::uint32_t& generator) {
  PooledEffectNode pooled = PooledNodeAt(PooledNodeShape::kLightningFan, anchor);
  pooled.node.life_ticks = effect_life_ticks;
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.tracks = SeedEffectLightningFanTracks(pattern, anchor, generator);
  return pooled;
}

}  // namespace ro::render
