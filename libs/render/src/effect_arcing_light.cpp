#include "ro/render/effect_arcing_light.h"

#include <algorithm>

#include "ro/render/effect_particle_quad.h"
#include "ro/render/effect_quad_rotation.h"
#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

// LES DEUX PISTES qui portent les angles de caméra, que la figure relit à des
// emplacements FIXES du nœud, quelle que soit la piste dessinée.
constexpr std::size_t kTrackThatTakesTheTurn = 0;
constexpr std::size_t kTrackThatTakesTheTilt = 1;

// LE MODE DE MÉLANGE que la figure pousse : zéro, l'ADDITION.
constexpr std::int16_t kAddedBlend = 0;
// LES QUATRE NOMBRES DE TEXTURE `0, 1, 0, 1` : l'image est retournée, un aux
// deux premiers coins et zéro aux deux derniers.
constexpr float kImageVAtFirstCorners = 1.0F;
constexpr float kImageVAtLastCorners = 0.0F;

constexpr float kOpaqueAlpha = 255.0F;

// LES QUATRE COINS DÉPORTÉS, dans le repère du client, dont l'ordonnée DESCEND.
[[nodiscard]] std::array<glm::vec3, 4> ClientCorners(const EffectTrack& track,
                                                     std::int16_t turn_degrees,
                                                     std::int16_t tilt_degrees) {
  std::array<glm::vec3, 4> corners;
  for (std::size_t corner = 0; corner < corners.size(); ++corner) {
    const std::int32_t angle = kArcingLightFirstCornerDegrees +
                               (kArcingLightDegreesPerCorner * static_cast<std::int32_t>(corner));
    corners[corner] = {track.radius_at_anchor * CosOfDegrees(angle), 0.0F,
                       track.radius_at_anchor * SinOfDegrees(angle)};
  }
  RotateEffectQuadCorners(corners, turn_degrees, tilt_degrees);
  // ⚠ L'ÉCART LATÉRAL SUIT LE TOUR DE LA CAMÉRA, celui-là même qui a redressé
  // le carré : il reste dans le plan de l'image, et la lueur passe d'un côté à
  // l'autre du porteur quelle que soit la vue.
  const std::int32_t tour = WithinFullTurn(turn_degrees);
  const float de_cote = track.tail[kArcingLightSlotOfSideways];
  const glm::vec3 offset{de_cote * CosOfDegrees(tour), track.tail[kArcingLightSlotOfDepthBelow],
                         de_cote * SinOfDegrees(tour)};
  for (glm::vec3& corner : corners) {
    corner += offset;
  }
  return corners;
}

// Vrai quand la piste se peint : allumée, et son premier coin pas SOUS le nœud.
//
// ⚠ UNE HAUTEUR QUI N'EST PAS UN NOMBRE ne se peint pas : la comparaison du
// client échoue alors, et c'est la branche qui saute la peinture.
[[nodiscard]] bool Paints(const std::array<glm::vec3, 4>& corners) { return corners[0].y <= 0.0F; }

}  // namespace

void AdvanceArcingLightTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
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
    float& alpha = effect_track.value[kArcingLightSlotOfAlpha];
    if (effect_track.ticks <= kArcingLightRisesUntilTick) {
      alpha += kArcingLightAlphaRise;
    } else if (effect_track.ticks > kArcingLightFadesAfterTick) {
      alpha -= kArcingLightAlphaFall;
      if (0.0F > alpha) {
        alpha = 0.0F;
      }
    }
    // ⚠ LE COMPTEUR D'APRÈS L'INCRÉMENT, divisé deux fois : la course accélère.
    float& course = effect_track.tail[kArcingLightSlotOfSweepDegrees];
    course += static_cast<float>(effect_track.ticks) / kArcingLightSweepFirstDivisor /
              kArcingLightSweepSecondDivisor;
    if (course >= kArcingLightSweepCeilingDegrees) {
      course = kArcingLightSweepCeilingDegrees;
    }
    // L'ANGLE TRONQUÉ sert aux deux coordonnées : il indexe la table de sinus.
    const auto degrees = static_cast<std::int32_t>(course);
    effect_track.tail[kArcingLightSlotOfSideways] =
        (static_cast<float>(degrees) / kArcingLightSidewaysDivisor) - kArcingLightSidewaysStart;
    effect_track.tail[kArcingLightSlotOfDepthBelow] =
        -(SinOfDegrees(degrees) * kArcingLightHeightSwing) - kArcingLightHeightBase;
    effect_track.tail[kArcingLightSlotOfForward] = 0.0F;
  }
}

std::optional<EffectWorldQuad> ComposeArcingLightQuad(const EffectTrack& track,
                                                      std::int16_t turn_degrees,
                                                      std::int16_t tilt_degrees,
                                                      glm::vec3 node_origin) {
  if (!track.active) {
    return std::nullopt;
  }
  const std::array<glm::vec3, 4> corners = ClientCorners(track, turn_degrees, tilt_degrees);
  if (!Paints(corners)) {
    return std::nullopt;
  }
  EffectParticleQuadRequest request;
  for (std::size_t corner = 0; corner < corners.size(); ++corner) {
    // ⚠ LE CARRÉ ET SON ÉCART SONT DANS LE REPÈRE DU CLIENT, tournés par les
    // angles de SA caméra : ils reviennent au nôtre ensemble, par `S` entier.
    request.corners[corner] = EffectPointFromClient(corners[corner]);
  }
  request.absolute = false;
  // L'alpha flottant, TRONQUÉ vers zéro.
  request.alpha = static_cast<std::uint8_t>(std::clamp(
      static_cast<float>(static_cast<std::int32_t>(track.value[kArcingLightSlotOfAlpha])), 0.0F,
      kOpaqueAlpha));
  request.color = kArcingLightColor;
  request.blend_mode = kAddedBlend;
  request.texture = 0;
  request.v_at_a_and_b = kImageVAtFirstCorners;
  request.v_at_c_and_d = kImageVAtLastCorners;
  return ComposeEffectParticleQuad(request, node_origin);
}

bool ArcingLightSowsFollowUp(const EffectTrack& track, std::int16_t turn_degrees,
                             std::int16_t tilt_degrees) {
  return track.active && track.ticks < kArcingLightFollowUpUntilTick &&
         Paints(ClientCorners(track, turn_degrees, tilt_degrees));
}

}  // namespace ro::render
