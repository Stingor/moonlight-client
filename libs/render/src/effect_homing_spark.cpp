#include "ro/render/effect_homing_spark.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

// Le mode que la voie A entend comme une ADDITION.
constexpr std::int16_t kAddedBlend = 0;

// CE QUE LE MOUVEMENT POSE EN DUR.

// Pendant combien de tics l'alpha monte, et de combien par tic.
constexpr std::int32_t kAlphaRiseTicks = 20;
constexpr std::int16_t kAlphaRise = 5;
// À quelle distance du centre l'approche cesse et le tournoiement commence.
constexpr float kHomingRadius = 12.0F;
// LE TOURNOIEMENT : ce qu'il fait par tic, et combien de tics il dure.
constexpr std::int16_t kSpinStepDegrees = 9;
constexpr std::int16_t kSpinAlphaFall = 2;
constexpr float kSpinShrink = 0.99F;
constexpr std::uint8_t kSpinTicks = 10;
// LA DÉRIVE : ses deux pas d'angle, et le tic à partir duquel elle s'éteint.
constexpr std::int16_t kDriftTiltStep = -3;
constexpr std::int16_t kDriftAzimuthStep = 1;
constexpr std::int32_t kDriftFadesAfterTick = 300;
constexpr std::int16_t kDriftAlphaFall = 1;

// UN DEMI-TOUR, pour l'autre bout d'une lame.
constexpr std::int32_t kHalfTurn = kEffectTrackHalfTurnDegrees;

// LE POINT D'UNE LAME à un angle donné, dans le repère du client.
//
// Les deux angles composent comme le client les compose : l'inclinaison donne
// la place sur le cercle, l'azimut fait basculer son plan, et l'axe polaire
// est celui du nord.
[[nodiscard]] glm::vec3 PointDeLame(float radius, std::int32_t circle_angle_degrees,
                                    std::int16_t azimuth, float lift) {
  const float radial = radius * CosOfDegrees(WithinFullTurn(circle_angle_degrees));
  return {radial * CosOfDegrees(azimuth), radial * SinOfDegrees(azimuth) + lift,
          radius * SinOfDegrees(WithinFullTurn(circle_angle_degrees))};
}

// LE PAS D'UNE PISTE : de combien son ancre recule à chaque tic, dans le
// repère du client.
[[nodiscard]] glm::vec3 ApproachStep(const EffectTrack& effect_track) {
  const float step = effect_track.value[kHomingSparkSlotOfStep];
  const float radial = step * CosOfDegrees(effect_track.tilt_degrees);
  return {radial * CosOfDegrees(effect_track.azimuth_degrees),
          radial * SinOfDegrees(effect_track.azimuth_degrees),
          step * SinOfDegrees(effect_track.tilt_degrees)};
}

}  // namespace

std::vector<EffectWorldQuad> ComposeHomingSparkQuads(const EffectTrack& track,
                                                     glm::vec3 node_origin,
                                                     const EffectOwnerTint& owner) {
  std::vector<EffectWorldQuad> quads;
  if (!track.active || track.alpha <= 0) {
    return quads;
  }
  const std::uint8_t tint = track.mark[kHomingSparkMarkOfColor];
  // ⚠ AU-DELÀ DES QUATRE TEINTES, LE CLIENT NE DESSINE RIEN : son aiguillage
  // n'a pas de cas par défaut. Retomber sur une teinte inventerait une figure.
  if (tint >= kHomingSparkMarks) {
    return quads;
  }

  const float radius = track.radius_at_anchor;
  const float lift = track.value[kHomingSparkSlotOfLift];
  const std::int32_t centre = track.tilt_degrees;

  quads.reserve(kHomingSparkBlades);
  for (std::size_t lame = 0; lame < kHomingSparkBlades; ++lame) {
    const std::int32_t half = kHomingSparkBladeHalfWidths[lame];
    // LES QUATRE COINS, dans l'ordre du client : les deux bords du premier
    // bout, puis les deux du bout OPPOSÉ — la lame traverse le centre.
    const std::array<std::int32_t, 4> angles = {
        centre - half, centre + half, centre + kHalfTurn - half, centre + kHalfTurn + half};
    EffectParticleQuadRequest request;
    for (std::size_t corner = 0; corner < angles.size(); ++corner) {
      const glm::vec3 point = PointDeLame(radius, angles[corner], track.azimuth_degrees, lift);
      // ⚠ NOTRE REPÈRE MONTE, CELUI DU CLIENT DESCEND : la hauteur du coin et
      // celle de l'ancre changent de signe ENSEMBLE, au dernier moment — et la
      // profondeur avec elles. N'en retourner qu'une ferait de la gerbe le
      // miroir de la sienne, qui tournoierait à l'envers.
      request.corners[corner] = EffectPointFromClient(track.anchor + point);
    }
    request.alpha = static_cast<std::uint8_t>(track.alpha);
    request.color = lame == 0 ? kHomingSparkThinColors[tint] : kHomingSparkWideColors[tint];
    request.blend_mode = kAddedBlend;
    // ⚠ L'IMAGE EST RETOURNÉE : le client donne un aux deux premiers coins et
    // zéro aux deux derniers, là où le défaut fait l'inverse.
    request.v_at_a_and_b = 1.0F;
    request.v_at_c_and_d = 0.0F;
    quads.push_back(ComposeEffectParticleQuad(request, node_origin, owner));
  }
  return quads;
}

void AdvanceHomingSparkTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks) {
  for (EffectTrack& effect_track : tracks) {
    if (!effect_track.active) {
      continue;
    }
    ++effect_track.ticks;
    if (effect_track.ticks <= 0) {
      continue;
    }
    if (effect_track.ticks < kAlphaRiseTicks) {
      effect_track.alpha = std::min<std::int16_t>(
          static_cast<std::int16_t>(effect_track.alpha + kAlphaRise), kEffectTrackAlphaMax);
    }
    const float distance = std::sqrt(effect_track.anchor.x * effect_track.anchor.x +
                                     effect_track.anchor.y * effect_track.anchor.y +
                                     effect_track.anchor.z * effect_track.anchor.z);
    std::uint8_t& phase = effect_track.mark[kHomingSparkMarkOfPhase];
    // ⚠ L'APPROCHE NE CESSE PAS AU CENTRE mais à DOUZE UNITÉS : l'étincelle
    // se met à tournoyer avant d'arriver. La laisser aller jusqu'au bout la
    // ferait traverser le lanceur.
    if (phase == kHomingSparkApproachPhase && distance < kHomingRadius) {
      phase = kHomingSparkSpinPhase;
      // ⚠ LE CLIENT ÉCRIT DEUX MARQUES D'UN COUP, en un seul mot de seize
      // bits : la phase passe à un ET son compteur repart de zéro.
      effect_track.mark[kHomingSparkMarkOfSpinCount] = 0;
    }
    if (phase == kHomingSparkSpinPhase) {
      effect_track.tilt_degrees =
          static_cast<std::int16_t>(WithinFullTurn(effect_track.tilt_degrees + kSpinStepDegrees));
      effect_track.azimuth_degrees =
          static_cast<std::int16_t>(WithinFullTurn(effect_track.azimuth_degrees));
      ++effect_track.mark[kHomingSparkMarkOfSpinCount];
      if (effect_track.mark[kHomingSparkMarkOfSpinCount] >= kSpinTicks) {
        phase = kHomingSparkDriftPhase;
      }
      effect_track.alpha =
          std::max<std::int16_t>(static_cast<std::int16_t>(effect_track.alpha - kSpinAlphaFall), 0);
      effect_track.radius_at_anchor *= kSpinShrink;
    }
    // ⚠ LE MÊME TIC PEUT TRAVERSER LES DEUX PHASES : le client retombe sur la
    // dérive dès que le compteur a débordé, sans attendre le tic suivant.
    if (phase == kHomingSparkDriftPhase) {
      effect_track.tilt_degrees =
          static_cast<std::int16_t>(WithinFullTurn(effect_track.tilt_degrees + kDriftTiltStep));
      effect_track.azimuth_degrees = static_cast<std::int16_t>(
          WithinFullTurn(effect_track.azimuth_degrees + kDriftAzimuthStep));
      if (effect_track.ticks > kDriftFadesAfterTick) {
        effect_track.alpha = std::max<std::int16_t>(
            static_cast<std::int16_t>(effect_track.alpha - kDriftAlphaFall), 0);
        effect_track.radius_at_anchor *= kSpinShrink;
      }
    }
    effect_track.anchor -= ApproachStep(effect_track);
  }
}

}  // namespace ro::render
