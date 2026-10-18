#include "ro/render/effect_spike_beam_seed.h"

#include <string>

#include "ro/core/sorted_table.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_spike_beam.h"
#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

constexpr std::int16_t kFullAlpha = 255;

}  // namespace

std::span<const EffectSpikeBeamPattern> EffectSpikeBeamPatternsFor(std::uint32_t effect_id) {
  return core::RowsWithKey(EffectSpikeBeamPatternTable(), effect_id,
                           &EffectSpikeBeamPattern::effect_id);
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectSpikeBeamTracks(std::int32_t tick) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    EffectTrack& track = tracks[rang];
    track.active = true;
    track.ticks = 0;
    track.alpha = kFullAlpha;
    track.radius_at_anchor = kSpikeBeamFirstReach;
    track.lift_key = kSpikeBeamStep;
    track.azimuth_degrees = 0;
    track.value[kSlotOfSpikeBeamHeight] = kSpikeBeamHeight;
    // L'INDICE D'IMAGE PART À ZÉRO : c'est le mouvement qui le fera tourner.
    track.value[kSlotOfSpikeBeamImage] = 0.0F;
    // ⚠ LE RANG ET LE TIC SE COMPOSENT, exactement comme chez le quartier de
    // tarte : le client lit son compteur de tics dans l'instance et le multiplie
    // par QUATRE avant d'y ajouter le rang. Douze faisceaux ferment le tour en
    // trois tics.
    const auto place =
        static_cast<std::int32_t>(rang) + static_cast<std::int32_t>(kEffectTracksPerNode) * tick;
    track.tilt_degrees = static_cast<std::int16_t>(kPieWedgeDegreesPerStep * place);
  }
  return tracks;
}

PooledEffectNode SowEffectSpikeBeamNode(const EffectSpikeBeamPattern& pattern, glm::vec3 anchor,
                                        std::int32_t tick, std::int32_t effect_life_ticks) {
  PooledEffectNode pooled = PooledNodeAt(PooledNodeShape::kSpikeBeam, anchor);
  pooled.node.life_ticks = effect_life_ticks;
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.tracks = SeedEffectSpikeBeamTracks(tick);
  return pooled;
}

}  // namespace ro::render
