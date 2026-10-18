#include "ro/render/effect_spin_ribbon_seed.h"

#include <cstddef>
#include <string>

#include "ro/core/sorted_table.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_spin_ribbon.h"
#include "ro/render/effect_track.h"

#include "effect_draw_below.h"

namespace ro::render {

std::span<const EffectSpinRibbonPattern> EffectSpinRibbonPatternsFor(std::uint32_t effect_id) {
  return core::RowsWithKey(EffectSpinRibbonPatternTable(), effect_id,
                           &EffectSpinRibbonPattern::effect_id);
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectSpinRibbonTracks(
    const EffectSpinRibbonPattern& pattern, std::uint32_t& generator) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    EffectTrack& track = tracks[rang];
    track.active = true;
    // ⚠ LE RETARD EST LE PREMIER TIRAGE DU 91, et l'autre n'en fait aucun ici :
    // le glisser pour les deux décalerait toute la suite du générateur.
    track.ticks = pattern.rising ? -DrawBelow(generator, kRisingSpinRibbonDelaySpread) : 0;
    track.lift_key = static_cast<float>(DrawBelow(generator, kSpinRibbonHalfWidthSpread)) *
                         kSpinRibbonHalfWidthStep +
                     (pattern.rising ? kRisingSpinRibbonHalfWidthFloor : kSpinRibbonHalfWidthFloor);
    track.radius_at_anchor = pattern.rising ? kRisingSpinRibbonRadius : kSpinRibbonRadius;
    track.value[kSpinRibbonSlotOfHeight] =
        pattern.rising ? kRisingSpinRibbonHeight : kSpinRibbonHeight;
    track.azimuth_degrees =
        static_cast<std::int16_t>(DrawBelow(generator, kEffectTrackFullTurnDegrees));
    // ⚠ QUARANTE-CINQ DEGRÉS D'ÉCART D'UNE PISTE À L'AUTRE : c'est ce qui fait
    // un bouquet. Leur donner la même inclinaison les superposerait.
    track.tilt_degrees = static_cast<std::int16_t>(
        pattern.base_tilt_degrees + kSpinRibbonTiltSpacing * static_cast<std::int16_t>(rang));
    track.alpha = pattern.rising ? kRisingSpinRibbonAlpha : kSpinRibbonAlpha;
    track.degrees = static_cast<std::int16_t>(DrawBelow(generator, kSpinRibbonSweepSpread) +
                                              kSpinRibbonSweepFloor);
    track.value[kSpinRibbonSlotOfSpinSpeed] =
        static_cast<float>(DrawBelow(generator, kSpinRibbonSpinSpread)) + kSpinRibbonSpinFloor;
  }
  return tracks;
}

PooledEffectNode SowEffectSpinRibbonNode(const EffectSpinRibbonPattern& pattern, glm::vec3 anchor,
                                         std::int32_t effect_life_ticks, std::uint32_t& generator) {
  PooledEffectNode pooled = PooledNodeAt(
      pattern.rising ? PooledNodeShape::kRisingSpinRibbon : PooledNodeShape::kSpinRibbon, anchor);
  pooled.node.life_ticks = effect_life_ticks;
  pooled.trail_variant = pattern.trail_variant;
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.tracks = SeedEffectSpinRibbonTracks(pattern, generator);
  return pooled;
}

}  // namespace ro::render
