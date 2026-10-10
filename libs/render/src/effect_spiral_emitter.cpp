#include "ro/render/effect_spiral_emitter.h"

#include "ro/render/effect_quad_rotation.h"
#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

constexpr std::size_t kTrackThatTakesTheTurn = 0;
constexpr std::size_t kTrackThatTakesTheTilt = 1;

// LE PREMIER COIN DU CARRÉ, à quarante-cinq degrés : c'est le seul que
// l'épreuve regarde.
constexpr std::int32_t kFirstCornerDegrees = 45;

// L'ORDONNÉE DU PREMIER COIN, déporté de l'écart, dans le repère DESCENDANT du
// client.
[[nodiscard]] float FirstCornerHeight(const EffectTrack& track, std::int16_t turn_degrees,
                                      std::int16_t tilt_degrees) {
  std::array<glm::vec3, 4> corners;
  for (std::size_t corner = 0; corner < corners.size(); ++corner) {
    const std::int32_t angle =
        kFirstCornerDegrees + (kEffectTrackFullTurnDegrees / 4 * static_cast<std::int32_t>(corner));
    corners[corner] = {track.radius_at_anchor * CosOfDegrees(angle), 0.0F,
                       track.radius_at_anchor * SinOfDegrees(angle)};
  }
  RotateEffectQuadCorners(corners, turn_degrees, tilt_degrees);
  return corners[0].y + track.tail[kSpiralEmitterSlotOfOffsetDown];
}

}  // namespace

void AdvanceSpiralEmitterTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                                std::int16_t billboard_turn_degrees,
                                std::int16_t billboard_tilt_degrees) {
  tracks[kTrackThatTakesTheTurn].style = billboard_turn_degrees;
  tracks[kTrackThatTakesTheTilt].style = billboard_tilt_degrees;

  for (EffectTrack& effect_track : tracks) {
    if (!effect_track.active) {
      continue;
    }
    ++effect_track.ticks;
    if (effect_track.ticks <= 0) {
      continue;
    }
    // ⚠ L'OCTET SE LIT SIGNÉ : « moins que un » ne retient que le zéro.
    const auto octet = static_cast<std::int8_t>(effect_track.mark[kSpiralEmitterMarkOfByte]);
    float& alpha = effect_track.value[kSpiralEmitterSlotOfAlpha];
    const std::int32_t fades_after_tick =
        octet < 1 ? kSpiralEmitterFadesAfterTickOfByteZero : kSpiralEmitterFadesAfterTickOfOthers;
    if (effect_track.ticks <= kSpiralEmitterRisesUntilTick) {
      alpha += kSpiralEmitterAlphaRise;
    } else if (effect_track.ticks > fades_after_tick) {
      alpha -= kSpiralEmitterAlphaFall;
      if (alpha < 0.0F) {
        alpha = 0.0F;
      }
    }
    float& radius = effect_track.tail[kSpiralEmitterSlotOfSpiralRadius];
    radius += kSpiralEmitterRadiusStep;
    // ⚠ LE RAYON SE LIT TRONQUÉ : la spirale s'élargit par crans d'une unité,
    // tous les vingt tics.
    const auto whole_radius = static_cast<float>(static_cast<std::int32_t>(radius));
    std::int32_t rang = effect_track.ticks % kSpiralEmitterTicksPerTurn;
    if (static_cast<std::uint8_t>(octet) == kSpiralEmitterReversedByte) {
      rang = kSpiralEmitterTicksPerTurn - rang;
    }
    const std::int32_t angle = kSpiralEmitterDegreesPerTick * rang;
    effect_track.tail[kSpiralEmitterSlotOfOffsetX] = whole_radius * SinOfDegrees(angle);
    effect_track.tail[kSpiralEmitterSlotOfOffsetDown] = kSpiralEmitterOffsetDown;
    effect_track.tail[kSpiralEmitterSlotOfOffsetZ] = whole_radius * CosOfDegrees(angle);
  }
}

std::optional<std::uint32_t> SpiralEmitterChildOf(const EffectTrack& track,
                                                  std::int16_t turn_degrees,
                                                  std::int16_t tilt_degrees) {
  if (!track.active || !(FirstCornerHeight(track, turn_degrees, tilt_degrees) <= 0.0F) ||
      track.ticks >= kSpiralEmitterEmitsUntilTick ||
      static_cast<std::int32_t>(track.value[kSpiralEmitterSlotOfAlpha]) <= 0) {
    return std::nullopt;
  }
  return track.mark[kSpiralEmitterMarkOfByte] != 0 ? kSpiralEmitterChildOfOthers
                                                   : kSpiralEmitterChildOfByteZero;
}

glm::vec3 SpiralEmitterChildOffset(const EffectTrack& track) {
  return {track.tail[kSpiralEmitterSlotOfOffsetX], -track.tail[kSpiralEmitterSlotOfOffsetDown],
          -track.tail[kSpiralEmitterSlotOfOffsetZ]};
}

}  // namespace ro::render
