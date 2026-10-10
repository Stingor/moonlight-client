#include "ro/render/effect_quad_rotation.h"

#include <cmath>
#include <numbers>

#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

// Ramène un angle dans un tour, y compris quand il est négatif — le reste de la
// division entière du C++ garde le signe du dividende, et un angle négatif
// indexerait avant le début de la table du client.
[[nodiscard]] std::int32_t WithinFullTurn(std::int32_t degrees) {
  const std::int32_t remainder = degrees % kEffectTrackFullTurnDegrees;
  return remainder < 0 ? remainder + kEffectTrackFullTurnDegrees : remainder;
}

[[nodiscard]] float RadiansOfDegrees(std::int16_t degrees) {
  return static_cast<float>(WithinFullTurn(degrees)) * std::numbers::pi_v<float> / 180.0F;
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
  const float tilt = RadiansOfDegrees(tilt_degrees);
  // L'INCLINAISON D'ABORD : voir l'avertissement de l'en-tête. Le tour travaille
  // ensuite sur la profondeur qu'elle vient de changer.
  TurnPair(point.z, point.y, std::cos(tilt), std::sin(tilt));

  const float tour = RadiansOfDegrees(turn_degrees);
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
