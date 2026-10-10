#include "ro/render/effect_snaking_arc_seed.h"

#include <algorithm>
#include <cstddef>
#include <string>

#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_snaking_arc.h"
#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

[[nodiscard]] std::int32_t DrawBelow(std::uint32_t& rng_state, std::int32_t exclusive_bound) {
  return static_cast<std::int32_t>(NextEffectNodeDraw(rng_state) %
                                   static_cast<std::uint32_t>(exclusive_bound));
}

}  // namespace

std::span<const EffectSnakingArcPattern> EffectSnakingArcPatternsFor(std::uint32_t effect_id) {
  const std::span<const EffectSnakingArcPattern> table = EffectSnakingArcPatternTable();
  const auto first =
      std::ranges::lower_bound(table, effect_id, {}, &EffectSnakingArcPattern::effect_id);
  const auto last =
      std::ranges::upper_bound(table, effect_id, {}, &EffectSnakingArcPattern::effect_id);
  return table.subspan(static_cast<std::size_t>(first - table.begin()),
                       static_cast<std::size_t>(last - first));
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
  PooledEffectNode pooled;
  pooled.shape = PooledNodeShape::kSnakingArc;
  pooled.ground_position = anchor;
  pooled.owner_position = anchor;
  pooled.node.life_ticks = effect_life_ticks;
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.tracks = SeedEffectSnakingArcTracks(pattern, anchor, generator);
  return pooled;
}

}  // namespace ro::render
