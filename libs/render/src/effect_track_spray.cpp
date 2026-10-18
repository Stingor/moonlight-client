#include <algorithm>
#include <cmath>

#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

// CE QUE LA GERBE POSE EN DUR.

// ⚠ LE MODE LENT SE RECONNAÎT À UN NOMBRE, pas à un drapeau : le client compare
// un champ du nœud à quatre-vingt-dix-neuf. Tout le reste est le mode rapide.
constexpr float kSlowMode = 99.0F;
constexpr std::int16_t kTurnPerTickWhenSlow = 3;
constexpr std::int16_t kTurnPerTick = 5;
constexpr float kRadiusGrowthWhenSlow = 0.1F;
constexpr float kRadiusGrowth = 0.30000001F;

// ⚠ L'INCLINAISON DESCEND D'UN DEGRÉ PAR TIC ET S'ARRÊTE À UN PLANCHER, qui
// n'est pas le même dans les deux modes. Sans lui, la gerbe finirait couchée et
// disparaîtrait dans le sol.
constexpr std::int16_t kTiltFallPerTick = 1;
constexpr std::int16_t kTiltFloorWhenSlow = 30;
constexpr std::int16_t kTiltFloor = 40;

// LA MONTÉE NE DURE QUE DIX TICS, et un style monte deux fois moins vite.
constexpr std::int32_t kTicksOfRise = 10;
constexpr std::int16_t kStyleThatRisesSlowly = 1;
constexpr std::int16_t kAlphaRiseOfThatStyle = 10;
constexpr std::int16_t kAlphaRisePerTick = 20;

// ⚠ LA DESCENTE, ELLE, DÉPEND DU STYLE et de rien d'autre — cinq vitesses pour
// cinq styles. C'est ce qui fait qu'une même gerbe s'éteint vite ou lentement
// selon l'effet qui l'emploie ; une vitesse commune les uniformiserait.
[[nodiscard]] std::int16_t ChuteDuStyle(std::int16_t style) {
  switch (style) {
    case 2:
      return 8;
    case 3:
    case 4:
      return 5;
    case 5:
      return 4;
    default:
      return 2;
  }
}

// ⚠ L'ARCHE EST CELLE DU CÔNE — neuf degrés par segment, vingt et un segments —,
// MAIS ELLE RESPIRE AU LIEU DE SE FIGER. Quinze centièmes d'amplitude, et chaque
// piste respire un quart de tour après la précédente : c'est ce décalage qui
// fait la gerbe. Les figer comme le cône donnerait quatre couronnes immobiles.
constexpr std::int32_t kDegreesPerSlot = 9;
constexpr float kBreathAmplitude = 0.15000001F;
constexpr std::int32_t kPhaseStepPerTrack = 90;

}  // namespace

void AdvanceDualModeSprayTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                                float spray_mode) {
  const bool lent = spray_mode == kSlowMode;
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    EffectTrack& track = tracks[rang];
    if (!track.active) {
      continue;
    }
    ++track.ticks;

    TurnOnward(track.azimuth_degrees, lent ? kTurnPerTickWhenSlow : kTurnPerTick);

    track.radius_at_anchor += lent ? kRadiusGrowthWhenSlow : kRadiusGrowth;
    track.tilt_degrees = std::max(static_cast<std::int16_t>(track.tilt_degrees - kTiltFallPerTick),
                                  lent ? kTiltFloorWhenSlow : kTiltFloor);

    if (track.ticks <= kTicksOfRise) {
      const std::int16_t rise =
          track.style == kStyleThatRisesSlowly ? kAlphaRiseOfThatStyle : kAlphaRisePerTick;
      track.alpha = static_cast<std::int16_t>(track.alpha + rise);
    } else {
      track.alpha = std::max(static_cast<std::int16_t>(track.alpha - ChuteDuStyle(track.style)),
                             static_cast<std::int16_t>(0));
    }

    // ⚠ LA PHASE PORTE LE RANG DE LA PISTE. Quatre couronnes qui respirent
    // ensemble ne font qu'une couronne épaisse.
    const std::int32_t phase =
        (static_cast<std::int32_t>(rang) * kPhaseStepPerTrack + track.ticks) %
        kEffectTrackFullTurnDegrees;
    const float over_time = 1.0F + kBreathAmplitude * SinOfDegrees(phase);
    for (std::size_t segment = 0; segment < kEffectTrackSlots; ++segment) {
      const float along_ribbon =
          track.lift_key * SinOfDegrees(static_cast<std::int32_t>(segment) * kDegreesPerSlot);
      track.value[segment] = along_ribbon * over_time;
    }
  }
}

}  // namespace ro::render
