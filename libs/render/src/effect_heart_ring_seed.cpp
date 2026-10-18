#include "ro/render/effect_heart_ring_seed.h"

#include <string>

#include "ro/core/sorted_table.h"
#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"
#include "ro/render/effect_track_figures.h"

namespace ro::render {
namespace {

// LES DEUX ANNEAUX, dans l'ordre où le bâtisseur les pose : le petit d'abord.
//
// ⚠ LEURS INCLINAISONS NE SONT PAS LA MÊME — quatre-vingt-six contre
// quatre-vingt-dix —, et c'est ce qui empêche les deux rubans de se confondre :
// le premier s'évase à peine, le second est droit.
constexpr std::array<EffectHeartRingSeed, 2> kRings = {{
    {12.0F, 2.3F, 86},
    {25.0F, 2.0F, 90},
}};

// L'ORDONNÉE QUI N'EN EST PAS UNE : la sentinelle du ruban. Elle vit ici parce
// que c'est le bâtisseur qui la pose, et `effect_track_figures.h` qui la lit.
constexpr float kAnchorHeightThatMeansTurn = 1234.5F;

}  // namespace

std::span<const EffectHeartRingSeed> EffectHeartRingSeeds() { return kRings; }

std::span<const EffectHeartRingPattern> EffectHeartRingPatternsFor(std::uint32_t effect_id) {
  return core::RowsWithKey(EffectHeartRingPatternTable(), effect_id,
                           &EffectHeartRingPattern::effect_id);
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectHeartRingTracks(
    const EffectHeartRingPattern& pattern, std::int32_t effect_life_ticks,
    std::uint32_t& azimuth_seed) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  for (std::size_t rang = 0; rang < kRings.size(); ++rang) {
    const EffectHeartRingSeed& seed = kRings[rang];
    EffectTrack& track = tracks[rang];
    track.active = true;
    track.ticks = 0;
    // ⚠ L'ALPHA NAÎT NUL : c'est le mouvement qui l'allume, sur seize tics.
    track.alpha = 0;
    track.degrees = kHeartRingSweepDegrees;
    track.lift_key = seed.lift_key;
    track.radius_at_anchor = seed.radius_at_anchor;
    track.tilt_degrees = seed.tilt_degrees;
    // ⚠ LE STYLE EST UNE DURÉE, et le bâtisseur la tire de la vie de l'effet.
    track.style = static_cast<std::int16_t>(effect_life_ticks - kHeartRingStyleBeforeEnd);
    track.azimuth_degrees = static_cast<std::int16_t>(
        NextEffectNodeDraw(azimuth_seed) % static_cast<std::uint32_t>(kHeartRingSweepDegrees));
    // ⚠ L'ANCRE PORTE LA SENTINELLE, et non une hauteur : la figure tourne son
    // abscisse et sa profondeur du cap de la caméra.
    track.anchor = {pattern.offset_x, kAnchorHeightThatMeansTurn, pattern.offset_z};
    // ⚠ LES VINGT ET UNE MARQUES À TRENTE-TROIS : c'est la première que le
    // mouvement lit, mais le bâtisseur les pose toutes, et les reproduire
    // toutes coûte une boucle et évite d'avoir à justifier pourquoi une seule.
    track.mark.fill(kHeartRingCameraMark);
  }
  return tracks;
}

PooledEffectNode SowEffectHeartRingNode(const EffectHeartRingPattern& pattern, glm::vec3 anchor,
                                        std::int32_t effect_life_ticks,
                                        std::uint32_t& azimuth_seed) {
  PooledEffectNode pooled = PooledNodeAt(PooledNodeShape::kHeartRings, anchor);
  pooled.node.life_ticks = effect_life_ticks;
  pooled.trail_variant = kHeartRingTrailVariant;
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.tracks = SeedEffectHeartRingTracks(pattern, effect_life_ticks, azimuth_seed);
  return pooled;
}

}  // namespace ro::render
