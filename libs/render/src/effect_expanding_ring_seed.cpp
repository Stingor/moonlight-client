#include "ro/render/effect_expanding_ring_seed.h"

#include <algorithm>
#include <cstddef>
#include <string>

#include "ro/core/sorted_table.h"
#include "ro/render/effect_expanding_ring.h"
#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

namespace ro::render {

std::span<const EffectExpandingRingPattern> EffectExpandingRingPatternsFor(
    std::uint32_t effect_id) {
  return core::RowsWithKey(EffectExpandingRingPatternTable(), effect_id,
                           &EffectExpandingRingPattern::effect_id);
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectExpandingRingTracks(
    std::int32_t age_ticks, std::uint32_t& generator) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  const float from_age = kExpandingRingRadiusPerAgeTick * static_cast<float>(age_ticks);
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    EffectTrack& track = tracks[rang];
    // Les drapeaux des quatre pistes : remis à zéro ensemble, puis rallumés un à
    // un. Les quatre finissent allumés.
    track.active = true;
    track.ticks = 0;
    track.degrees = kExpandingRingSweepDegrees;
    track.lift_key = kExpandingRingLiftKeys[rang];
    // LES DEUX TIRAGES, dans l'ordre du client : l'azimut, puis l'alpha.
    track.azimuth_degrees =
        static_cast<std::int16_t>(NextEffectNodeDraw(generator) % kEffectTrackFullTurnDegrees);
    track.alpha =
        static_cast<std::int16_t>(NextEffectNodeDraw(generator) % kExpandingRingAlphaDraw);
    // LA BASE D'ABORD, rangée en queue AVANT le tirage :
    // c'est elle que le mouvement relit comme seuil et comme renaissance.
    const float base = from_age + kExpandingRingBaseRadii[rang];
    track.tail[kExpandingRingTailOfBaseRadius] = base;
    const auto draw = static_cast<float>(NextEffectNodeDraw(generator) % kExpandingRingJitterDraw);
    track.radius_at_anchor =
        base + (kExpandingRingJitterSigns[rang] * draw * kExpandingRingJitterStep);
    track.tilt_degrees = kExpandingRingTiltAtBirthDegrees;
    // ⚠ LE STYLE ET L'ANCRE NE SONT PAS ÉCRITS : ils gardent ce que la naissance
    // du nœud leur laisse, zéro. L'ancre à
    // zéro n'est pas la sentinelle 1234,5 du ruban, qui ne la lit donc pas comme
    // un décalage tourné.
  }
  return tracks;
}

PooledEffectNode SowEffectExpandingRingNode(const EffectExpandingRingPattern& pattern,
                                            glm::vec3 owner_position, std::int32_t age_ticks,
                                            std::int32_t life_ticks, std::uint32_t& generator) {
  // ⚠ LE NŒUD NAÎT SUR L'INSTANCE, sans écart, et le mouvement l'y recale à
  // chaque tic. Voir `AdvanceTracksOf`.
  PooledEffectNode pooled = PooledNodeAt(PooledNodeShape::kExpandingRing, owner_position);
  pooled.node.life_ticks = std::max(life_ticks, 1);
  pooled.node.render_word = kExpandingRingRenderWord;
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.trail_variant = pattern.trail_variant;
  pooled.tracks = SeedEffectExpandingRingTracks(age_ticks, generator);
  return pooled;
}

}  // namespace ro::render
