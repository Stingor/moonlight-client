#include "ro/render/effect_tri_plane_seed.h"

#include <cstddef>
#include <string>

#include "ro/core/sorted_table.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"
#include "ro/render/effect_tri_plane.h"

#include "effect_draw_below.h"

namespace ro::render {
namespace {

[[nodiscard]] float DrawAngleDegrees(std::uint32_t& rng_state) {
  return static_cast<float>(DrawBelow(rng_state, kEffectTrackFullTurnDegrees));
}

// LE SEMIS DU 111 : deux pistes, et DOUZE angles par piste — six paires que le
// mouvement poursuit.
void SeedDrift(EffectTrack& track, std::uint8_t octet, std::uint32_t& rng_state) {
  track.active = true;
  track.lift_key = kDriftingTriPlaneStep;
  track.radius_at_anchor = static_cast<float>(DrawBelow(rng_state, kDriftingTriPlaneRadiusSpread)) *
                               kDriftingTriPlaneRadiusStep +
                           kDriftingTriPlaneRadiusFloor;
  track.ticks = 0;
  // ⚠ LE CLIENT ENTRELACE : pour chaque plan il pose le décalage, tire ses
  // QUATRE angles, puis met son alpha à zéro. L'ordre des tirages est celui-là,
  // et le changer changerait la figure de tout nœud rejoué depuis une graine.
  for (std::size_t plan = 0; plan < kTriPlanePlanes; ++plan) {
    SetEffectTriPlaneOffset(track, plan, glm::vec3{0.0F, 0.0F, 0.0F});
    for (std::size_t angle = 0; angle < 4; ++angle) {
      track.value[4 * plan + angle] = DrawAngleDegrees(rng_state);
    }
    track.value[kTriPlaneSlotOfFirstAlpha + plan] = 0.0F;
  }
  if (octet == kDriftingTriPlaneFixedColorByte) {
    track.value[kTriPlaneSlotOfColor] = kTriPlanePerPlaneColor;
  } else if (octet == kDriftingTriPlaneDrawnColorByte) {
    // ⚠ CELUI-CI TIRE SA TEINTE, entre un et trois : trois roses au hasard.
    track.value[kTriPlaneSlotOfColor] =
        static_cast<float>(DrawBelow(rng_state, kDriftingTriPlaneColorSpread) + 1);
  } else {
    track.value[kTriPlaneSlotOfColor] = kTriPlaneBlueColor;
  }
}

// LE SEMIS DU 115 : quatre pistes, et SIX angles par piste — un cap et une
// pente par plan, jamais poursuivis.
void SeedFlight(EffectTrack& track, std::uint32_t& rng_state) {
  track.active = true;
  track.lift_key = kFlyingTriPlaneStep;
  track.radius_at_anchor = static_cast<float>(DrawBelow(rng_state, kFlyingTriPlaneRadiusSpread)) *
                               kFlyingTriPlaneRadiusStep +
                           kFlyingTriPlaneRadiusFloor;
  track.ticks = 0;
  for (std::size_t plan = 0; plan < kTriPlanePlanes; ++plan) {
    SetEffectTriPlaneOffset(track, plan, glm::vec3{0.0F, 0.0F, 0.0F});
    track.value[4 * plan] = DrawAngleDegrees(rng_state);
    // ⚠ LA PENTE SE TIRE SUR UN DEMI-TOUR, le cap sur un tour entier.
    track.value[4 * plan + 2] =
        static_cast<float>(DrawBelow(rng_state, kFlyingTriPlanePitchSpread));
    track.value[kTriPlaneSlotOfFirstAlpha + plan] = kFlyingTriPlaneAlpha;
  }
  track.value[kTriPlaneSlotOfColor] =
      static_cast<float>(DrawBelow(rng_state, kFlyingTriPlaneColorSpread) + 1);
}

}  // namespace

std::span<const EffectTriPlanePattern> EffectTriPlanePatternsFor(std::uint32_t effect_id) {
  return core::RowsWithKey(EffectTriPlanePatternTable(), effect_id,
                           &EffectTriPlanePattern::effect_id);
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectTriPlaneTracks(
    const EffectTriPlanePattern& pattern, std::uint32_t& generator) {
  const bool derive = pattern.builder == EffectTriPlaneBuilder::kDrifting;
  const std::size_t vivantes = derive ? kDriftingTriPlaneTracks : kEffectTracksPerNode;
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  for (std::size_t rang = 0; rang < vivantes; ++rang) {
    if (derive) {
      SeedDrift(tracks[rang], pattern.style_byte, generator);
    } else {
      SeedFlight(tracks[rang], generator);
    }
  }
  return tracks;
}

PooledEffectNode SowEffectTriPlaneNode(const EffectTriPlanePattern& pattern, glm::vec3 anchor,
                                       std::int32_t effect_life_ticks, std::uint32_t& generator) {
  PooledEffectNode pooled = PooledNodeAt(pattern.builder == EffectTriPlaneBuilder::kDrifting
                                             ? PooledNodeShape::kDriftingTriPlane
                                             : PooledNodeShape::kFlyingTriPlane,
                                         anchor);
  pooled.node.life_ticks = effect_life_ticks;
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.tracks = SeedEffectTriPlaneTracks(pattern, generator);
  return pooled;
}

}  // namespace ro::render
