#include "ro/render/effect_snaking_arc.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

// CE QUE LE MOUVEMENT POSE, cas 70 du répartiteur.

// De combien la phase de tête avance par tic.
constexpr float kPhaseStep = 8.0F;
// À partir de quel tic l'alpha tombe, et de combien.
constexpr std::int32_t kFadeAfterTick = 35;
constexpr std::int16_t kAlphaFall = 5;

// LE FACTEUR D'UN RANG : sept dixièmes élevés au rang, comme le client
// l'accumule — par multiplications successives, jamais par une puissance.
[[nodiscard]] float FacteurDuRang(std::uint8_t rang) {
  float facteur = 1.0F;
  for (std::uint8_t tour = 0; tour < rang; ++tour) {
    facteur *= kSnakingArcDecay;
  }
  return facteur;
}

// LA DISTANCE PARCOURUE au bout d'un segment : la somme des rangs déjà
// franchis, plus la part du rang courant que la phase a entamée.
[[nodiscard]] float DistanceAuJoint(const EffectTrack& effect_track, std::size_t segment) {
  const std::uint8_t rang = effect_track.mark[segment];
  const float length = effect_track.radius_at_anchor + effect_track.radius_at_anchor;
  float parcourue = 0.0F;
  float facteur = 1.0F;
  for (std::uint8_t tour = 0; tour < rang; ++tour) {
    parcourue += length * facteur;
    facteur *= kSnakingArcDecay;
  }
  const float phase = effect_track.value[segment];
  if (phase > 0.0F) {
    parcourue += effect_track.radius_at_anchor * facteur * (phase / kSnakingArcPhaseHalfDegrees);
  }
  return parcourue;
}

// DE COMBIEN LE SEGMENT BOMBE, dans NOTRE repère.
//
// ⚠ NOTRE REPÈRE MONTE, CELUI DU CLIENT DESCEND : ce qu'il pose en ordonnée
// négative bombe vers le haut, et se pose ici en positive.
[[nodiscard]] float BombementAuJoint(const EffectTrack& effect_track, std::size_t segment) {
  const float phase = effect_track.value[segment];
  return effect_track.radius_at_anchor *
         SinOfDegrees(static_cast<std::int32_t>(static_cast<std::int16_t>(phase))) *
         kSnakingArcBowScale * FacteurDuRang(effect_track.mark[segment]);
}

}  // namespace

std::array<std::uint8_t, 3> EffectSnakingArcColor(std::int16_t style, std::size_t segment) {
  const auto rise =
      static_cast<std::uint8_t>(kSnakingArcTintPerSegment * static_cast<std::uint8_t>(segment));
  if (style == kSnakingArcPinkStyle) {
    const auto neighbor = static_cast<std::uint8_t>(rise + kSnakingArcPinkFloor);
    return {kEffectTrackAlphaMax, neighbor, neighbor};
  }
  const auto neighbor = static_cast<std::uint8_t>(rise + kSnakingArcOtherFloor);
  if (style == kSnakingArcBlueStyle) {
    return {neighbor, neighbor, kEffectTrackAlphaMax};
  }
  // ⚠ LE CAS PAR DÉFAUT EST AMBRE, et il sert : le premier des trois sites
  // passe un style zéro, que l'aiguillage ne nomme pas.
  return {kEffectTrackAlphaMax, kSnakingArcAmberGreen, neighbor};
}

glm::vec3 EffectSnakingArcJoint(const EffectTrack& track, std::size_t segment) {
  const float distance = DistanceAuJoint(track, segment);
  // LE CAP TIRÉ EST CELUI DU CLIENT : sa profondeur `distance·sin(cap)` se nie
  // chez nous, comme sa hauteur.
  return {distance * CosOfDegrees(track.tilt_degrees), BombementAuJoint(track, segment),
          -distance * SinOfDegrees(track.tilt_degrees)};
}

