#include "ro/render/effect_warp_ring_seed.h"

#include <algorithm>
#include <string>

#include "ro/core/sorted_table.h"
#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

// UN ANNEAU NAÎT ÉTEINT : c'est le mouvement qui l'allume, d'un pas par tic.
// Le semer visible le ferait paraître d'un coup au bord extérieur.
constexpr std::int16_t kStartsInvisible = 0;

// ⚠ LES VINGT ET UNE MARQUES VALENT ZÉRO, à l'inverse des anneaux qui pulsent.
// Elles ne sont pas décoratives : le mouvement n'écrit la longueur d'un segment
// QUE là où la marque est nulle. Les poser à un figerait les vingt et un
// segments à leur longueur de naissance, et l'anneau cesserait de respirer.
constexpr std::uint8_t kMarkAtBirth = 0;

// L'inclinaison que le client tire du rayon, arrondie comme lui : il tronque,
// il n'arrondit pas (`cvttss2si`).
[[nodiscard]] std::int16_t TiltFromRadius(float radius) {
  return static_cast<std::int16_t>(kWarpRingTiltBaseDegrees - kWarpRingTiltPerRadius * radius);
}

}  // namespace

std::span<const EffectWarpRingPattern> EffectWarpRingPatternsFor(std::uint32_t effect_id) {
  return core::RowsWithKey(EffectWarpRingPatternTable(), effect_id,
                           &EffectWarpRingPattern::effect_id);
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectWarpRingTracks(
    const EffectWarpRingPattern& pattern) {
  std::array<EffectTrack, kEffectTracksPerNode> node_tracks;
  for (std::size_t rang = 0; rang < kWarpRingCount && rang < kEffectTracksPerNode; ++rang) {
    EffectTrack& track = node_tracks[rang];
    track.active = true;
    track.ticks = 0;
    track.degrees = kWarpRingSweepDegrees;
    track.azimuth_degrees = pattern.azimuths[rang];
    track.radius_at_anchor = pattern.radii[rang];
    // ⚠ LA CLÉ EST LE RAYON, recopiée au semis comme à chaque tic.
    track.lift_key = pattern.radii[rang];
    track.tilt_degrees = TiltFromRadius(pattern.radii[rang]);
    track.alpha = kStartsInvisible;
    track.value.fill(pattern.radii[rang]);
    track.mark.fill(kMarkAtBirth);
  }
  return node_tracks;
}

PooledEffectNode SowEffectWarpRingNode(const EffectWarpRingPattern& pattern, glm::vec3 anchor,
                                       std::int32_t effect_life_ticks) {
  PooledEffectNode pooled = PooledNodeAt(PooledNodeShape::kWarpRings, anchor);
  pooled.node.life_ticks = effect_life_ticks;
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.trail_variant = pattern.trail_variant;
  pooled.tracks = SeedEffectWarpRingTracks(pattern);
  return pooled;
}

void AdvanceWarpRingTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                           std::int32_t node_life_ticks) {
  for (EffectTrack& track : tracks) {
    if (!track.active) {
      continue;
    }
    ++track.ticks;
    track.radius_at_anchor -= kWarpRingRadiusStepPerTick;
    // 🔴 LE REMBOBINAGE EST CE QUI REND LA FIGURE PERPÉTUELLE. Passé le centre,
    // l'anneau repart du bord extérieur et REPART ÉTEINT : c'est ce qui le fait
    // renaître en fondu au lieu de réapparaître d'un bloc.
    if (!(track.radius_at_anchor > 0.0F)) {
      track.radius_at_anchor = kWarpRingRewindRadius;
      track.alpha = kStartsInvisible;
    }
    track.tilt_degrees = TiltFromRadius(track.radius_at_anchor);
    track.lift_key = track.radius_at_anchor;
    // L'alpha retombe dans les derniers tics de la VIE DU NŒUD, non de la
    // piste : les quatre s'éteignent ensemble quand l'effet s'en va.
    if (track.ticks >= node_life_ticks - kWarpRingFadeOutTicks) {
      track.alpha = std::max<std::int16_t>(0, track.alpha - kWarpRingAlphaFallPerTick);
    } else if (track.alpha < kWarpRingAlphaCeiling) {
      track.alpha = static_cast<std::int16_t>(track.alpha + kWarpRingAlphaRisePerTick);
    }
    // ⚠ SEULS LES SEGMENTS NON MARQUÉS SUIVENT LA CLÉ. Le client teste la
    // marque avant d'écrire, et un segment marqué garde sa longueur.
    for (std::size_t segment = 0; segment < kEffectTrackSlots; ++segment) {
      if (track.mark[segment] != 0) {
        continue;
      }
      track.value[segment] = std::max(0.0F, track.lift_key);
    }
  }
}

}  // namespace ro::render
