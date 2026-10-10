#include "ro/render/effect_radial_petal.h"

#include "ro/render/effect_particle_quad.h"
#include "ro/render/effect_quad_rotation.h"
#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

namespace ro::render {

void AdvanceRadialPetalTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                              std::int16_t billboard_turn_degrees,
                              std::int16_t billboard_tilt_degrees) {
  tracks[0].style = billboard_turn_degrees;
  tracks[1].style = billboard_tilt_degrees;

  for (EffectTrack& effect_track : tracks) {
    if (!effect_track.active) {
      continue;
    }
    ++effect_track.ticks;
    // ⚠ TANT QUE LE COMPTEUR N'EST PAS POSITIF, RIEN : ni course ni alpha.
    // C'est le retard que le semis a tiré.
    if (effect_track.ticks <= 0) {
      continue;
    }
    if (effect_track.ticks <= kRadialPetalDriftTicks) {
      // LA COURSE LE LONG DU CAP, à la vitesse de la piste.
      const float speed = effect_track.lift_key;
      effect_track.tail[kRadialPetalSlotOfSideways] +=
          speed * CosOfDegrees(effect_track.tilt_degrees);
      effect_track.tail[kRadialPetalSlotOfForward] +=
          speed * SinOfDegrees(effect_track.tilt_degrees);
      // LE SAUT : deux degrés par tic, replié une fois sur le tour. La hauteur
      // ne s'ACCUMULE pas : elle est RÉÉCRITE depuis la base à chaque tic.
      std::int32_t phase = kRadialPetalHopDegreesPerTick * effect_track.ticks;
      if (phase >= kEffectTrackFullTurnDegrees) {
        phase -= kEffectTrackFullTurnDegrees;
      }
      effect_track.tail[kRadialPetalSlotOfDepthBelow] =
          effect_track.anchor.y - (speed * SinOfDegrees(phase) * kRadialPetalHopScale);
    }
    if (effect_track.ticks <= kRadialPetalRisesUntilTick) {
      effect_track.alpha = static_cast<std::int16_t>(effect_track.alpha + kRadialPetalAlphaRise);
    }
    if (effect_track.ticks > kRadialPetalFadesAfterTick) {
      effect_track.alpha = static_cast<std::int16_t>(effect_track.alpha - kRadialPetalAlphaFall);
    }
    if (effect_track.alpha < 0) {
      effect_track.alpha = 0;
    }
  }
}

std::optional<EffectWorldQuad> ComposeRadialPetalQuad(const EffectTrack& track,
                                                      std::int16_t turn_degrees,
                                                      std::int16_t tilt_degrees,
                                                      glm::vec3 node_origin) {
  if (!track.active) {
    return std::nullopt;
  }

  // LE CARRÉ À PLAT, de demi-diagonale le rayon à l'ancre, puis redressé.
  std::array<glm::vec3, 4> quad;
  for (std::size_t corner = 0; corner < quad.size(); ++corner) {
    const std::int32_t angle = kRadialPetalFirstCornerDegrees +
                               (kRadialPetalDegreesPerCorner * static_cast<std::int32_t>(corner));
    quad[corner] = {track.radius_at_anchor * CosOfDegrees(angle), 0.0F,
                    track.radius_at_anchor * SinOfDegrees(angle)};
  }
  RotateEffectQuadCorners(quad, turn_degrees, tilt_degrees);

  const float de_cote = track.tail[kRadialPetalSlotOfSideways];
  const float en_dessous = track.tail[kRadialPetalSlotOfDepthBelow];
  const float forward = track.tail[kRadialPetalSlotOfForward];
  EffectParticleQuadRequest request;
  for (std::size_t corner = 0; corner < quad.size(); ++corner) {
    // ⚠ LE COIN ET SA POSITION SONT DANS LE REPÈRE DU CLIENT, tournés par les
    // angles de SA caméra : ils reviennent au nôtre ENSEMBLE, par `S` entier, au
    // dernier moment — comme chez le grain qui dérive, qui lit les mêmes cases.
    request.corners[corner] = EffectPointFromClient(
        {quad[corner].x + de_cote, quad[corner].y + en_dessous, quad[corner].z + forward});
  }
  request.absolute = false;  // 18e paramètre = 0
  request.alpha = static_cast<std::uint8_t>(track.alpha);
  request.blend_mode = kRadialPetalBlendMode;
  request.texture = 0;  // 17e paramètre = 0
  // ⚠ LES MÊMES NOMBRES DE TEXTURE QUE LE GRAIN, donc la même image retournée.
  request.v_at_a_and_b = 1.0F;
  request.v_at_c_and_d = 0.0F;
  return ComposeEffectParticleQuad(request, node_origin);
}

}  // namespace ro::render
