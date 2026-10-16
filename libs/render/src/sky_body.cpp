#include "ro/render/sky_body.h"

#include <algorithm>
#include <cmath>

namespace ro::render {
namespace {

// LA DISTANCE OÙ L'ASTRE SE POSE pour être projeté : assez loin pour que la
// caméra ne s'en approche jamais, en deçà du plan lointain.
constexpr float kSkyBodyDistanceShare = 0.5F;

}  // namespace

SkyBodyOnScreen PlaceSkyBody(const glm::vec3& toward_body, const Camera& camera, float aspect_ratio,
                             float angular_radius_degrees) {
  const glm::vec3 body = glm::normalize(toward_body);
  const float altitude = std::asin(std::clamp(body.y, -1.0F, 1.0F));
  const glm::vec3 eye = camera.Position();
  const glm::vec4 clip = camera.ProjectionMatrix(aspect_ratio) * camera.ViewMatrix() *
                         glm::vec4(eye + body * camera.z_far * kSkyBodyDistanceShare, 1.0F);
  // DERRIÈRE LA CAMÉRA, il n'y a rien à peindre.
  if (clip.w <= 0.0F) {
    return SkyBodyOnScreen();
  }
  const float half_fov = glm::radians(camera.fov_degrees) * 0.5F;
  return SkyBodyOnScreen{
      .center = glm::vec2(clip) / clip.w,
      .radius = std::tan(glm::radians(angular_radius_degrees)) / std::tan(half_fov),
      .visibility = std::clamp(altitude / glm::radians(kSkyBodyFadeDegrees), 0.0F, 1.0F)};
}

}  // namespace ro::render
