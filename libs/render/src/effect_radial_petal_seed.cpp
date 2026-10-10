#include "ro/render/effect_radial_petal_seed.h"

#include <algorithm>
#include <string>

#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_radial_petal.h"
#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

[[nodiscard]] std::int32_t DrawBelow(std::uint32_t& rng_state, std::int32_t exclusive_bound) {
  return static_cast<std::int32_t>(NextEffectNodeDraw(rng_state) %
                                   static_cast<std::uint32_t>(exclusive_bound));
}

}  // namespace

std::int32_t RadialPetalSowTick(std::int16_t variant) {
  if (variant == kRadialPetalLateVariant) {
    return kRadialPetalLateSowsAtTick;
  }
  if (variant == kRadialPetalAtOnceVariant) {
    return 0;
  }
  return variant;
}

std::span<const EffectRadialPetalPattern> EffectRadialPetalPatternsFor(std::uint32_t effect_id) {
  const std::span<const EffectRadialPetalPattern> table = EffectRadialPetalPatternTable();
  const auto [first, last] =
      std::ranges::equal_range(table, effect_id, {}, &EffectRadialPetalPattern::effect_id);
  return {first, last};
}

std::array<EffectTrack, kEffectTracksPerNode> SeedRadialPetalTracks(
    std::int16_t base_heading_degrees, std::uint32_t& generator) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  for (EffectTrack& track : tracks) {
    track.active = true;
    // L'ORDRE DES TIRAGES est celui du client : taille, cap, retard, puis les
    // quatre angles que personne ne lit.
    track.radius_at_anchor =
        (static_cast<float>(DrawBelow(generator, kRadialPetalSizeSpread)) * kRadialPetalSizeStep) +
        kRadialPetalSizeFloor;
    track.alpha = 0;
    std::int32_t cap = base_heading_degrees + DrawBelow(generator, kRadialPetalHeadingSpread);
    if (cap >= kEffectTrackFullTurnDegrees) {
      cap -= kEffectTrackFullTurnDegrees;
    }
    track.ticks = -DrawBelow(generator, kRadialPetalDelaySpread);
    track.tilt_degrees = static_cast<std::int16_t>(cap);
    track.lift_key = kRadialPetalSpeed;
    track.tail[kRadialPetalSlotOfSideways] = CosOfDegrees(cap) * kRadialPetalStartRing;
    track.tail[kRadialPetalSlotOfForward] = SinOfDegrees(cap) * kRadialPetalStartRing;
    track.tail[kRadialPetalSlotOfDepthBelow] = 0.0F;
    // ⚠ LA BASE DU SAUT, EN REPÈRE DU CLIENT : seul notre mouvement la relit,
    // pour en tirer la hauteur de la queue — qui est, elle aussi, en repère du
    // client jusqu'à la figure.
    track.anchor.y = kRadialPetalBaseDepthBelow;
    // LES QUATRE ANGLES SUIVANTS : ni le mouvement ni la figure ne les lisent.
    // Tirés quand même, pour que la suite du générateur garde l'ordre.
    for (std::size_t angle = 0; angle < kRadialPetalUnreadAngles; ++angle) {
      track.value[angle] = static_cast<float>(DrawBelow(generator, kEffectTrackFullTurnDegrees));
    }
  }
  return tracks;
}

PooledEffectNode SowRadialPetalNode(const EffectRadialPetalPattern& pattern, glm::vec3 anchor,
                                    std::int32_t effect_life_ticks, std::uint32_t& generator) {
  PooledEffectNode pooled;
  pooled.shape = PooledNodeShape::kRadialPetal;
  pooled.ground_position = anchor;
  pooled.owner_position = anchor;
  pooled.node.render_word = kRadialPetalRenderWord;
  pooled.node.life_ticks = std::max(effect_life_ticks, 1);
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.tracks = SeedRadialPetalTracks(pattern.base_heading_degrees, generator);
  return pooled;
}

}  // namespace ro::render
