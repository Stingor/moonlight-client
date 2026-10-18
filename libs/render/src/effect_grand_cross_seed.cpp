#include "ro/render/effect_grand_cross_seed.h"

#include <string>

#include "ro/core/sorted_table.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

namespace ro::render {

std::span<const EffectGrandCrossPattern> EffectGrandCrossPatternsFor(std::uint32_t effect_id) {
  return core::RowsWithKey(EffectGrandCrossPatternTable(), effect_id,
                           &EffectGrandCrossPattern::effect_id);
}

std::array<EffectTrack, kEffectTracksPerNode> SeedGrandCrossRibbonTracks() {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  for (std::size_t rang = 0; rang < kEffectTracksPerNode; ++rang) {
    EffectTrack& track = tracks[rang];
    track.active = true;
    track.ticks = 0;
    // ⚠ IL NAÎT INVISIBLE, comme le bouclier de Kyrie : c'est le mouvement qui
    // monte l'alpha de dix par tic pendant dix tics.
    track.alpha = 0;
    track.degrees = kGrandCrossRibbonSweepDegrees;
    track.lift_key = kGrandCrossRibbonLiftKey;
    track.radius_at_anchor = kGrandCrossRibbonRadius;
    track.tilt_degrees = kGrandCrossRibbonTilt;
    track.azimuth_degrees = kGrandCrossRibbonAzimuths[rang];
    track.mark[0] = static_cast<std::uint8_t>(kGrandCrossRibbonArmOffsets[rang][0]);
    track.mark[1] = static_cast<std::uint8_t>(kGrandCrossRibbonArmOffsets[rang][1]);
  }
  return tracks;
}

std::array<EffectTrack, kEffectTracksPerNode> SeedGrandCrossFrameTracks() {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  // ⚠ DEUX SEULEMENT. Le bâtisseur couche explicitement les deux dernières.
  for (std::size_t rang = 0; rang < kGrandCrossFrameLivingTracks; ++rang) {
    EffectTrack& track = tracks[rang];
    track.active = true;
    track.ticks = 0;
    track.alpha = 0;
    track.lift_key = kGrandCrossFrameLiftKey;
    track.azimuth_degrees =
        static_cast<std::int16_t>(kGrandCrossFrameAzimuthStep * static_cast<std::int16_t>(rang));
    track.value[0] = kGrandCrossFrameHalfWidth;
    track.value[1] = kGrandCrossFrameHalfDepth;
    track.value[2] = 0.0F;
  }
  return tracks;
}

PooledEffectNode SowGrandCrossRibbonNode(const EffectGrandCrossPattern& pattern, glm::vec3 anchor,
                                         std::int32_t effect_life_ticks) {
  PooledEffectNode pooled = PooledNodeAt(PooledNodeShape::kGrandCrossRibbon, anchor);
  pooled.node.life_ticks = effect_life_ticks;
  pooled.node.render_word = kGrandCrossRenderWord;
  pooled.resource_paths = {std::string(pattern.ribbon_texture)};
  pooled.trail_variant = static_cast<std::uint8_t>(pattern.ribbon_node_scale);
  pooled.tracks = SeedGrandCrossRibbonTracks();
  return pooled;
}

PooledEffectNode SowGrandCrossFrameNode(const EffectGrandCrossPattern& pattern, glm::vec3 anchor,
                                        std::int32_t effect_life_ticks) {
  PooledEffectNode pooled = PooledNodeAt(PooledNodeShape::kGrandCrossFrame, anchor);
  pooled.node.life_ticks = effect_life_ticks;
  pooled.node.render_word = kGrandCrossRenderWord;
  pooled.resource_paths = {std::string(pattern.frame_texture)};
  pooled.tracks = SeedGrandCrossFrameTracks();
  return pooled;
}

}  // namespace ro::render
