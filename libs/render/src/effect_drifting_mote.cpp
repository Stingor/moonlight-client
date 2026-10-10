#include "ro/render/effect_drifting_mote.h"

#include "ro/render/effect_particle_quad.h"
#include "ro/render/effect_quad_rotation.h"
#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

namespace ro::render {

std::array<std::uint8_t, 3> EffectDriftingMoteColor(std::uint8_t mark) {
  if (mark == kDriftingMoteMarkBlue) {
    return kDriftingMoteBlue;
  }
  if (mark == kDriftingMoteMarkDim) {
    return kDriftingMoteDim;
  }
  return kDriftingMoteWhite;
}

std::optional<EffectWorldQuad> ComposeDriftingMoteQuad(const EffectTrack& track,
                                                       std::int16_t turn_degrees,
                                                       std::int16_t tilt_degrees,
                                                       glm::vec3 anchor) {
  if (!track.active) {
    return std::nullopt;
  }

  // LE CARRÉ DE DÉPART : à plat, tourné d'un huitième de tour, de la taille du
  // rayon à l'ancre — puis couché et tourné par les deux angles de caméra.
  std::array<glm::vec3, 4> quad;
  for (std::size_t corner = 0; corner < quad.size(); ++corner) {
    const std::int32_t angle = kDriftingMoteFirstCornerDegrees +
                               kDriftingMoteDegreesPerCorner * static_cast<std::int32_t>(corner);
    quad[corner] = {track.radius_at_anchor * CosOfDegrees(angle), 0.0F,
                    track.radius_at_anchor * SinOfDegrees(angle)};
  }
  RotateEffectQuadCorners(quad, turn_degrees, tilt_degrees);

  const float de_cote = track.tail[kDriftingMoteSlotOfSideways];
  const float en_dessous = track.tail[kDriftingMoteSlotOfDepthBelow];
  const float forward = track.tail[kDriftingMoteSlotOfForward];
  // ⚠ LE CLIENT TESTE LA HAUTEUR DU PREMIER COIN, pas celle du centre : un
  // carré dont le centre est au ras du nœud se montre déjà si son premier coin
  // est au-dessus. Tester le centre le ferait paraître un demi-carré trop tôt.
  if (quad[0].y + en_dessous > 0.0F) {
    return std::nullopt;
  }

  EffectParticleQuadRequest request;
  for (std::size_t corner = 0; corner < quad.size(); ++corner) {
    // ⚠ LE COIN ET SON DÉCALAGE SONT DANS LE REPÈRE DU CLIENT, tournés par les
    // angles de SA caméra : ils reviennent au nôtre ensemble, par `S` entier.
    request.corners[corner] = EffectPointFromClient(
        {quad[corner].x + de_cote, quad[corner].y + en_dessous, quad[corner].z + forward});
  }
  request.alpha = static_cast<std::uint8_t>(track.alpha);
  request.color = EffectDriftingMoteColor(track.mark[0]);
  // ⚠ L'IMAGE EST RETOURNÉE : le client donne un aux deux premiers coins et
  // zéro aux deux derniers, là où le défaut fait l'inverse.
  request.v_at_a_and_b = 1.0F;
  request.v_at_c_and_d = 0.0F;
  return ComposeEffectParticleQuad(request, anchor);
}

}  // namespace ro::render
