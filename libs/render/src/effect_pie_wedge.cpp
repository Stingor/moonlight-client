#include "ro/render/effect_pie_wedge.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

[[nodiscard]] float DegreesToRadians(float degrees) {
  return degrees * std::numbers::pi_v<float> / 180.0F;
}

// LE MÉLANGE : zéro est l'ADDITION, dans la numérotation de la voie A.
constexpr std::int16_t kAddedBlend = 0;

// ⚠ LE REBOUCLAGE EST CELUI DU CLIENT, ET IL EST ASYMÉTRIQUE. Sous cinq degrés
// il AJOUTE trois cent cinquante-cinq au lieu de retrancher cinq ; au-dessus de
// trois cent cinquante-quatre il RETRANCHE trois cent cinquante-cinq au lieu
// d'ajouter cinq. Les deux donnent le même angle — c'est la même correction d'un
// tour, écrite en une addition au lieu de deux. La reproduire telle quelle est
// ce qui garantit qu'aucun cas de bord ne diverge.
[[nodiscard]] std::int32_t TiltBeforeWithinTurn(std::int32_t tilt) {
  return tilt >= kPieWedgeHalfOpeningDegrees
             ? tilt - kPieWedgeHalfOpeningDegrees
             : tilt + (kEffectTrackFullTurnDegrees - kPieWedgeHalfOpeningDegrees);
}

[[nodiscard]] std::int32_t TiltAfterWithinTurn(std::int32_t tilt) {
  return tilt < kEffectTrackFullTurnDegrees - kPieWedgeHalfOpeningDegrees
             ? tilt + kPieWedgeHalfOpeningDegrees
             : tilt - (kEffectTrackFullTurnDegrees - kPieWedgeHalfOpeningDegrees);
}

}  // namespace

std::vector<EffectWorldQuad> ComposePieWedgeQuads(const EffectTrack& track, glm::vec3 node_origin,
                                                  const EffectOwnerTint& owner) {
  std::vector<EffectWorldQuad> quads;
  if (!track.active || track.alpha <= 0) {
    return quads;
  }

  const float radius = track.radius_at_anchor;
  const float apex = track.value[kSlotOfPieWedgeApexHeight];
  // L'AZIMUT FAIT PIVOTER le quartier autour de l'axe vertical ; l'inclinaison
  // dit OÙ sur le cercle il se pose.
  const float vers_x = std::cos(DegreesToRadians(static_cast<float>(track.azimuth_degrees)));
  const float vers_y = std::sin(DegreesToRadians(static_cast<float>(track.azimuth_degrees)));

  const auto before = static_cast<float>(TiltBeforeWithinTurn(track.tilt_degrees));
  const auto after = static_cast<float>(TiltAfterWithinTurn(track.tilt_degrees));
  const float reach_before = radius * std::cos(DegreesToRadians(before));
  const float reach_after = radius * std::cos(DegreesToRadians(after));
  // ⚠ LA POINTE EST RÉPÉTÉE, et c'est ainsi que la voie A transporte un
  // triangle : elle ne sait émettre que des quads.
  // ⚠ LES FORMULES SONT CELLES DU CLIENT, dans son repère : l'apex négatif du
  // semis s'y dresse au-dessus de l'ancre. Chaque coin revient au nôtre.
  const glm::vec3 vertex = EffectPointFromClient({0.0F, apex, 0.0F});

  EffectParticleQuadRequest request;
  request.corners = {
      EffectPointFromClient({vers_x * reach_before, vers_y * reach_before + apex,
                             radius * std::sin(DegreesToRadians(before))}),
      EffectPointFromClient({vers_x * reach_after, vers_y * reach_after + apex,
                             radius * std::sin(DegreesToRadians(after))}),
      vertex,
      vertex,
  };
  request.alpha = static_cast<std::uint8_t>(std::clamp<std::int16_t>(track.alpha, 0, 255));
  request.color = {0, 0, kPieWedgeBlue};
  request.blend_mode = kAddedBlend;
  // ⚠ CETTE FIGURE PASSE TOUJOURS DEVANT TOUT : voir `EffectQuadDepth`.
  request.depth = EffectQuadDepth::kFrontmost;
  quads.push_back(ComposeEffectParticleQuad(request, node_origin, owner));
  return quads;
}

}  // namespace ro::render
