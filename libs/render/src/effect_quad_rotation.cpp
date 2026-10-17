#include "ro/render/effect_quad_rotation.h"

#include <cmath>

#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

// L'angle d'abord ramené dans le tour, puis en radians : un angle négatif
// indexerait avant le début de la table du client.
[[nodiscard]] float RadiansWithinTurn(std::int16_t degrees) {
  return RadiansOfDegrees(WithinFullTurn(degrees));
}

// Une rotation dans le plan de deux composantes, telle que le client l'écrit :
// la première perd, la seconde gagne.
void TurnPair(float& first, float& second, float cosinus, float sinus) {
  const float before = first;
  const float after = second;
  first = (before * cosinus) - (after * sinus);
  second = (after * cosinus) + (before * sinus);
}

}  // namespace

glm::vec3 RotateEffectQuadPoint(glm::vec3 point, std::int16_t turn_degrees,
                                std::int16_t tilt_degrees) {
  const float tilt = RadiansWithinTurn(tilt_degrees);
  // L'INCLINAISON D'ABORD : voir l'avertissement de l'en-tête. Le tour travaille
  // ensuite sur la profondeur qu'elle vient de changer.
  TurnPair(point.z, point.y, std::cos(tilt), std::sin(tilt));

  const float tour = RadiansWithinTurn(turn_degrees);
  TurnPair(point.x, point.z, std::cos(tour), std::sin(tour));
  return point;
}

void RotateEffectQuadCorners(std::array<glm::vec3, 4>& corners, std::int16_t turn_degrees,
                             std::int16_t tilt_degrees) {
  for (glm::vec3& corner : corners) {
    corner = RotateEffectQuadPoint(corner, turn_degrees, tilt_degrees);
  }
}

void StoreEffectBillboardAngles(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                                std::int16_t turn_degrees, std::int16_t tilt_degrees) {
  tracks[0].style = turn_degrees;
  tracks[1].style = tilt_degrees;
}

}  // namespace ro::render
