#include "ro/render/effect_spike_beam.h"

#include <algorithm>
#include <cmath>

#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

namespace ro::render {

std::vector<EffectWorldQuad> ComposeSpikeBeamQuads(const EffectTrack& track, glm::vec3 node_origin,
                                                   const EffectOwnerTint& owner) {
  std::vector<EffectWorldQuad> quads;
  if (!track.active || track.alpha <= 0) {
    return quads;
  }

  const float radius = track.lift_key;
  const float height = track.value[kSlotOfSpikeBeamHeight];
  const float cap = RadiansOfDegrees(track.azimuth_degrees);
  const float vers_x = std::cos(cap);
  const float vers_y = std::sin(cap);
  const float le_long_x = std::cos(RadiansOfDegrees(track.tilt_degrees));
  const float le_long_z = std::sin(RadiansOfDegrees(track.tilt_degrees));

  // LA SECTION DU FAISCEAU, calculée UNE FOIS : elle ne dépend pas du segment.
  // Chaque coin porte son écart au cap et sa hauteur ; c'est la portée qui les
  // fait avancer.
  std::array<glm::vec3, 4> section{};
  for (std::size_t corner = 0; corner < section.size(); ++corner) {
    const std::int32_t angle = WithinFullTurnOnce(static_cast<std::int32_t>(track.tilt_degrees) +
                                                  kSpikeBeamCornerDegrees[corner]);
    const float carries = radius * std::cos(RadiansOfDegrees(angle));
    section[corner] = {carries * vers_x, carries * vers_y + height,
                       radius * std::sin(RadiansOfDegrees(angle))};
  }

  const auto alpha = static_cast<std::uint8_t>(std::clamp<std::int16_t>(track.alpha, 0, 255));
  // ⚠ L'INDICE D'IMAGE SE TRONQUE, il ne s'arrondit pas : le client le convertit
  // par une instruction de troncature. Un indice de 2,9 désigne donc la troisième
  // image, non la quatrième — et c'est ce qui tient l'indice sous le nombre
  // d'images, que le mouvement fait juste atteindre avant de le ramener à zéro.
  const auto image = static_cast<std::int32_t>(track.value[kSlotOfSpikeBeamImage]);

  for (std::size_t segment = 0; segment < kSpikeBeamMaxSegments; ++segment) {
    const float reach = static_cast<float>(segment) * radius + track.radius_at_anchor;
    // ⚠ LA BORNE EST SUR LA PORTÉE, PAS SUR LE COMPTE : le client SORT de sa
    // boucle, il ne saute pas le segment. Un faisceau à grand pas s'arrête donc
    // avant son sixième.
    if (reach > kSpikeBeamMaxReach) {
      break;
    }
    // ⚠ ET UNE PORTÉE NULLE OU NÉGATIVE SE SAUTE SANS ARRÊTER LA BOUCLE. Les
    // deux règles ne sont pas la même : le bâtisseur peut poser un rayon de
    // départ négatif, et le premier segment ne paraît alors qu'au second tour.
    if (reach <= 0.0F) {
      continue;
    }

    EffectParticleQuadRequest request;
    for (std::size_t corner = 0; corner < section.size(); ++corner) {
      // ⚠ LA SECTION ET LA PORTÉE SONT CELLES DU CLIENT, dans son repère : la
      // hauteur négative du semis y élève le faisceau. Chaque coin revient au
      // nôtre, sans quoi le faisceau filait sous le sol.
      request.corners[corner] =
          EffectPointFromClient({section[corner].x + reach * le_long_x, section[corner].y,
                                 section[corner].z + reach * le_long_z});
    }
    request.alpha = alpha;
    request.color = kSpikeBeamTint;
    // LE MELANGE EST L ADDITION : le client passe zero au seizieme parametre.
    request.blend_mode = 0;
    request.texture = image < 0 ? 0U : static_cast<std::size_t>(image);
    quads.push_back(ComposeEffectParticleQuad(request, node_origin, owner));
  }
  return quads;
}

}  // namespace ro::render
