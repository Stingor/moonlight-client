#include <algorithm>
#include <cmath>

#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

// CE QUE LES GROUPES RADIAUX POSENT EN DUR.

// ⚠ LA PREMIÈRE MARQUE FAIT DEUX MÉTIERS, et c'est l'ADR 0017 dans un seul
// octet : elle choisit la vitesse de rotation AVANT la boucle, et la règle de
// longueur du PREMIER segment dedans. La lire une fois pour les deux emplois est
// ce que fait le client ; lui en donner deux séparerait ce qu'il a joint.
constexpr std::uint8_t kMarkThatTurnsFastest = 1;
constexpr std::uint8_t kMarkThatTurnsByHalves = 2;
constexpr std::uint8_t kMarkThatTurnsMedium = 3;
// ⚠ ET UNE MARQUE FAIT REGARDER LA CAMÉRA. Elle range le tour de billboard dans
// le champ que la TROISIÈME piste emploie comme durée de vie : le même champ,
// deux sens, et c'est le client qui le veut ainsi.
constexpr std::uint8_t kMarkThatFacesTheCamera = 33;
constexpr std::size_t kTrackThatTakesTheCameraTurn = 2;

// LA ROTATION : chaque piste ajoute son rang au pas de sa marque.
constexpr std::int16_t kTurnOfFastestMark = 8;
constexpr std::int16_t kTurnOfMediumMark = 6;
constexpr std::int16_t kTurnOfHalvesWhenNear = 4;
constexpr std::int16_t kTurnOfHalvesWhenFar = 2;
constexpr std::size_t kFirstFarTrack = 2;

// LA MONTÉE DURE SEIZE TICS, et sa vitesse dépend de la marque.
constexpr std::int32_t kTicksOfRise = 16;
constexpr std::int16_t kAlphaRiseOfHalvesMark = 3;
constexpr std::int16_t kAlphaRiseOfMediumMark = 2;
constexpr std::int16_t kAlphaRisePerTick = 5;
constexpr std::int16_t kAlphaCeiling = 180;
constexpr std::int16_t kAlphaFallPerTick = 2;

// ⚠ LE STYLE EST ICI UNE DURÉE : le tic où la piste commence à s'effacer. Un
// style NÉGATIF veut dire « jamais », et le prendre pour un zéro ferait s'effacer
// d'emblée une piste qui doit tenir.
constexpr std::int16_t kStyleThatNeverFades = 0;

// LA LONGUEUR D'UN SEGMENT : trois règles, une par marque de case.
//
// ⚠ CHAQUE CASE A SA PROPRE MARQUE, et c'est ce qui fait les GROUPES du nom.
// Une seule règle pour les vingt et une donnerait une couronne uniforme là où le
// client en fait des paquets qui battent séparément.
constexpr std::uint8_t kSlotMarkThatHolds = 1;
constexpr std::uint8_t kSlotMarkThatBlinks = 2;
constexpr std::uint8_t kSlotMarkThatHoldsToo = 3;

// L'onde le long du ruban : trente-quatre degrés par case, et une vitesse par
// piste — quatre, trois, puis deux.
constexpr std::int32_t kDegreesPerSlot = 34;
constexpr std::int32_t kTimeRateOfFirstTrack = 4;
constexpr std::int32_t kTimeRateOfSecondTrack = 3;
constexpr std::int32_t kTimeRateOfOtherTracks = 2;

// La part FIXE de la longueur et la part qui ondule : trois quarts contre un.
constexpr float kSteadyShare = 0.75F;
constexpr float kWavingShare = 0.25F;

// LA MONTÉE D'UN SEGMENT dure un quart de tour, et la marque clignotante quatre
// fois moins.
constexpr std::int32_t kLastTickOfGrowth = 90;
constexpr std::int32_t kLastTickOfBlink = 45;
constexpr std::int32_t kBlinkRate = 4;

}  // namespace

void AdvanceRadialParticleGroupTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                                      std::int16_t billboard_turn_degrees) {
  if (tracks.front().mark[0] == kMarkThatFacesTheCamera) {
    tracks[kTrackThatTakesTheCameraTurn].style = billboard_turn_degrees;
  }

  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    EffectTrack& track = tracks[rang];
    if (!track.active) {
      continue;
    }
    ++track.ticks;

    const std::uint8_t mark = track.mark[0];
    const std::int16_t rank_degrees = static_cast<std::int16_t>(rang);
    if (mark == kMarkThatTurnsFastest) {
      track.azimuth_degrees =
          static_cast<std::int16_t>(track.azimuth_degrees + rank_degrees + kTurnOfFastestMark);
    } else if (mark == kMarkThatTurnsByHalves) {
      const std::int16_t step =
          rang < kFirstFarTrack ? kTurnOfHalvesWhenNear : kTurnOfHalvesWhenFar;
      track.azimuth_degrees =
          static_cast<std::int16_t>(track.azimuth_degrees + rank_degrees + step);
    } else if (mark == kMarkThatTurnsMedium) {
      track.azimuth_degrees =
          static_cast<std::int16_t>(track.azimuth_degrees + rank_degrees + kTurnOfMediumMark);
    }
    if (track.azimuth_degrees >= kEffectTrackFullTurnDegrees) {
      track.azimuth_degrees =
          static_cast<std::int16_t>(track.azimuth_degrees - kEffectTrackFullTurnDegrees);
    }

    // ⚠ LE STYLE EST UNE DURÉE, et négative elle veut dire « jamais ».
    const bool still_holds = track.ticks < track.style || track.style < kStyleThatNeverFades;
    if (still_holds) {
      if (track.ticks < kTicksOfRise) {
        const std::int16_t rise = mark == kMarkThatTurnsByHalves ? kAlphaRiseOfHalvesMark
                                  : mark == kMarkThatTurnsMedium ? kAlphaRiseOfMediumMark
                                                                 : kAlphaRisePerTick;
        track.alpha = std::min(static_cast<std::int16_t>(track.alpha + rise), kAlphaCeiling);
      }
    } else {
      track.alpha = std::max(static_cast<std::int16_t>(track.alpha - kAlphaFallPerTick),
                             static_cast<std::int16_t>(0));
    }

    const std::int32_t speed = rang == 0   ? kTimeRateOfFirstTrack
                               : rang == 1 ? kTimeRateOfSecondTrack
                                           : kTimeRateOfOtherTracks;
    for (std::size_t segment = 0; segment < kEffectTrackSlots; ++segment) {
      const std::uint8_t slot_mark = track.mark[segment];
      if (slot_mark == kSlotMarkThatHolds || slot_mark == kSlotMarkThatHoldsToo) {
        track.value[segment] = track.ticks <= kLastTickOfGrowth
                                   ? track.lift_key * SinOfDegrees(track.ticks)
                                   : track.lift_key;
        continue;
      }
      if (slot_mark == kSlotMarkThatBlinks) {
        // ⚠ CELLE-CI S'ÉTEINT POUR DE BON passé quarante-cinq tics, là où les
        // autres tiennent. C'est elle qui fait clignoter un groupe.
        track.value[segment] = track.ticks <= kLastTickOfBlink
                                   ? track.lift_key * SinOfDegrees(kBlinkRate * track.ticks)
                                   : 0.0F;
        continue;
      }
      const std::int32_t phase = WithinFullTurn(
          static_cast<std::int32_t>(segment) * kDegreesPerSlot + speed * track.ticks);
      float length =
          track.lift_key * kWavingShare * SinOfDegrees(phase) + track.lift_key * kSteadyShare;
      if (track.ticks <= kLastTickOfGrowth) {
        length *= SinOfDegrees(track.ticks);
      }
      track.value[segment] = std::max(length, 0.0F);
    }
  }
}

}  // namespace ro::render
