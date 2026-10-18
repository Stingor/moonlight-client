#include "ro/render/effect_double_ribbon.h"

#include <algorithm>

#include "ro/render/effect_scene.h"
#include "ro/render/effect_track.h"
#include "ro/render/effect_trail_palette.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

// UN POINT DE L'ANNEAU, à une ouverture et à un angle de tour donnés.
//
// `axis` est la perpendiculaire au regard du porteur, rangée par le mouvement.
[[nodiscard]] glm::vec3 RingPoint(const EffectTrack& track, std::int32_t opening_degrees,
                                  std::int32_t turn_degrees, glm::vec3 axis) {
  const float radius = track.radius_at_anchor * SinOfDegrees(opening_degrees);
  const float de_cote = radius * SinOfDegrees(turn_degrees);
  const float vers_az = CosOfDegrees(track.azimuth_degrees);
  const float vers_az_z = SinOfDegrees(track.azimuth_degrees);
  // ⚠ LE CENTRE GLISSE AVEC L'OUVERTURE : trois fois l'axe, plus la clé fois le
  // cosinus de l'ouverture. Il est donc le plus loin quand l'anneau est un point.
  const float glissement = CosOfDegrees(opening_degrees) * track.lift_key;
  return {de_cote * vers_az + kDoubleRibbonCentreSpread * axis.x + axis.x * glissement,
          // ⚠ L'ANNEAU TOURNE AUTOUR D'UN CENTRE ÉLEVÉ DE LA CLÉ. Le client
          // retranche la clé dans son repère qui descend ; dans le nôtre, qui
          // monte, l'ordonnée change de signe.
          track.lift_key - radius * CosOfDegrees(turn_degrees),
          // ⚠ ET LA PROFONDEUR AVEC ELLE : l'azimut et l'axe sont ceux du client,
          // et n'en retourner que l'ordonnée poserait l'anneau en miroir, de
          // l'autre côté du porteur.
          -(de_cote * vers_az_z + kDoubleRibbonCentreSpread * axis.z + axis.z * glissement)};
}

}  // namespace

std::vector<EffectWorldQuad> ComposeDoubleRibbonQuads(const EffectTrack& track,
                                                      glm::vec3 node_origin,
                                                      std::uint8_t trail_variant,
                                                      std::size_t texture) {
  std::vector<EffectWorldQuad> quads;
  const std::int32_t sweep_degrees = track.degrees;
  if (!track.active || track.alpha <= 0 || sweep_degrees < 0) {
    return quads;
  }
  const std::int32_t step = sweep_degrees / kDoubleRibbonSegments;
  if (step <= 0) {
    // Voir l'en-tête : le client se figerait ici.
    return quads;
  }

  const auto ouverture = static_cast<std::int32_t>(track.value[kSlotOfDoubleRibbonOpening]);
  // ⚠ LE SECOND BORD SE PLAFONNE, PAS LE PREMIER : passé quatre-vingts degrés
  // d'ouverture, les deux bords se rejoignent et le ruban s'amincit jusqu'à
  // disparaître. Plafonner les deux ferait un ruban de largeur constante.
  const std::int32_t edge_opening =
      std::min(ouverture + kDoubleRibbonEdgeGapDegrees, kDoubleRibbonOpeningCeilingDegrees);
  const glm::vec3 axe{track.tail[kTailOfDoubleRibbonAxisX], 0.0F,
                      track.tail[kTailOfDoubleRibbonAxisZ]};

  const EffectTrailVariant tint = EffectTrailVariantFor(trail_variant);
  const std::int16_t retenu = tint.forced_alpha != 0 ? tint.forced_alpha : track.alpha;
  if (retenu <= 0) {
    return quads;
  }

  glm::vec3 previous_inner{0.0F, 0.0F, 0.0F};
  glm::vec3 previous_outer{0.0F, 0.0F, 0.0F};
  bool has_previous = false;
  std::int32_t travelled = 0;
  std::int32_t rang = 0;
  while (travelled <= sweep_degrees) {
    // ⚠ UNE SEULE SOUSTRACTION, ET PAS DE MODULO, comme partout dans cette
    // famille : le client retranche un tour quand la somme l'atteint.
    std::int32_t tour = track.tilt_degrees + travelled;
    DropOneTurn(tour);
    const glm::vec3 inner = node_origin + RingPoint(track, ouverture, tour, axe);
    const glm::vec3 outer = node_origin + RingPoint(track, edge_opening, tour, axe);

    if (has_previous) {
      const float u_before = static_cast<float>(rang) / kDoubleRibbonTextureSpan;
      const float u_after = static_cast<float>(rang + 1) / kDoubleRibbonTextureSpan;
      EffectWorldQuad quad;
      quad.corners = {previous_inner, inner, outer, previous_outer};
      quad.texture_uv = {glm::vec2{u_before, 1.0F}, glm::vec2{u_after, 1.0F},
                         glm::vec2{u_after, 0.0F}, glm::vec2{u_before, 0.0F}};
      quad.tint = {tint.color[0], tint.color[1], tint.color[2], static_cast<std::uint8_t>(retenu)};
      quad.blend_source = kEffectNodeBlendSource;
      quad.blend_destination = tint.blend_destination;
      quad.texture = texture;
      // ⚠ DEUX FOIS LE MÊME QUAD, et c'est le dessinateur qui le veut : en
      // mélange additif, la seconde passe double la luminosité du ruban.
      quads.push_back(quad);
      quads.push_back(quad);
      // ⚠ LE RANG DE TEXTURE S'ARRÊTE AU DERNIER SEGMENT et n'avance plus : le
      // client écrête son compteur à vingt. Un balayage plus long que vingt pas
      // reprendrait donc la même bande d'image jusqu'au bout.
      rang = std::min(rang + 1, kDoubleRibbonSegments - 1);
    }

    previous_inner = inner;
    previous_outer = outer;
    has_previous = true;
    travelled += step;
  }
  return quads;
}

}  // namespace ro::render
