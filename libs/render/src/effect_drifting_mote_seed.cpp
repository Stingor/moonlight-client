#include "ro/render/effect_drifting_mote_seed.h"

#include <string>

#include "ro/core/sorted_table.h"
#include "ro/render/effect_drifting_mote.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

#include "effect_draw_below.h"
#include "effect_track_angles.h"

namespace ro::render {
namespace {

// COMBIEN D'ANGLES chaque piste porte : deux paires, donc quatre cases.
constexpr std::size_t kAnglesPerTrack = 4;

}  // namespace

std::span<const EffectDriftingMotePattern> EffectDriftingMotePatternsFor(std::uint32_t effect_id) {
  return core::RowsWithKey(EffectDriftingMotePatternTable(), effect_id,
                           &EffectDriftingMotePattern::effect_id);
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectDriftingMoteTracks(
    const EffectDriftingMotePattern& pattern, std::uint32_t& generator) {
  const bool solitaire = pattern.variant == kDriftingMoteLoneVariant;
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  if (!solitaire && pattern.variant != kDriftingMoteSwarmVariant) {
    // ⚠ TOUT AUTRE ENTIER BÂTIT UN NŒUD SANS AUCUNE PISTE VIVE, et le client
    // fait exactement cela : il éteint les quatre puis n'entre dans aucune
    // branche. Retomber sur l'une des deux inventerait une figure.
    return tracks;
  }

  // ⚠ LE SOLITAIRE N'ALLUME QUE SA PREMIÈRE PISTE. Les trois autres restent
  // éteintes, et le client ne les touche même pas.
  const std::size_t vivantes = solitaire ? 1 : kEffectTracksPerNode;
  for (std::size_t rang = 0; rang < vivantes; ++rang) {
    EffectTrack& track = tracks[rang];
    track.active = true;
    track.lift_key = solitaire
                         ? static_cast<float>(DrawBelow(generator, kDriftingMoteLoneSpeedSpread)) *
                                   kDriftingMoteLoneSpeedStep +
                               kDriftingMoteLoneSpeedFloor
                         : static_cast<float>(DrawBelow(generator, kDriftingMoteSpeedSpread)) *
                                   kDriftingMoteSwarmSpeedStep +
                               kDriftingMoteSwarmSpeedFloor;
    track.radius_at_anchor =
        solitaire ? static_cast<float>(DrawBelow(generator, kDriftingMoteRadiusSpread)) *
                            kDriftingMoteLoneRadiusStep +
                        kDriftingMoteLoneRadiusFloor
                  : static_cast<float>(DrawBelow(generator, kDriftingMoteRadiusSpread)) *
                            kDriftingMoteSwarmRadiusStep +
                        kDriftingMoteSwarmRadiusFloor;
    // ⚠ LE RETARD EST TIRÉ PAR PISTE : la volée ne part pas d'un bloc.
    track.ticks = -DrawBelow(generator, kDriftingMoteDelaySpread);
    // ⚠ IL NAÎT INVISIBLE : c'est le mouvement qui l'allume, de cinq par tic.
    track.alpha = 0;
    // ⚠ LA HAUTEUR EST NÉGATIVE DANS LE REPÈRE DU CLIENT, donc AU-DESSUS du
    // nœud chez nous. La figure la retourne au dernier moment ; l'inverser
    // ici la retournerait deux fois.
    track.tail[kDriftingMoteSlotOfDepthBelow] =
        -static_cast<float>(DrawBelow(generator, kDriftingMoteHeightSpread));
    // ⚠ ET LE GRAIN NAÎT SUR UN CERCLE, pas au centre : un angle tiré, et un
    // rayon fixe. Le poser au centre ferait partir les quatre du même point.
    const std::int32_t cap = DrawBelow(generator, kEffectTrackFullTurnDegrees);
    const float circle = solitaire ? kDriftingMoteLoneRing : kDriftingMoteSwarmRing;
    track.tail[kDriftingMoteSlotOfSideways] = CosOfDegrees(cap) * circle;
    track.tail[kDriftingMoteSlotOfForward] = SinOfDegrees(cap) * circle;
    for (std::size_t angle = 0; angle < kAnglesPerTrack; ++angle) {
      track.value[angle] = static_cast<float>(DrawBelow(generator, kEffectTrackFullTurnDegrees));
    }
    track.mark[0] = solitaire ? kDriftingMoteLoneMark : kDriftingMoteSwarmMark;
  }
  return tracks;
}

PooledEffectNode SowEffectDriftingMoteNode(const EffectDriftingMotePattern& pattern,
                                           glm::vec3 anchor, std::int32_t effect_life_ticks,
                                           std::uint32_t& generator) {
  PooledEffectNode pooled = PooledNodeAt(PooledNodeShape::kDriftingMote, anchor);
  pooled.node.life_ticks = effect_life_ticks;
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.tracks = SeedEffectDriftingMoteTracks(pattern, generator);
  return pooled;
}

}  // namespace ro::render
