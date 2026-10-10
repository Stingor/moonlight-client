#include "ro/render/effect_ring_starburst_seed.h"

#include <algorithm>
#include <cstddef>
#include <string>

#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

namespace ro::render {

std::span<const EffectRingStarburstPattern> EffectRingStarburstPatternsFor(
    std::uint32_t effect_id) {
  const std::span<const EffectRingStarburstPattern> table = EffectRingStarburstPatternTable();
  const auto first =
      std::ranges::lower_bound(table, effect_id, {}, &EffectRingStarburstPattern::effect_id);
  const auto last =
      std::ranges::upper_bound(table, effect_id, {}, &EffectRingStarburstPattern::effect_id);
  return table.subspan(static_cast<std::size_t>(first - table.begin()),
                       static_cast<std::size_t>(last - first));
}

std::int32_t EffectRingStarburstLastSowTick(std::uint32_t effect_id) {
  std::int32_t last = 0;
  for (const EffectRingStarburstPattern& pattern : EffectRingStarburstPatternsFor(effect_id)) {
    last = std::max(last, pattern.sown_at_tick);
  }
  return last;
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectRingStarburstTracks(
    const EffectRingStarburstPattern& pattern) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  for (std::size_t rank = 0; rank < tracks.size(); ++rank) {
    EffectTrack& track = tracks[rank];
    const auto r = static_cast<std::int16_t>(rank);
    track.active = true;
    track.ticks = 0;
    track.alpha = 0;
    track.style = 0;
    track.degrees = kRingStarburstFullSweep;
    track.azimuth_degrees = static_cast<std::int16_t>(
        (pattern.first_azimuth_degrees + kRingStarburstDegreesPerTrack * r) %
        kEffectTrackFullTurnDegrees);
    track.tilt_degrees = pattern.seed_tilt_degrees[rank];
    // LE CYCLE COMMENCE UN QUART PLUS LOIN À CHAQUE PISTE, et c'est le rayon du
    // cercle intérieur ; le semis en recopie la valeur dans la clé, que le
    // premier mouvement remplace.
    track.radius_at_anchor =
        pattern.first_cycle + kRingStarburstCyclePerTrack * static_cast<float>(rank);
    track.lift_key = track.radius_at_anchor;
    track.anchor = {0.0F, 0.0F, 0.0F};
    // LE RUBAN NAÎT SANS LARGEUR : c'est le mouvement qui l'ouvre.
    track.value.fill(0.0F);
    track.mark.fill(0);
  }
  return tracks;
}

PooledEffectNode SowEffectRingStarburstNode(const EffectRingStarburstPattern& pattern,
                                            glm::vec3 anchor, std::int32_t effect_life_ticks) {
  PooledEffectNode pooled;
  pooled.shape = PooledNodeShape::kRingStarburst;
  pooled.ground_position = anchor;
  pooled.owner_position = anchor;
  pooled.node.life_ticks = effect_life_ticks;
  pooled.trail_variant =
      static_cast<std::uint8_t>(static_cast<std::int32_t>(pattern.trail_variant));
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.tracks = SeedEffectRingStarburstTracks(pattern);
  return pooled;
}

void AdvanceRingStarburstTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks) {
  for (EffectTrack& track : tracks) {
    if (!track.active) {
      continue;
    }
    ++track.ticks;
    track.radius_at_anchor += kRingStarburstGrowthPerTick;
    // Le cycle bouclé, le cercle repart de trois, invisible — et une piste qui
    // a vécu plus de cinquante tics s'éteint là.
    if (track.radius_at_anchor >= kRingStarburstOuterRadius) {
      track.radius_at_anchor = kRingStarburstInnerRadius;
      track.alpha = 0;
      if (track.ticks > kRingStarburstDiesAfterTick) {
        track.active = false;
      }
    }
    const float travelled = track.radius_at_anchor - kRingStarburstInnerRadius;
    float width = travelled;
    if (travelled > kRingStarburstHalfCycle) {
      width = kRingStarburstFullCycle - travelled;
      if (track.alpha > 0) {
        track.alpha = static_cast<std::int16_t>(track.alpha - kRingStarburstAlphaStep);
      }
    } else {
      track.alpha = static_cast<std::int16_t>(track.alpha + kRingStarburstAlphaStep);
    }
    track.lift_key = width * kRingStarburstWidthPerUnit;
    track.tilt_degrees = static_cast<std::int16_t>(static_cast<std::int32_t>(
        kRingStarburstUprightDegrees - travelled * kRingStarburstTiltPerUnit));
    // ⚠ UNE LARGEUR NÉGATIVE — le cercle n'a pas encore atteint trois — se lit
    // ZÉRO : le ruban n'a pas d'aire tant que le cycle n'a pas commencé.
    for (std::size_t segment = 0; segment < track.value.size(); ++segment) {
      if (track.mark[segment] == 0) {
        track.value[segment] = std::max(track.lift_key, 0.0F);
      }
    }
  }
}

}  // namespace ro::render
