#include "ro/render/effect_expanding_ring.h"

#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

void AdvanceOne(EffectTrack& track, std::size_t rank) {
  if (!track.active) {
    return;
  }
  ++track.ticks;
  track.radius_at_anchor += kExpandingRingGrowthPerTick;

  track.tilt_degrees =
      static_cast<std::int16_t>(track.tilt_degrees - kExpandingRingTiltFallPerTick);
  if (track.tilt_degrees < kExpandingRingTiltFloorDegrees) {
    track.tilt_degrees = kExpandingRingTiltFloorDegrees;
    track.alpha = 0;
  }

  // ⚠ LE SEUIL EST PAR PISTE : sa base plus un.
  const float base = track.tail[kExpandingRingTailOfBaseRadius];
  if (track.radius_at_anchor >= base + kExpandingRingFadeBeyondBase) {
    track.alpha = static_cast<std::int16_t>(track.alpha - kExpandingRingAlphaFallPerTick);
    if (track.alpha <= 0) {  // Seul un alpha strictement positif continue.
      // LA RENAISSANCE, sans condition de vie.
      track.alpha = 0;
      track.radius_at_anchor = base;
      track.tilt_degrees = kExpandingRingTiltAtRebirthDegrees;
    }
  } else {
    track.alpha = static_cast<std::int16_t>(track.alpha + kExpandingRingAlphaRisePerTick);
    if (track.alpha > kExpandingRingAlphaCeiling) {
      track.alpha = kExpandingRingAlphaCeiling;
    }
  }

  // L'ONDE, sur les VINGT ET UNE cases (la boucle court jusqu'à la fin de la
  // table des sinus, pas de neuf degrés). Le ruban n'en
  // lit que onze ; les dix autres sont écrites pour rien, comme chez le client.
  const float le_long_du_temps = SinOfDegrees(UndulationPhaseDegrees(rank, track.ticks));
  const float key = track.lift_key;
  for (std::size_t segment = 0; segment < kEffectTrackSlots; ++segment) {
    const auto along_ribbon =
        static_cast<std::int32_t>(segment) * kEffectTrackUndulationStepDegrees;
    track.value[segment] = key + (key * kExpandingRingUndulationAmplitude *
                                  SinOfDegrees(along_ribbon) * le_long_du_temps);
  }
}

}  // namespace

void AdvanceExpandingRingTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks) {
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    AdvanceOne(tracks[rang], rang);
  }
}

}  // namespace ro::render