std::vector<EffectWorldQuad> ComposeSnakingArcQuads(const EffectTrack& track,
                                                    const EffectOwnerTint& owner) {
  std::vector<EffectWorldQuad> quads;
  // ⚠ TROIS GARDES, ET LA TROISIÈME EST UN COMPTEUR : une piste en retard ne se
  // dessine pas, même allumée et même opaque.
  if (!track.active || track.alpha <= 0 || track.ticks <= 0) {
    return quads;
  }

  for (std::size_t segment = 0; segment < kSnakingArcSegments; ++segment) {
    const std::int16_t alpha = static_cast<std::int16_t>(
        track.alpha - kSnakingArcAlphaPerSegment * static_cast<std::int16_t>(segment));
    // ⚠ UN SEGMENT DONT L'ALPHA EST ÉPUISÉ N'EST PAS DESSINÉ, et les suivants
    // non plus : l'arc se voit sur d'autant moins de segments qu'il est pâle.
    if (alpha <= 0) {
      break;
    }
    const glm::vec3 near_point = track.anchor + EffectSnakingArcJoint(track, segment);
    const glm::vec3 far_point = track.anchor + EffectSnakingArcJoint(track, segment + 1);
    // LE PLAN DU TUBE est à un quart de tour du cap qui va du bout lointain
    // vers le proche. Ce cap, mesuré sur nos points, est l'opposé de celui du
    // client : le quart de tour qu'il y ajoute se RETRANCHE ici.
    const std::int32_t plan =
        WithinFullTurn(HeadingOfVector(near_point.x - far_point.x, near_point.z - far_point.z) -
                       kEffectTrackFullTurnDegrees / 4);

    glm::vec3 previous_near{0.0F, 0.0F, 0.0F};
    glm::vec3 previous_far{0.0F, 0.0F, 0.0F};
    for (std::size_t face = 0; face <= kSnakingArcFaces; ++face) {
      const std::int32_t autour = kSnakingArcFaceStepDegrees * static_cast<std::int32_t>(face);
      const float radial = track.lift_key * CosOfDegrees(autour);
      // ⚠ LA HAUTEUR DE L'ANNEAU CHANGE DE SIGNE, comme tout ce qui monte.
      const glm::vec3 offset{radial * CosOfDegrees(plan), -track.lift_key * SinOfDegrees(autour),
                             radial * SinOfDegrees(plan)};
      if (face > 0) {
        EffectParticleQuadRequest request;
        request.corners = {previous_near, near_point + offset, far_point + offset, previous_far};
        // ⚠ ABSOLUS : l'ancre de la piste est déjà dans le monde.
        request.absolute = true;
        request.alpha = static_cast<std::uint8_t>(alpha);
        request.color = EffectSnakingArcColor(track.style, segment);
        request.v_at_a_and_b = 1.0F;
        request.v_at_c_and_d = 0.0F;
        quads.push_back(ComposeEffectParticleQuad(request, glm::vec3{0.0F, 0.0F, 0.0F}, owner));
      }
      previous_near = near_point + offset;
      previous_far = far_point + offset;
    }
  }
  return quads;
}

void AdvanceSnakingArcTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks) {
  for (EffectTrack& effect_track : tracks) {
    if (!effect_track.active) {
      continue;
    }
    ++effect_track.ticks;
    // ⚠ LE RETARD EST PAR PISTE : tant que le compteur n'est pas positif, rien
    // n'avance et rien ne se dessine.
    if (effect_track.ticks <= 0) {
      continue;
    }
    if (effect_track.ticks > kFadeAfterTick) {
      effect_track.alpha = static_cast<std::int16_t>(effect_track.alpha - kAlphaFall);
    }
    effect_track.alpha = std::max<std::int16_t>(effect_track.alpha, 0);

    effect_track.value[0] += kPhaseStep;
    if (effect_track.value[0] > kSnakingArcPhaseFullDegrees) {
      effect_track.value[0] -= kSnakingArcPhaseFullDegrees;
      ++effect_track.mark[0];
    }
    // ⚠ TOUT L'HISTORIQUE RECULE D'UN CRAN, de la queue vers la tête : remonter
    // la boucle écraserait les vingt crans avec le même.
    for (std::size_t rang = kEffectTrackSlots - 1; rang > 0; --rang) {
      effect_track.value[rang] = effect_track.value[rang - 1];
      effect_track.mark[rang] = effect_track.mark[rang - 1];
    }
  }
}

}  // namespace ro::render
