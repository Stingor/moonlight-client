#include "ro/render/sky_body.h"

#include <cmath>

#include <catch2/catch_test_macros.hpp>
#include <glm/glm.hpp>

#include "ro/render/camera.h"

using ro::render::Camera;
using ro::render::PlaceSkyBody;
using ro::render::SkyBodyOnScreen;

namespace {

constexpr float kTolerance = 1e-3F;
constexpr float kAspectRatio = 16.0F / 9.0F;
constexpr float kRadiusDegrees = 2.0F;

// Une caméra qui regarde vers le haut, comme la caméra sans bornes le permet.
Camera LookingUp(float pitch_degrees) {
  Camera camera;
  camera.distance = ro::render::kGameViewDistance;
  camera.pitch_degrees = pitch_degrees;
  camera.fov_degrees = ro::render::kGameFovDegrees;
  return camera;
}

}  // namespace

TEST_CASE("L'astre dans la visée se peint au centre de l'écran", "[render][sky]") {
  // L'élévation négative met l'œil sous le point visé : il regarde vers le haut.
  constexpr float kUpwardPitch = -30.0F;
  const Camera camera = LookingUp(kUpwardPitch);
  const glm::vec3 sight = glm::normalize(camera.look_at - camera.Position());
  const SkyBodyOnScreen seen = PlaceSkyBody(sight, camera, kAspectRatio, kRadiusDegrees);
  CHECK(std::abs(seen.center.x) < kTolerance);
  CHECK(std::abs(seen.center.y) < kTolerance);
  CHECK(seen.visibility == 1.0F);
  // Le rayon se compte en part de la demi-hauteur : deux degrés sur sept et demi.
  const float expected = std::tan(glm::radians(kRadiusDegrees)) /
                         std::tan(glm::radians(ro::render::kGameFovDegrees) * 0.5F);
  CHECK(std::abs(seen.radius - expected) < kTolerance);
}

TEST_CASE("L'astre derrière la caméra ou sous l'horizon ne se voit pas", "[render][sky]") {
  constexpr float kUpwardPitch = -30.0F;
  const Camera camera = LookingUp(kUpwardPitch);
  const glm::vec3 sight = glm::normalize(camera.look_at - camera.Position());
  CHECK(PlaceSkyBody(-sight, camera, kAspectRatio, kRadiusDegrees).visibility == 0.0F);
  const glm::vec3 on_horizon = glm::normalize(glm::vec3(sight.x, 0.0F, sight.z));
  CHECK(PlaceSkyBody(on_horizon, camera, kAspectRatio, kRadiusDegrees).visibility == 0.0F);
}
