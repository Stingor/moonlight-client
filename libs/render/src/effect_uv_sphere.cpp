#include "ro/render/effect_uv_sphere.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "ro/render/effect_node.h"
#include "ro/render/effect_scene.h"

namespace ro::render {
namespace {

[[nodiscard]] float Radians(float degrees) { return degrees * std::numbers::pi_v<float> / 180.0F; }

[[nodiscard]] std::uint8_t Clamped(float channel) {
  return static_cast<std::uint8_t>(std::clamp(channel, 0.0F, kEffectNodeChannelMax));
}

// UN POINT DE LA SPHÈRE : `RotY(longitude) · RotX(latitude) · (0, 0, rayon)`.
//
// ⚠ NOTRE REPÈRE N'EST PAS CELUI DU CLIENT : son ordonnée descend, la nôtre
// monte, et la profondeur est retournée avec elle. Une latitude de moins
// quatre-vingt-dix est chez lui le pôle du BAS, et le reste ici — c'est ce qui
// met le bas de l'image sous le sol et son haut dans le dôme.
[[nodiscard]] glm::vec3 PointOn(float radius, float latitude_degrees, float longitude_degrees) {
  const float latitude = Radians(latitude_degrees);
  const float longitude = Radians(longitude_degrees);
  const float ring = radius * std::cos(latitude);
  return glm::vec3{ring * std::sin(longitude), radius * std::sin(latitude),
                   -ring * std::cos(longitude)};
}

}  // namespace

std::vector<EffectWorldQuad> ComposeEffectNodeUvSphere(const EffectNode& node, glm::vec3 anchor,
                                                       float yaw_degrees) {
  std::vector<EffectWorldQuad> quads;
  const float step = node.ring_step_degrees;
  // ⚠ UN RAYON NUL DONNE CINQUANTE QUADS SANS AIRE, et non rien : le client ne
  // les saute pas, et c'est ainsi que naît le dôme, d'un point qui s'étend.
  const float radius = node.ring_radius.half_size;
  if (!(step > 0.0F)) {
    return quads;
  }
  const std::array<std::uint8_t, 4> tint = {Clamped(node.color[0]), Clamped(node.color[1]),
                                            Clamped(node.color[2]), EffectNodeAlpha(node)};
  // L'image se déroule une fois autour et une fois du bas en haut : chaque
  // colonne en prend `pas / 360`, chaque rang `pas / 180`, comme le client
  // l'écrit (`1 / (360 / pas)`, `1 / (180 / pas)`).
  const float u_step = step / kUvSphereLongitudeSpanDegrees;
  const float v_step = step / kUvSphereLatitudeSpanDegrees;

  // ⚠ LES DEUX BOUCLES S'ARRÊTENT À « ÉTENDUE MOINS PAS », comme celles du
  // client : au pas de trente-six, cinq rangs et dix colonnes ferment la
  // sphère exactement.
  float v = 0.0F;
  for (float latitude = kUvSphereFirstLatitudeDegrees;
       latitude <= kUvSphereFirstLatitudeDegrees + kUvSphereLatitudeSpanDegrees - step;
       latitude += step) {
    float u = 0.0F;
    for (float longitude = 0.0F; longitude <= kUvSphereLongitudeSpanDegrees - step;
         longitude += step) {
      const float turned = longitude + yaw_degrees;
      EffectWorldQuad quad;
      quad.corners = {anchor + PointOn(radius, latitude, turned),
                      anchor + PointOn(radius, latitude, turned + step),
                      anchor + PointOn(radius, latitude + step, turned + step),
                      anchor + PointOn(radius, latitude + step, turned)};
      quad.texture_uv = {glm::vec2{u, v}, glm::vec2{u + u_step, v},
                         glm::vec2{u + u_step, v + v_step}, glm::vec2{u, v + v_step}};
      quad.tint = tint;
      // Le dessinateur passe le mode UN à la primitive, comme la couronne.
      quad.blend_source = kEffectNodeBlendSource;
      quad.blend_destination = kEffectNodeBlendDestination;
      quads.push_back(quad);
      u += u_step;
    }
    v += v_step;
  }
  return quads;
}

}  // namespace ro::render
