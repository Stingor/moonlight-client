#include "ro/render/effect_pentagon_trail.h"

#include <cmath>
#include <cstddef>

#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

// Le mode que la voie A entend comme une ADDITION : ce que les deux sites
// poussent. Voir `EffectParticleBlend`.
constexpr std::int16_t kAddedBlend = 0;

}  // namespace

std::vector<EffectWorldQuad> ComposePentagonTrailQuads(const EffectTrack& track,
                                                       const EffectOwnerTint& owner) {
  std::vector<EffectWorldQuad> quads;
  if (!track.active || track.alpha <= 0) {
    return quads;
  }

  // ⚠ LE RAYON EST LA CLÉ, et non le rayon à l'ancre : c'est l'ADR 0017, sur la
  // case même que l'anneau lit comme une clé de décalage. Le bâtisseur y met
  // deux, trois ou quatre selon la couche, et le mouvement l'y fait respirer.
  const float radius = track.lift_key;
  const glm::vec3 near_point{track.tail[kTailSlotOfNearCenterX], track.tail[kTailSlotOfNearCenterY],
                             track.tail[kTailSlotOfNearCenterZ]};
  const glm::vec3 far_point{track.tail[kTailSlotOfFarCenterX], track.tail[kTailSlotOfFarCenterY],
                            track.tail[kTailSlotOfFarCenterZ]};
  const std::array<std::uint8_t, 3> tint = track.mark[kMarkOfTrailShade] == kPentagonTrailBlue
                                               ? kPentagonTrailBlueColor
                                               : kPentagonTrailPaleColor;

  // LE SOMMET D'UN RANG, sur l'un des deux cercles.
  const auto vertex = [&](const glm::vec3& centre, std::int32_t rang) {
    std::int32_t angle = track.azimuth_degrees + kPentagonTrailStepDegrees * rang;
    // Le client ne ramène QU'UNE FOIS, et il lui suffit : l'azimut est déjà dans
    // le tour, et cinq pas de soixante-douze n'y ajoutent qu'un tour.
    DropOneTurn(angle);
    const float theta = DegreesToRadians(static_cast<float>(angle));
    // ⚠ LE CENTRE EST DANS NOTRE REPÈRE, l'écart au sommet s'écrit dans celui du
    // client : sa profondeur se retourne. La garder ferait du tube le miroir du
    // sien, qui tournerait à l'envers.
    return glm::vec3{centre.x + radius * std::cos(theta), centre.y,
                     centre.z - radius * std::sin(theta)};
  };

  quads.reserve(kPentagonTrailSides);
  for (std::int32_t cote = 1; cote <= static_cast<std::int32_t>(kPentagonTrailSides); ++cote) {
    EffectParticleQuadRequest request;
    request.corners = {vertex(near_point, cote - 1), vertex(near_point, cote),
                       vertex(far_point, cote), vertex(far_point, cote - 1)};
    // ⚠ ABSOLUS : les deux centres sont déjà dans le monde.
    request.absolute = true;
    request.alpha = static_cast<std::uint8_t>(track.alpha);
    request.color = tint;
    request.blend_mode = kAddedBlend;
    quads.push_back(ComposeEffectParticleQuad(request, glm::vec3{0.0F, 0.0F, 0.0F}, owner));
  }
  return quads;
}

}  // namespace ro::render
