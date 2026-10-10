#include "ro/render/effect_homing_spark_seed.h"

#include <algorithm>
#include <cstddef>
#include <string>

#include "ro/render/effect_homing_spark.h"
#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

[[nodiscard]] std::int32_t DrawBelow(std::uint32_t& rng_state, std::int32_t exclusive_bound) {
  return static_cast<std::int32_t>(NextEffectNodeDraw(rng_state) %
                                   static_cast<std::uint32_t>(exclusive_bound));
}

}  // namespace

std::span<const EffectHomingSparkPattern> EffectHomingSparkPatternsFor(std::uint32_t effect_id) {
  const std::span<const EffectHomingSparkPattern> table = EffectHomingSparkPatternTable();
  const auto first =
      std::ranges::lower_bound(table, effect_id, {}, &EffectHomingSparkPattern::effect_id);
  const auto last =
      std::ranges::upper_bound(table, effect_id, {}, &EffectHomingSparkPattern::effect_id);
  return table.subspan(static_cast<std::size_t>(first - table.begin()),
                       static_cast<std::size_t>(last - first));
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectHomingSparkTracks(
    const EffectHomingSparkPattern& pattern, std::uint32_t& generator) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  for (EffectTrack& track : tracks) {
    track.active = true;
    // ⚠ LE RETARD EST TIRÉ PAR PISTE, et il va jusqu'à CENT tics : les quatre
    // étincelles ne partent pas ensemble.
    track.ticks = -DrawBelow(generator, kHomingSparkDelaySpread);
    track.radius_at_anchor = kHomingSparkBladeRadius;
    // ⚠ ELLE NAÎT INVISIBLE : le mouvement l'allume de cinq par tic.
    track.alpha = 0;
    track.tilt_degrees =
        static_cast<std::int16_t>(DrawBelow(generator, kEffectTrackFullTurnDegrees));
    track.azimuth_degrees =
        static_cast<std::int16_t>(DrawBelow(generator, kEffectTrackFullTurnDegrees));
    track.value[kHomingSparkSlotOfLift] = kHomingSparkLift;
    track.value[kHomingSparkSlotOfStep] = kHomingSparkStep;
    track.mark[kHomingSparkMarkOfPhase] = kHomingSparkApproachPhase;
    track.mark[kHomingSparkMarkOfSpinCount] = 0;
    track.mark[kHomingSparkMarkOfColor] = pattern.mark;
    // ⚠ L'ANCRE EST UN POINT DE LA SPHÈRE, dans le repère du CLIENT : les deux
    // angles la placent, et le décalage vertical s'y ajoute une première fois.
    // La figure l'ajoutera une SECONDE fois à chaque coin — c'est ce que le
    // client fait, et n'en garder qu'un abaisserait chaque lame.
    const float radial = kHomingSparkSphereRadius * CosOfDegrees(track.tilt_degrees);
    track.anchor = {radial * CosOfDegrees(track.azimuth_degrees),
                    radial * SinOfDegrees(track.azimuth_degrees) + kHomingSparkLift,
                    kHomingSparkSphereRadius * SinOfDegrees(track.tilt_degrees)};
  }
  return tracks;
}

PooledEffectNode SowEffectHomingSparkNode(const EffectHomingSparkPattern& pattern, glm::vec3 anchor,
                                          std::int32_t effect_life_ticks,
                                          std::uint32_t& generator) {
  PooledEffectNode pooled;
  pooled.shape = PooledNodeShape::kHomingSpark;
  pooled.ground_position = anchor;
  pooled.owner_position = anchor;
  pooled.node.life_ticks = effect_life_ticks;
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.tracks = SeedEffectHomingSparkTracks(pattern, generator);
  return pooled;
}

}  // namespace ro::render
