#include "ro/render/camera.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include <glm/gtc/matrix_transform.hpp>

#include "ro/render/map_effect_math.h"

namespace ro::render {
namespace {

// Marge autour de l'emprise : sans elle, les sommets exactement sur le bord se
// retrouvent sur la limite du champ, où la moindre imprécision les fait sortir.
constexpr float kFramingMargin = 1.05F;

}  // namespace

glm::vec3 Camera::Position() const {
  const float pitch = glm::radians(std::clamp(pitch_degrees, -kMaxPitchDegrees, kMaxPitchDegrees));
  const float yaw = glm::radians(yaw_degrees);

  // Décomposition en deux temps : la distance projetée au sol, puis la hauteur.
  const float ground_distance = distance * std::cos(pitch);
  glm::vec3 eye = look_at + glm::vec3{ground_distance * std::sin(yaw), distance * std::sin(pitch),
                                      ground_distance * std::cos(yaw)};
  eye.y = std::max(eye.y, eye_floor_height);
  return eye;
}

glm::mat4 Camera::ViewMatrix() const {
  return glm::lookAt(Position(), look_at, glm::vec3{0.0F, 1.0F, 0.0F});
}

glm::mat4 Camera::ProjectionMatrix(float aspect_ratio) const {
  return glm::perspective(glm::radians(fov_degrees), aspect_ratio, z_near, z_far);
}

float ApparentScale(const Camera& camera) {
  const float half_field = std::tan(glm::radians(camera.fov_degrees) * 0.5F);
  const float reference = kGameViewDistance * std::tan(glm::radians(kGameFovDegrees) * 0.5F);
  const float here = camera.distance * half_field;
  // ⚠ UNE CAMÉRA DÉGÉNÉRÉE NE REND PAS L'INFINI : à distance ou champ nuls, il
  // n'y a pas d'échelle, et rendre un nombre énorme ferait enfler ce qui s'y
  // rapporte au lieu de le laisser tel quel.
  if (here <= 0.0F || reference <= 0.0F) {
    return 1.0F;
  }
  return reference / here;
}

float ScreenAnchorScale(float viewport_height, float apparent_scale, float body_scale) {
  // ⚠ UNE FENÊTRE DÉGÉNÉRÉE NE REND PAS ZÉRO : une hauteur nulle collerait tout
  // ce qui s'ancre aux pieds de son porteur, au lieu de le laisser où il est.
  const float window_share =
      viewport_height > 0.0F ? viewport_height / kClientReferenceHeight : 1.0F;
  return window_share * std::max(apparent_scale, 0.0F) * std::max(body_scale, 0.0F);
}

Camera FrameBounds(const std::array<float, 3>& bounds_min, const std::array<float, 3>& bounds_max,
                   float aspect_ratio, float fov_degrees, float pitch_degrees, float yaw_degrees) {
  Camera camera;
  camera.fov_degrees = fov_degrees;
  camera.pitch_degrees = pitch_degrees;
  camera.yaw_degrees = yaw_degrees;
  camera.look_at = {(bounds_min[0] + bounds_max[0]) * 0.5F, (bounds_min[1] + bounds_max[1]) * 0.5F,
                    (bounds_min[2] + bounds_max[2]) * 0.5F};

  // On raisonne sur la sphère englobante plutôt que sur la boîte : le résultat
  // ne dépend alors ni de l'orientation de la caméra ni de celle de la carte,
  // et il n'y a pas de cas particulier à traiter quand on tourne.
  const glm::vec3 extent{bounds_max[0] - bounds_min[0], bounds_max[1] - bounds_min[1],
                         bounds_max[2] - bounds_min[2]};
  const float radius = glm::length(extent) * 0.5F;
  if (radius <= 0.0F) {
    return camera;
  }

  // Le champ vertical est donné ; l'horizontal s'en déduit par le rapport
  // d'aspect. C'est le PLUS ÉTROIT des deux qui limite le cadrage.
  const float half_vertical = glm::radians(fov_degrees) * 0.5F;
  const float half_horizontal = std::atan(std::tan(half_vertical) * std::max(aspect_ratio, 0.01F));
  const float limiting = std::min(half_vertical, half_horizontal);

  camera.distance = radius / std::sin(limiting) * kFramingMargin;
  // Le plan lointain doit rester derrière la scène, sinon elle est tronquée.
  camera.z_far = std::max(camera.z_far, (camera.distance + radius) * 2.0F);
  return camera;
}

glm::vec3 ClientListenerRight(const Camera& camera) {
  constexpr float kHalfTurnDegrees = 180.0F;
  const auto angle = static_cast<std::int32_t>(kHalfTurnDegrees + camera.yaw_degrees);
  const float radians = NativeTableRadians(angle);
  return {-std::cos(radians), 0.0F, std::sin(radians)};
}

}  // namespace ro::render
