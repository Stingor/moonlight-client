#include "ro/render/effect_break_ring_seed.h"

#include <algorithm>
#include <string>

#include "ro/core/sorted_table.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

namespace ro::render {

std::span<const EffectBreakRingPattern> EffectBreakRingPatternsFor(std::uint32_t effect_id) {
  return core::RowsWithKey(EffectBreakRingPatternTable(), effect_id,
                           &EffectBreakRingPattern::effect_id);
}

std::array<EffectTrack, kEffectTracksPerNode> SeedBreakRingTracks(std::int32_t tick) {
  const std::int32_t rang = tick - kBreakRingFirstWindowTick;
  // ⚠ EN FLOTTANT SIMPLE, comme le client : le rang converti puis multiplié.
  const float key = kBreakRingKeyStart - (static_cast<float>(rang) * kBreakRingKeyPerRank);
  const float radius = (static_cast<float>(rang) * kBreakRingRadiusPerRank) + kBreakRingRadiusStart;
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  for (std::size_t crown = 0; crown < kBreakRingRings; ++crown) {
    const auto k = static_cast<std::int32_t>(crown);
    EffectTrack& track = tracks[crown];
    track.active = true;
    track.ticks = 0;
    track.degrees = kBreakRingSweepDegrees;
    track.lift_key = key;
    track.azimuth_degrees = static_cast<std::int16_t>((kBreakRingAzimuthPerRank * rang) +
                                                      (kBreakRingAzimuthPerRing * k));
    track.alpha = 0;
    track.radius_at_anchor = radius;
    track.tilt_degrees = static_cast<std::int16_t>(
        kBreakRingTiltScale * (kBreakRingTiltBase - (kBreakRingTiltPerRing * k) - rang));
    track.style = static_cast<std::int16_t>(k);
  }
  return tracks;
}

PooledEffectNode SowBreakRingNode(const EffectBreakRingPattern& pattern, glm::vec3 owner_position,
                                  std::int32_t tick, std::int32_t effect_life_ticks) {
  PooledEffectNode pooled = PooledNodeAt(PooledNodeShape::kBreakRings, owner_position);
  pooled.node.life_ticks = std::max(effect_life_ticks, 1);
  pooled.node.render_word = kBreakRingRenderWord;
  pooled.trail_variant = kBreakRingTrailVariant;
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.tracks = SeedBreakRingTracks(tick);
  return pooled;
}

}  // namespace ro::render
