#include <array>

#include "ro/render/effect_double_ribbon.h"
#include "ro/render/effect_track.h"
#include "ro/render/effect_track_motion.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

// L'OUVERTURE DE L'ANNEAU : elle monte d'un degré par tic jusqu'au quart de
// tour, puis RECOMMENCE à zéro. C'est ce qui fait battre l'aura.
constexpr float kOpeningStep = 1.0F;
constexpr float kOpeningCeiling = 90.0F;

// L'ALPHA PENDANT LA MONTÉE : dix de plus par tic, plafonné bas, et remis à dix
// à chaque fois que l'anneau recommence.
constexpr std::int16_t kAlphaStep = 10;
constexpr std::int16_t kAlphaCeiling = 60;
constexpr std::int16_t kAlphaAtRestart = 10;

// L'EXTINCTION : passé le trentième tic, l'alpha est BORNÉ par une pente qui
// atteint zéro au quarante-cinquième. Ce n'est pas une soustraction — c'est un
// plafond qui descend, et l'alpha ne bouge que s'il le dépasse.
constexpr std::int32_t kFadeStartsAfterTick = 30;
constexpr std::int32_t kTickWhenTheCeilingReachesZero = 45;
constexpr std::int32_t kCeilingSlope = 4;

// D'OÙ VIENT L'AZIMUT : le demi-tour ajouté au regard du porteur. L'anneau se
// dresse donc DERRIÈRE lui.
constexpr float kFacingOffsetDegrees = 180.0F;

// ET L'AXE DE GLISSEMENT est la perpendiculaire à cet azimut.
constexpr std::int32_t kAxisOffsetDegrees = 90;

// DE COMBIEN LE TOUR AVANCE PAR TIC.
constexpr std::int16_t kTurnPerTick = 1;

}  // namespace

void AdvanceRadialFanTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                            float owner_facing_degrees) {
  for (EffectTrack& track : tracks) {
    if (!track.active) {
      continue;
    }
    ++track.ticks;
    // ⚠ UN COMPTEUR NÉGATIF EST UN RETARD, et le client le respecte ici : le
    // bâtisseur pose son second nœud quatre tics après le premier, et c'est ce
    // qui donne à l'aura ses deux battements décalés.
    if (track.ticks <= 0) {
      continue;
    }

    if (track.value[kSlotOfDoubleRibbonOpening] < kOpeningCeiling) {
      track.value[kSlotOfDoubleRibbonOpening] += kOpeningStep;
      track.alpha = static_cast<std::int16_t>(track.alpha + kAlphaStep);
      if (track.alpha > kAlphaCeiling) {
        track.alpha = kAlphaCeiling;
      }
    } else {
      // ⚠ L'ANNEAU REPART D'UN POINT ET L'ALPHA REPART DE DIX. C'est la seule
      // boucle de cette famille, et aucun compteur ne la décrit : elle se lit
      // uniquement dans ce retour à zéro.
      track.alpha = kAlphaAtRestart;
      track.value[kSlotOfDoubleRibbonOpening] = 0.0F;
    }

    if (track.ticks > kFadeStartsAfterTick) {
      const auto ceiling =
          static_cast<std::int16_t>((kTickWhenTheCeilingReachesZero - track.ticks) * kCeilingSlope);
      if (track.alpha >= ceiling) {
        track.alpha = ceiling < 0 ? 0 : ceiling;
      }
    }

    track.tilt_degrees = static_cast<std::int16_t>(track.tilt_degrees + kTurnPerTick);
    if (track.tilt_degrees >= kEffectTrackFullTurnDegrees) {
      track.tilt_degrees =
          static_cast<std::int16_t>(track.tilt_degrees - kEffectTrackFullTurnDegrees);
    }

    // ⚠ LE CLIENT TRONQUE APRÈS L'ADDITION, comme pour le bouclier qui fait
    // face : il ajoute le demi-tour au flottant du porteur et convertit le total.
    track.azimuth_degrees = static_cast<std::int16_t>(
        static_cast<std::int32_t>(owner_facing_degrees + kFacingOffsetDegrees));
    if (track.azimuth_degrees >= kEffectTrackFullTurnDegrees) {
      track.azimuth_degrees =
          static_cast<std::int16_t>(track.azimuth_degrees - kEffectTrackFullTurnDegrees);
    }

    // L'AXE DE GLISSEMENT, recalculé à chaque tic : un vecteur horizontal
    // unitaire, perpendiculaire à l'azimut.
    const std::int32_t axe =
        track.azimuth_degrees + kAxisOffsetDegrees >= kEffectTrackFullTurnDegrees
            ? track.azimuth_degrees + kAxisOffsetDegrees - kEffectTrackFullTurnDegrees
            : track.azimuth_degrees + kAxisOffsetDegrees;
    track.tail[kTailOfDoubleRibbonAxisX] = CosOfDegrees(axe);
    track.tail[kTailOfDoubleRibbonAxisZ] = SinOfDegrees(axe);
  }
}

}  // namespace ro::render
