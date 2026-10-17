#include "ro/render/effect_aura_column.h"

#include <algorithm>
#include <cmath>

#include "ro/render/effect_quad_rotation.h"
#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

// LES QUATRE ANGLES DU CARRÉ, dans l'ordre où le client pousse ses coins.
//
// ⚠ CE SONT DES INDEX FIXES DANS SES TABLES, non quatre tables à part : voir
// l'en-tête. Les écrire en degrés est la façon de le dire une fois pour toutes.
constexpr std::array<std::int32_t, 4> kCornerDegrees = {135, 45, 315, 225};

// LE MÉLANGE : zéro est l'ADDITION, dans la numérotation de la voie A.
constexpr std::int16_t kAddedBlend = 0;

}  // namespace

std::vector<EffectWorldQuad> ComposeAuraColumnQuads(const EffectTrack& track,
                                                    std::int16_t turn_degrees,
                                                    std::int16_t tilt_degrees,
                                                    glm::vec3 node_origin,
                                                    const EffectOwnerTint& owner) {
  std::vector<EffectWorldQuad> quads;
  if (!track.active) {
    return quads;
  }

  const bool tight = track.mark[kMarkOfAuraColumnShape] == kAuraColumnTightMark;
  const float step = tight ? kAuraColumnTightStep : kAuraColumnWideStep;
  const float rise = tight ? kAuraColumnTightRise : kAuraColumnWideRise;
  const float offset = track.value[kSlotOfAuraColumnShift];
  const auto image = static_cast<std::size_t>(track.mark[kMarkOfAuraColumnImage]);

  // ⚠ DE LA PLUS LARGE À LA PLUS ÉTROITE, et c'est l'ordre du client : sa boucle
  // descend de dix à un. L'inverser changerait l'ordre de soumission des quads,
  // donc celui du mélange additif — qui n'en dépend pas —, mais surtout le rang
  // auquel la couche la plus claire se dessine.
  for (std::size_t layer = kAuraColumnLayers; layer >= 1; --layer) {
    // ⚠ L'ALPHA D'UNE COUCHE EST SA CASE, et il se TRONQUE : le client convertit
    // le flottant par une instruction de troncature avant de le comparer à zéro.
    const auto alpha = static_cast<std::int32_t>(track.value[layer]);
    if (alpha <= 0) {
      continue;
    }
    const float radius =
        static_cast<float>(kAuraColumnLayers - layer) * step + track.radius_at_anchor;

    EffectParticleQuadRequest request;
    for (std::size_t corner = 0; corner < request.corners.size(); ++corner) {
      const float angle = RadiansOfDegrees(kCornerDegrees[corner]);
      request.corners[corner] = {radius * std::cos(angle) + offset, 0.0F, radius * std::sin(angle)};
    }
    // ⚠ LES COINS SE TOURNENT AVANT DE MONTER, et non l'inverse : le client
    // applique sa rotation sur un carré posé à plat, puis retranche la montée à
    // l'ordonnée des quatre. Monter d'abord ferait pencher la colonne.
    // ⚠ ET LES DEUX SE FONT DANS LE REPÈRE DU CLIENT, qui descend : retrancher la
    // montée y élève la colonne. Le carré, tourné par les angles de la caméra
    // du client, revient au nôtre par `S` entier, montée comprise.
    RotateEffectQuadCorners(request.corners, turn_degrees, tilt_degrees);
    for (glm::vec3& corner : request.corners) {
      corner = EffectPointFromClient({corner.x, corner.y - rise, corner.z});
    }

    request.alpha = static_cast<std::uint8_t>(std::clamp(alpha, 0, 255));
    request.color = layer == kAuraColumnLayers ? kAuraColumnOuterTint : kAuraColumnInnerTint;
    request.blend_mode = kAddedBlend;
    request.texture = image;
    quads.push_back(ComposeEffectParticleQuad(request, node_origin, owner));
  }
  return quads;
}

}  // namespace ro::render
