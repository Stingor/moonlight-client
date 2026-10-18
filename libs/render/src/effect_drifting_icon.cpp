#include "ro/render/effect_drifting_icon.h"

#include <algorithm>

#include "ro/render/effect_node.h"
#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

// LES DEUX PISTES QUI PORTENT LES ANGLES DE CAMÉRA.
constexpr std::size_t kTrackThatTakesTheTurn = 0;
constexpr std::size_t kTrackThatTakesTheTilt = 1;

// LES CASES DES DEUX VALEURS COURANTES, dont le sinus fait dériver l'ancre.
constexpr std::size_t kSlotOfSideDrift = 0;
constexpr std::size_t kSlotOfDepthDrift = 2;

// Le tour, en flottant, tel que l'angle propre se replie.
constexpr float kFullTurnDegrees = static_cast<float>(kEffectTrackFullTurnDegrees);

// LE PAS DE L'ANGLE PROPRE : le même 1.0 que celui des valeurs qui courent.
constexpr float kSpinPerTick = 1.0F;

// Un tirage sous `bornes`, ou zéro SANS TIRER quand la borne est nulle : le
// client saute alors son `rand()`.
[[nodiscard]] std::int32_t DrawBelowOrZero(std::int32_t exclusive_bound, std::uint32_t& generator) {
  if (exclusive_bound == 0) {
    return 0;
  }
  return static_cast<std::int32_t>(NextEffectNodeDraw(generator) %
                                   static_cast<std::uint32_t>(exclusive_bound));
}

// UN COUPLE (valeur, cible) d'un pas.
//
// ⚠ LA NOUVELLE CIBLE SE TIRE CONTRE LA VALEUR COURANTE, pas contre le tour :
// en montant, elle tombe SOUS la valeur (`rand() % valeur`) ; en descendant,
// elle tombe au-dessus de `359 − valeur` (`359 − rand() % valeur`). La valeur
// oscille donc sans jamais s'arrêter.
void ChaseTarget(float& value, float& target, std::uint32_t& generator) {
  if (target > value) {
    value = std::min(value + kDriftingIconChaseStep, kDriftingIconChaseCeiling);
    if (value > target) {
      target = static_cast<float>(DrawBelowOrZero(static_cast<std::int32_t>(value), generator));
    }
    return;
  }
  value = std::max(value - kDriftingIconChaseStep, 0.0F);
  // ⚠ « ATTEINTE » VEUT DIRE « CIBLE AU MOINS ÉGALE » en descendant, et
  // « VALEUR STRICTEMENT AU-DELÀ » en montant. Une valeur qui monte pile sur
  // sa cible ne tire donc rien : c'est le tic suivant, en redescendant d'un
  // degré, qui tire.
  if (target >= value) {
    target = kDriftingIconChaseCeiling -
             static_cast<float>(DrawBelowOrZero(static_cast<std::int32_t>(value), generator));
  }
}

}  // namespace

void AdvanceDriftingIconTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                               std::int16_t camera_turn_degrees, std::int16_t camera_tilt_degrees,
                               std::uint32_t& generator) {
  tracks[kTrackThatTakesTheTurn].style = camera_turn_degrees;
  tracks[kTrackThatTakesTheTilt].style = camera_tilt_degrees;

  for (EffectTrack& track : tracks) {
    if (!track.active) {
      continue;
    }
    ++track.ticks;
    // ⚠ PAS DE GARDE SUR UN COMPTEUR NÉGATIF : le bâtisseur le pose à zéro, et
    // ce mouvement n'a pas de retard à respecter.
    if (track.ticks < kDriftingIconFirstTickPastRise) {
      track.alpha = static_cast<std::int16_t>(track.alpha + kDriftingIconAlphaStep);
    }
    if (track.ticks > track.azimuth_degrees) {
      track.alpha = static_cast<std::int16_t>(track.alpha - kDriftingIconAlphaStep);
    }

    for (std::size_t couple = 0; couple < kDriftingIconChasingPairs; ++couple) {
      const std::size_t value_slot = couple * kDriftingIconSlotsPerPair;
      ChaseTarget(track.value[value_slot], track.value[value_slot + 1], generator);
    }

    // ⚠ LA VALEUR SE TRONQUE AVANT D'INDEXER (`cvttss2si`) : elle est toujours
    // entière ici, mais c'est la troncature que le client fait.
    track.anchor.x += SinOfDegrees(static_cast<std::int32_t>(track.value[kSlotOfSideDrift])) *
                      kDriftingIconDriftFactor;
    // ⚠ LA PROFONDEUR SE RETRANCHE : l'ancre est dans notre repère, dont la
    // profondeur est opposée à celle du client.
    track.anchor.z -= SinOfDegrees(static_cast<std::int32_t>(track.value[kSlotOfDepthDrift])) *
                      kDriftingIconDriftFactor;

    float& own = track.value[kDriftingIconSlotOfSpinDegrees];
    own += kSpinPerTick;
    if (own >= kFullTurnDegrees) {
      own -= kFullTurnDegrees;
    }

    TurnOnward(track.degrees, kDriftingIconTurnPerTick);
  }
}

}  // namespace ro::render
