#include "ro/render/effect_triple_wall.h"

#include <cmath>

#include "ro/render/effect_particle_quad.h"
#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

// LE RAYON INTÉRIEUR vit dans la case zéro ; l'ÉPAISSEUR du mur est le rayon à
// l'ancre. Le second s'ajoute au premier pour donner le rayon extérieur.
constexpr std::size_t kSlotOfInnerRadius = 0;

}  // namespace

std::vector<EffectWorldQuad> ComposeTripleWallQuads(const EffectTrack& track, glm::vec3 anchor) {
  std::vector<EffectWorldQuad> quads;
  if (!track.active || track.alpha <= 0) {
    return quads;
  }

  const float inner = track.value[kSlotOfInnerRadius];
  const float outer = inner + track.radius_at_anchor;

  for (std::size_t copie = 0; copie < kTripleWallCopies; ++copie) {
    // Les trois copies : un degré avant l'azimut, l'azimut, un degré après.
    const auto offset = static_cast<std::int32_t>(copie) - kTripleWallSpreadDegrees;
    const std::int32_t angle = WithinFullTurn(track.azimuth_degrees + offset);
    const float theta = DegreesToRadians(static_cast<float>(angle));
    const float vers_x = std::cos(theta);
    // LA PROFONDEUR SE RETOURNE AVEC L'ORDONNÉE : n'en retourner qu'une ferait
    // du mur le miroir du sien, posé de l'autre côté de son ancre.
    const float vers_z = -std::sin(theta);

    // ⚠ LE MILIEU EST FRANC, LES CÔTÉS SONT DES OMBRES : le tiers de l'alpha, et
    // deux pentes plus douces. C'est cela, le halo.
    const bool in_middle = copie == 1;
    const std::int16_t alpha =
        in_middle ? track.alpha
                  : static_cast<std::int16_t>(track.alpha / kTripleWallSideAlphaDivisor);
    // ⚠ NOTRE REPÈRE MONTE, CELUI DU CLIENT DESCEND : là où il retranche ces
    // deux hauteurs, nous les ajoutons.
    const float inner_height =
        track.lift_key * (in_middle ? kTripleWallMiddleInnerRise : kTripleWallSideInnerRise);
    const float outer_height =
        track.lift_key * (in_middle ? kTripleWallMiddleOuterRise : kTripleWallSideOuterRise);

    EffectParticleQuadRequest request;
    // LES QUATRE COINS, en faisant le tour du rectangle : le bas intérieur, le
    // bas extérieur, le haut extérieur, le haut intérieur.
    request.corners = {glm::vec3{inner * vers_x, 0.0F, inner * vers_z},
                       glm::vec3{outer * vers_x, 0.0F, outer * vers_z},
                       glm::vec3{outer * vers_x, outer_height, outer * vers_z},
                       glm::vec3{inner * vers_x, inner_height, inner * vers_z}};
    request.alpha = static_cast<std::uint8_t>(alpha);
    quads.push_back(ComposeEffectParticleQuad(request, anchor));
  }
  return quads;
}

}  // namespace ro::render
