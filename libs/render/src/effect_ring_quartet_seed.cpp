#include "ro/render/effect_ring_quartet_seed.h"

#include <algorithm>
#include <cstddef>
#include <string>

#include "ro/core/sorted_table.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

// DE COMBIEN LE RAYON GRANDIT d'une piste à la suivante. Les deux bâtisseurs
// emploient le même demi.
constexpr float kRadiusPerTrack = 0.5F;

// LA LONGUEUR DU RUBAN à ce tic, pour une portée donnée.
[[nodiscard]] float LengthAtTick(std::int32_t ticks, float reach) {
  if (ticks < kRingQuartetOpensAtTick) {
    return 0.0F;
  }
  if (ticks > kRingQuartetOpenedAtTick) {
    return reach;
  }
  // ⚠ UN SINUS DE ZÉRO À QUATRE-VINGT-DIX DEGRÉS, un degré par tic. Le client
  // indexe sa table à `tics - deux cents`.
  return SinOfDegrees(ticks - kRingQuartetOpensAtTick) * reach;
}

}  // namespace

std::span<const EffectRingQuartetPattern> EffectRingQuartetPatternsFor(std::uint32_t effect_id) {
  return core::RowsWithKey(EffectRingQuartetPatternTable(), effect_id,
                           &EffectRingQuartetPattern::effect_id);
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectRingQuartetTracks(
    const EffectRingQuartetPattern& pattern) {
  std::array<EffectTrack, kEffectTracksPerNode> node_tracks;
  for (std::size_t rang = 0; rang < node_tracks.size(); ++rang) {
    EffectTrack& track = node_tracks[rang];
    const auto r = static_cast<std::int32_t>(rang);
    track.active = true;
    // ⚠ LE COMPTEUR AVANCE D'UNE PISTE À L'AUTRE chez le premier bâtisseur :
    // elles s'ouvrent l'une après l'autre, et c'est ce décalage qui fait la
    // vague. Le second les part toutes au même tic.
    track.ticks = pattern.first_tick + pattern.ticks_per_track * r;
    track.degrees = kRingQuartetFullSweep;
    track.lift_key = kRingQuartetReach;
    track.azimuth_degrees =
        static_cast<std::int16_t>(kRingQuartetDegreesPerTrack * static_cast<std::int16_t>(rang));
    track.alpha = pattern.alpha;
    track.radius_at_anchor = pattern.first_radius + kRadiusPerTrack * static_cast<float>(rang);
    track.tilt_degrees = static_cast<std::int16_t>(
        pattern.first_tilt + pattern.tilt_per_track * static_cast<std::int16_t>(rang));
    track.style = pattern.style;
    track.anchor = {0.0F, 0.0F, 0.0F};
    // LE RUBAN NAÎT SANS LONGUEUR : c'est le mouvement qui l'ouvre.
    track.value.fill(0.0F);
    track.mark.fill(0);
  }
  return node_tracks;
}

PooledEffectNode SowEffectRingQuartetNode(const EffectRingQuartetPattern& pattern, glm::vec3 anchor,
                                          std::int32_t effect_life_ticks) {
  PooledEffectNode pooled = PooledNodeAt(PooledNodeShape::kRingQuartet, anchor);
  pooled.node.life_ticks = effect_life_ticks;
  pooled.trail_variant =
      static_cast<std::uint8_t>(static_cast<std::int32_t>(pattern.trail_variant));
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.tracks = SeedEffectRingQuartetTracks(pattern);
  return pooled;
}

void AdvanceRingQuartetTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks) {
  for (EffectTrack& track : tracks) {
    if (!track.active) {
      continue;
    }
    ++track.ticks;

    TurnOnward(track.azimuth_degrees, 1);

    // ⚠ SEUL LE STYLE UN VOIT SON ALPHA TOMBER. Les autres gardent le leur
    // jusqu'à ce que la vie du nœud les emporte.
    if (track.style == kRingQuartetStyleThatFades && track.ticks > kRingQuartetFadeAfterTick) {
      track.alpha = std::max<std::int16_t>(track.alpha - 1, 0);
    }

    // ⚠ ET LA PORTÉE SE REFERME passé le huit centième tic : la jupe rentre
    // d'une unité par tic. C'est la seconde fin de cette figure, et elle est
    // indépendante de l'alpha.
    if (track.ticks >= kRingQuartetClosesAfterTick) {
      track.lift_key = std::max(track.lift_key - kRingQuartetClosePerTick, 0.0F);
    }

    // ⚠ LES VINGT ET UNE LONGUEURS REÇOIVENT LA MÊME VALEUR : le client
    // recalcule la sienne à chaque case sans jamais lire l'index. La jupe est
    // UNIFORME.
    const float length = std::max(LengthAtTick(track.ticks, track.lift_key), 0.0F);
    track.value.fill(length);
  }
}

}  // namespace ro::render
