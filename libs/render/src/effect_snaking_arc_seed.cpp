#include "ro/render/effect_snaking_arc_seed.h"

#include <string>

#include "ro/core/sorted_table.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_snaking_arc.h"
#include "ro/render/effect_track.h"

#include "effect_draw_below.h"

namespace ro::render {

std::span<const EffectSnakingArcPattern> EffectSnakingArcPatternsFor(std::uint32_t effect_id) {
  return core::RowsWithKey(EffectSnakingArcPatternTable(), effect_id,
                           &EffectSnakingArcPattern::effect_id);
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectSnakingArcTracks(
    const EffectSnakingArcPattern& pattern, glm::vec3 anchor, std::uint32_t& generator) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  for (EffectTrack& track : tracks) {
    track.active = true;
    // ⚠ QUATRE TIRAGES PAR PISTE, ET DANS CET ORDRE : le retard, l'alpha, le
    // cap, puis la longueur. Les intervertir donnerait quatre arcs différents
    // de ceux du client à graine égale.
    track.ticks = -DrawBelow(generator, kSnakingArcDelaySpread);
    track.alpha = static_cast<std::int16_t>(DrawBelow(generator, kSnakingArcAlphaSpread) +
                                            kSnakingArcAlphaFloor);
    track.tilt_degrees =
        static_cast<std::int16_t>(DrawBelow(generator, kEffectTrackFullTurnDegrees));
    track.radius_at_anchor =
        static_cast<float>(DrawBelow(generator, kSnakingArcLengthSpread)) + kSnakingArcLengthFloor;
    track.lift_key = kSnakingArcTubeRadius;
    track.style = pattern.style;
    track.anchor = anchor;
  }
  return tracks;
}

PooledEffectNode SowEffectSnakingArcNode(const EffectSnakingArcPattern& pattern, glm::vec3 anchor,
                                         std::int32_t effect_life_ticks, std::uint32_t& generator) {
  PooledEffectNode pooled = PooledNodeAt(PooledNodeShape::kSnakingArc, anchor);
  pooled.node.life_ticks = effect_life_ticks;
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.tracks = SeedEffectSnakingArcTracks(pattern, anchor, generator);
  return pooled;
}

}  // namespace ro::render
