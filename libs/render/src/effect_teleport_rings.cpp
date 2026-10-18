#include "ro/render/effect_teleport_rings.h"

#include <algorithm>
#include <cstddef>

#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

// LA PHASE DE L'ONDE à ce tic, pour cette piste.
//
// ⚠ DEUX FORMULES, ET LE RANG CHOISIT — mais le seuil se mesure sur le pas de la
// PREMIÈRE formule, non sur celui de la seconde. Le client compare quatre-vingt-dix
// fois le rang à cent quatre-vingts : les deux premières pistes prennent la
// branche lente, les deux dernières la rapide.
[[nodiscard]] std::int32_t WavePhase(std::size_t rang, std::int32_t ticks) {
  const std::int32_t r = static_cast<std::int32_t>(rang);
  if (r * kTeleportRingPhaseStepPerRank < kTeleportRingPhaseThreshold) {
    return WithinFullTurn(ticks + r * kTeleportRingPhaseStepPerRank);
  }
  return WithinFullTurn((ticks + r * kTeleportRingFastStepPerRank) * kTeleportRingFastFactor);
}

}  // namespace

void AdvanceTeleportRingTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks) {
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    EffectTrack& track = tracks[rang];
    if (!track.active) {
      continue;
    }
    ++track.ticks;

    // ⚠ L'AZIMUT TOURNE AVANT TOUT, et même quand le compte de tics est encore
    // négatif : le client le fait hors de la garde qui suit. Une piste retardée
    // tourne donc sans se montrer, et son azimut n'est pas celui de son premier
    // tic visible.
    const std::int16_t step = static_cast<std::int16_t>(kTeleportRingTurnPerTickDegrees -
                                                        static_cast<std::int16_t>(rang));
    TurnOnward(track.azimuth_degrees, step);

    if (track.ticks <= 0) {
      continue;
    }

    const std::int32_t breath = kTeleportRingBreathFactor * track.ticks;
    if (track.mark[kTeleportRingMarkOfSwitch] == 0) {
      if (breath >= kTeleportRingSwitchThreshold) {
        track.mark[kTeleportRingMarkOfSwitch] = 1;
      } else if (breath >= kTeleportRingFadeThreshold) {
        track.alpha = std::max<std::int16_t>(
            static_cast<std::int16_t>(track.alpha - kTeleportRingFadeBeforeSwitch), 0);
      }
      // ⚠ LE RAYON SE RECALCULE, IL NE S'ACCUMULE PAS. L'amplitude vient de
      // l'octet que le semis a posé, et le sinus du double du tic la fait
      // respirer : l'anneau s'ouvre, se referme, et rouvre.
      //
      // ⚠ ET L'OCTET EST SIGNÉ. Au-delà de cent vingt-sept, le semis du client
      // le fait passer négatif et l'anneau respire à l'envers ; le lire non
      // signé montrerait ce que le client ne montre pas.
      const auto amplitude =
          static_cast<float>(static_cast<std::int8_t>(track.mark[kTeleportRingMarkOfAmplitude]));
      track.lift_key = amplitude * SinOfDegrees(breath);
    } else {
      track.alpha = std::max<std::int16_t>(
          static_cast<std::int16_t>(track.alpha - kTeleportRingFadeAfterSwitch), 0);
    }

    // Chaque segment prend la longueur du rayon, ondulée par le produit de deux
    // sinus : l'un court LE LONG du ruban, l'autre dans le TEMPS. C'est l'onde
    // du sous-type 30 ; ce qui change est le rayon qu'elle module, et la phase.
    const float over_time = SinOfDegrees(WavePhase(rang, track.ticks));
    for (std::size_t segment = 0; segment < kEffectTrackSlots; ++segment) {
      const std::int32_t along_ribbon =
          static_cast<std::int32_t>(segment) * kEffectTrackUndulationStepDegrees;
      const float ondulation =
          track.lift_key * kTeleportRingUndulation * SinOfDegrees(along_ribbon) * over_time;
      track.value[segment] = track.lift_key + ondulation;
    }
  }
}

}  // namespace ro::render
