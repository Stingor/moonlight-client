#include "ro/render/effect_rising_quad.h"

#include <algorithm>
#include <cstddef>
#include <string>

#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

// LES MARQUES QUE LA FIGURE LIT sur la piste zéro — voir `effect_tinted_quad.h` —,
// et celle où le mouvement lit le mode, sur chaque piste.
constexpr std::size_t kMarkOfMode = 1;
constexpr std::size_t kMarkOfQuadDrawing = 2;
// La parure deux : bleue, coins échangés, mélange additif.
constexpr std::uint8_t kRisingQuadDrawingMark = 2;
// L'ANGLE PROPRE, que la figure lit pour faire respirer le rayon.
constexpr std::size_t kSlotOfSpinDegrees = 8;

// Les tics de départ, tirés sous cette borne.
constexpr std::int32_t kStartTicksBound = 11;
// LE RAYON DU MODE ZÉRO : un entier tiré sous six, plus cinq.
constexpr std::int32_t kWideRadiusDraws = 6;
constexpr float kWideRadiusFloor = 5.0F;
// CELUI DU MODE TOURNANT : un dixième d'un entier tiré sous vingt, plus trois.
constexpr std::int32_t kNarrowRadiusDraws = 20;
constexpr float kNarrowRadiusStep = 0.1F;
constexpr float kNarrowRadiusFloor = 3.0F;
constexpr float kFullTurnDegrees = static_cast<float>(kEffectTrackFullTurnDegrees);

[[nodiscard]] std::int32_t DrawBelow(std::uint32_t& rng_state, std::int32_t exclusive_bound) {
  return static_cast<std::int32_t>(NextEffectNodeDraw(rng_state) %
                                   static_cast<std::uint32_t>(exclusive_bound));
}

}  // namespace

std::span<const EffectRisingQuadPattern> EffectRisingQuadPatternsFor(std::uint32_t effect_id) {
  const std::span<const EffectRisingQuadPattern> table = EffectRisingQuadPatternTable();
  const auto first =
      std::ranges::lower_bound(table, effect_id, {}, &EffectRisingQuadPattern::effect_id);
  const auto last =
      std::ranges::upper_bound(table, effect_id, {}, &EffectRisingQuadPattern::effect_id);
  return table.subspan(static_cast<std::size_t>(first - table.begin()),
                       static_cast<std::size_t>(last - first));
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectRisingQuadTracks(
    const EffectRisingQuadPattern& pattern, glm::vec3 anchor, std::uint32_t& generator) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  const std::size_t live = pattern.mode == kRisingQuadTurningMode ? 1U : 2U;
  for (std::size_t rang = 0; rang < live; ++rang) {
    EffectTrack& track = tracks[rang];
    track.active = true;
    track.ticks = DrawBelow(generator, kStartTicksBound);
    // ⚠ UN MODE NI ZÉRO NI UN NE TIRE PAS DE RAYON : le carré reste un point.
    if (pattern.mode == 0) {
      track.radius_at_anchor =
          static_cast<float>(DrawBelow(generator, kWideRadiusDraws)) + kWideRadiusFloor;
    } else if (pattern.mode == kRisingQuadTurningMode) {
      track.radius_at_anchor =
          static_cast<float>(DrawBelow(generator, kNarrowRadiusDraws)) * kNarrowRadiusStep +
          kNarrowRadiusFloor;
    }
    track.alpha = 0;
    track.anchor = {anchor.x, 0.0F, anchor.z};
    track.degrees = kRisingQuadStartDegrees;
    track.mark[kMarkOfMode] = pattern.mode;
    track.mark[kMarkOfQuadDrawing] = kRisingQuadDrawingMark;
  }
  return tracks;
}

void AdvanceRisingQuadTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                             float owner_height, std::int16_t camera_turn_degrees,
                             std::int16_t camera_tilt_degrees) {
  tracks[0].style = camera_turn_degrees;
  tracks[1].style = camera_tilt_degrees;
  for (EffectTrack& track : tracks) {
    if (!track.active) {
      continue;
    }
    ++track.ticks;
    if (track.ticks <= kRisingQuadLastTickOfRise) {
      track.alpha = static_cast<std::int16_t>(track.alpha + kRisingQuadAlphaRise);
    } else {
      track.alpha = static_cast<std::int16_t>(track.alpha - kRisingQuadAlphaFall);
      if (track.alpha <= 0) {
        // ÉTEINTE, ELLE REPART du porteur, et son compte aussi.
        track.alpha = 0;
        track.ticks = 0;
        track.anchor.y = owner_height;
        if (track.mark[kMarkOfMode] != 0) {
          track.anchor.y += kRisingQuadTurningLift;
        }
      }
      // ⚠ ET ELLE MONTE DÉJÀ À L'AVANCE DE SA REPRISE, sans attendre la suivante.
      track.anchor.y += kRisingQuadClimbPerTick;
    }
    float& own = track.value[kSlotOfSpinDegrees];
    own += kRisingQuadSpinPerTick;
    if (own >= kFullTurnDegrees) {
      own -= kFullTurnDegrees;
    }
    if (track.mark[kMarkOfMode] == kRisingQuadTurningMode) {
      TurnOnward(track.degrees, kRisingQuadTurnPerTick);
    }
  }
}

PooledEffectNode SowEffectRisingQuadNode(const EffectRisingQuadPattern& pattern, glm::vec3 anchor,
                                         std::int32_t effect_life_ticks, std::uint32_t& generator) {
  PooledEffectNode pooled;
  pooled.shape = PooledNodeShape::kRisingQuads;
  pooled.ground_position = anchor;
  pooled.owner_position = anchor;
  pooled.node.life_ticks = effect_life_ticks;
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.tracks = SeedEffectRisingQuadTracks(pattern, anchor, generator);
  return pooled;
}

}  // namespace ro::render
