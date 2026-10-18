#include "ro/render/effect_anchored_skirt_seed.h"

#include <cstddef>
#include <string>

#include "ro/core/sorted_table.h"
#include "ro/render/effect_anchored_skirt.h"
#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

namespace ro::render {

std::span<const EffectAnchoredSkirtPattern> EffectAnchoredSkirtPatternsFor(
    std::uint32_t effect_id) {
  return core::RowsWithKey(EffectAnchoredSkirtPatternTable(), effect_id,
                           &EffectAnchoredSkirtPattern::effect_id);
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectAnchoredSkirtTracks(
    const EffectAnchoredSkirtPattern& pattern, glm::vec3 toward_target) {
  // ⚠ LE CAP VIENT DE LA CIBLE, PLUS CENT QUATRE-VINGT-QUINZE DEGRÉS. La gerbe
  // part donc PRESQUE à l'opposé de ce qu'elle vise — c'est mesuré, et c'est ce
  // qui la fait jaillir vers l'arrière avant de dériver.
  const std::int32_t to_target = HeadingOfVector(toward_target.x, toward_target.z);
  const auto cap =
      static_cast<std::int16_t>(WithinFullTurn(to_target + kAnchoredSkirtHeadingOffset));

  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    EffectTrack& track = tracks[rang];
    track.active = true;
    track.ticks = -kAnchoredSkirtDelayPerTrack * static_cast<std::int32_t>(rang);
    track.alpha = 0;
    track.lift_key = pattern.ribbon_length;
    track.degrees = kAnchoredSkirtSweepDegrees;
    track.azimuth_degrees = cap;
    track.radius_at_anchor =
        pattern.first_radius - static_cast<float>(rang) * kAnchoredSkirtRadiusPerTrack;
    // ⚠ SOIXANTE-DIX DEGRÉS, PUIS CINQ DE MOINS PAR PISTE : le client écrit cinq
    // fois quatorze moins le rang.
    track.tilt_degrees =
        static_cast<std::int16_t>(kAnchoredSkirtTiltPerTrack *
                                  (kAnchoredSkirtFirstTiltStep - static_cast<std::int16_t>(rang)));
    track.tail[kAnchoredSkirtTailOfDriftX] = 0.0F;
    track.tail[kAnchoredSkirtTailOfDriftZ] = 0.0F;
    track.tail[kAnchoredSkirtTailOfVariant] = pattern.trail_variant;
  }
  return tracks;
}

PooledEffectNode SowEffectAnchoredSkirtNode(const EffectAnchoredSkirtPattern& pattern,
                                            glm::vec3 anchor, glm::vec3 toward_target,
                                            std::int32_t effect_life_ticks) {
  PooledEffectNode pooled = PooledNodeAt(PooledNodeShape::kAnchoredSkirt, anchor);
  pooled.node.life_ticks = effect_life_ticks;
  pooled.resource_paths.reserve(pattern.textures.size());
  for (const std::string_view image : pattern.textures) {
    pooled.resource_paths.emplace_back(image);
  }
  pooled.tracks = SeedEffectAnchoredSkirtTracks(pattern, toward_target);
  return pooled;
}

}  // namespace ro::render
