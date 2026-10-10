#include "ro/render/effect_spinning_round.h"

#include <algorithm>
#include <cstddef>

#include "ro/render/effect_four_particle_slots.h"
#include "ro/render/effect_quad_burst.h"
#include "ro/render/effect_quad_rotation.h"
#include "ro/render/effect_track.h"
#include "ro/render/effect_track_spread.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

// LES SIGNES DU TREMBLEMENT, un quadrant par particule.
constexpr std::array<std::array<float, 2>, kSpinningRoundParticles> kJitterSigns = {{
    {1.0F, 1.0F},
    {-1.0F, -1.0F},
    {1.0F, -1.0F},
    {-1.0F, 1.0F},
}};

}  // namespace

glm::vec3 EffectSpinningRoundOffsetOf(const EffectTrack& track, std::size_t particle) {
  return EffectFourParticleOffsetOf(track, particle);
}

void SetEffectSpinningRoundOffset(EffectTrack& track, std::size_t particle, glm::vec3 offset) {
  SetEffectFourParticleOffset(track, particle, offset);
}

std::array<std::uint8_t, 3> EffectSpinningRoundColor(std::uint8_t mark) {
  return mark == kSpinningRoundGreenMark ? kSpinningRoundGreen : kSpinningRoundWhite;
}

std::int16_t EffectSpinningRoundAlpha(std::uint8_t mark, float height) {
  // ⚠ LA HAUTEUR EST DANS LE REPÈRE DU CLIENT, qui DESCEND : plus elle est
  // négative, plus la particule est haut, et plus l'alpha tombe.
  if (height < kSpinningRoundFadeBelow) {
    const float rampe = (height - kSpinningRoundFadeBelow) * kSpinningRoundFadePerUnit +
                        static_cast<float>(kSpinningRoundWhiteAlpha);
    return std::max<std::int16_t>(static_cast<std::int16_t>(rampe), 0);
  }
  return mark == kSpinningRoundGreenMark ? kSpinningRoundGreenAlpha : kSpinningRoundWhiteAlpha;
}

std::vector<EffectWorldQuad> ComposeSpinningRoundQuads(const EffectTrack& track,
                                                       std::int16_t turn_degrees,
                                                       std::int16_t tilt_degrees, glm::vec3 anchor,
                                                       const EffectOwnerTint& owner) {
  std::vector<EffectWorldQuad> quads;
  if (!track.active) {
    return quads;
  }

  // LE CARRÉ DE DÉPART : à plat, tourné d'un huitième de tour, de la taille du
  // rayon à l'ancre — puis couché et tourné par les deux angles de caméra.
  std::array<glm::vec3, 4> quad;
  for (std::size_t corner = 0; corner < quad.size(); ++corner) {
    const std::int32_t angle = kQuadBurstFirstCornerDegrees +
                               kQuadBurstDegreesPerCorner * static_cast<std::int32_t>(corner);
    quad[corner] = {track.radius_at_anchor * CosOfDegrees(angle), 0.0F,
                    track.radius_at_anchor * SinOfDegrees(angle)};
  }
  RotateEffectQuadCorners(quad, turn_degrees, tilt_degrees);

  const std::uint8_t style_mark = track.mark[kSpinningRoundMarkOfColor];
  quads.reserve(kSpinningRoundParticles);
  for (std::size_t particle_index = 0; particle_index < kSpinningRoundParticles; ++particle_index) {
    const glm::vec3 shift = EffectSpinningRoundOffsetOf(track, particle_index);
    // ⚠ LA HAUTEUR QUI DÉCIDE DE TOUT EST CELLE DU PREMIER COIN, pas celle du
    // centre : le client compare le coin A translaté, et rien d'autre.
    const float lift = quad[0].y + shift.y;
    if (lift > 0.0F) {
      continue;
    }
    EffectParticleQuadRequest request;
    for (std::size_t corner = 0; corner < quad.size(); ++corner) {
      // ⚠ LE COIN ET SON DÉCALAGE SONT DANS LE REPÈRE DU CLIENT, tournés par
      // les angles de SA caméra : ils reviennent au nôtre ENSEMBLE, par `S`
      // entier, au dernier moment.
      request.corners[corner] = EffectPointFromClient(quad[corner] + shift);
    }
    request.alpha = static_cast<std::uint8_t>(EffectSpinningRoundAlpha(style_mark, lift));
    request.color = EffectSpinningRoundColor(style_mark);
    // ⚠ L'IMAGE EST RETOURNÉE : le client donne un aux deux premiers coins et
    // zéro aux deux derniers, là où le défaut fait l'inverse.
    request.v_at_a_and_b = 1.0F;
    request.v_at_c_and_d = 0.0F;
    quads.push_back(ComposeEffectParticleQuad(request, anchor, owner));
  }
  return quads;
}

void AdvanceSpinningRoundTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                                std::int16_t camera_turn_degrees, std::int16_t camera_tilt_degrees,
                                std::uint32_t& generator) {
  // ⚠ UNE SEULE FOIS PAR TIC, ET NON PAR PISTE : le client pose les deux angles
  // avant sa boucle, comme pour la rafale.
  StoreEffectBillboardAngles(tracks, camera_turn_degrees, camera_tilt_degrees);

  for (EffectTrack& effect_track : tracks) {
    if (!effect_track.active) {
      continue;
    }
    ChaseEffectTrackAngles(effect_track, kSpinningRoundAnglePairs, generator,
                           kSpinningRoundAngleFall, kSpinningRoundAngleRise);

    // LES QUATRE ANGLES DE RONDE DESCENDENT D'UN DEGRÉ et rattrapent sous zéro.
    for (std::size_t particle_index = 0; particle_index < kSpinningRoundParticles;
         ++particle_index) {
      float& angle = effect_track.value[kSpinningRoundSlotOfFirstSpin + particle_index];
      angle -= kSpinningRoundSpinStep;
      if (angle < 0.0F) {
        angle += static_cast<float>(kEffectTrackFullTurnDegrees);
      }
    }

    // LE TREMBLEMENT : les quatre particules partagent leurs deux courants et ne
    // diffèrent que par le quadrant qu'elles leur donnent.
    const float jitter_x =
        SinOfDegrees(static_cast<std::int32_t>(effect_track.value[0])) * kSpinningRoundJitter;
    const float jitter_z =
        SinOfDegrees(static_cast<std::int32_t>(effect_track.value[2])) * kSpinningRoundJitter;
    for (std::size_t particle_index = 0; particle_index < kSpinningRoundParticles;
         ++particle_index) {
      const auto angle = static_cast<std::int32_t>(
          effect_track.value[kSpinningRoundSlotOfFirstSpin + particle_index]);
      glm::vec3 shift = EffectSpinningRoundOffsetOf(effect_track, particle_index);
      shift.x = kSpinningRoundOrbitRadius * SinOfDegrees(angle) +
                kJitterSigns[particle_index][0] * jitter_x;
      shift.z = kSpinningRoundOrbitRadius * CosOfDegrees(angle) +
                kJitterSigns[particle_index][1] * jitter_z;
      // ⚠ LA HAUTEUR N'EST JAMAIS ÉCRITE : elle garde celle du semis, et c'est
      // ce qui rend la garde du dessinateur toujours vraie.
      SetEffectSpinningRoundOffset(effect_track, particle_index, shift);
    }
  }
}

}  // namespace ro::render
