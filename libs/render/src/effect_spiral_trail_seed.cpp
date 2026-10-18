#include "ro/render/effect_spiral_trail_seed.h"

#include <string>

#include "ro/core/sorted_table.h"
#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_spiral_trail.h"
#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

// LA MARQUE D'ÉTAT, nulle au semis : le grain commence par monter.
constexpr std::size_t kMarkOfFlightState = 1;
// LA MARQUE DE RETARD DE L'ÉTINCELLE, par pas de vingt tics.
constexpr std::size_t kMarkOfSparkDelay = 2;

}  // namespace

std::span<const EffectSpiralTrailPattern> EffectSpiralTrailPatternsFor(std::uint32_t effect_id) {
  return core::RowsWithKey(EffectSpiralTrailPatternTable(), effect_id,
                           &EffectSpiralTrailPattern::effect_id);
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectSpiralTrailTracks(
    glm::vec3 anchor, std::uint32_t& generator) {
  glm::vec3 start = anchor;
  start.x += kSpiralTrailAnchorShiftX;

  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  for (EffectTrack& track : tracks) {
    track.active = true;
    track.ticks =
        kSpiralTrailDelayFloorTicks -
        static_cast<std::int32_t>(NextEffectNodeDraw(generator) % kSpiralTrailDelaySpread);
    track.azimuth_degrees = static_cast<std::int16_t>(
        NextEffectNodeDraw(generator) % static_cast<std::uint32_t>(kEffectTrackFullTurnDegrees));
    track.anchor = start;
    // ⚠ LA TRACE NAÎT AU MÊME POINT QUE LE GRAIN, les trois cases : sans cela le
    // premier tic dessinerait sa traînée à l'origine du monde, le temps que le
    // décalage la remplisse.
    for (std::size_t axe = 0; axe < 3; ++axe) {
      track.tail[axe] = start[static_cast<glm::length_t>(axe)];
      track.tail[kTailOfSpiralTrailRecent + axe] = start[static_cast<glm::length_t>(axe)];
      track.tail[6 + axe] = start[static_cast<glm::length_t>(axe)];
    }
    // ⚠ L'ALPHA PART À ZÉRO ET Y RESTE CENT CINQUANTE-QUATRE TICS : le grain
    // monte invisible, et ne s'allume qu'au décollage.
    track.alpha = 0;
    track.lift_key = kSpiralTrailHalfSize;
    track.mark[kMarkOfSpiralTrailTint] =
        static_cast<std::uint8_t>(NextEffectNodeDraw(generator) % kSpiralTrailTintSpread);
    track.mark[kMarkOfFlightState] = 0;
    track.mark[kMarkOfSpiralTrailSpark] = 0;
    track.mark[kMarkOfSparkDelay] =
        static_cast<std::uint8_t>(kSpiralTrailSparkDelayStep *
                                  (NextEffectNodeDraw(generator) % kSpiralTrailSparkDelaySpread));
    track.radius_at_anchor =
        kSpiralTrailStepFloor +
        static_cast<float>(NextEffectNodeDraw(generator) % kSpiralTrailStepSpread) *
            kSpiralTrailStepUnit;
    track.degrees = static_cast<std::int16_t>(
        NextEffectNodeDraw(generator) % static_cast<std::uint32_t>(kEffectTrackFullTurnDegrees));
    track.tilt_degrees = static_cast<std::int16_t>(
        NextEffectNodeDraw(generator) % static_cast<std::uint32_t>(kEffectTrackFullTurnDegrees));
  }
  return tracks;
}

PooledEffectNode SowEffectSpiralTrailNode(const EffectSpiralTrailPattern& pattern, glm::vec3 anchor,
                                          std::int32_t effect_life_ticks,
                                          std::uint32_t& generator) {
  PooledEffectNode pooled = PooledNodeAt(PooledNodeShape::kSpiralTrail, anchor);
  pooled.node.life_ticks = effect_life_ticks;
  pooled.resource_paths.clear();
  for (const std::string_view path : pattern.textures) {
    pooled.resource_paths.emplace_back(path);
  }
  pooled.tracks = SeedEffectSpiralTrailTracks(anchor, generator);
  return pooled;
}

}  // namespace ro::render
