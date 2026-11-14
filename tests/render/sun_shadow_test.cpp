#include "ro/render/sun_shadow.h"

#include <cmath>

#include <catch2/catch_test_macros.hpp>
#include <glm/glm.hpp>

#include "ro/render/camera.h"

using ro::render::Camera;
using ro::render::FrameSunShadow;
using ro::render::kGameElevationDegrees;
using ro::render::kGameFovDegrees;
using ro::render::kGameViewDistance;
using ro::render::kSunShadowCasterRise;
using ro::render::SunShadowFrame;

namespace {

constexpr float kAspectRatio = 16.0F / 9.0F;
constexpr int kMapTexels = 2048;
// Une carte de deux cents cubes de côté, au relief modeste.
const glm::vec3 kBoundsMin(0.0F, -50.0F, 0.0F);
const glm::vec3 kBoundsMax(2000.0F, 50.0F, 2000.0F);
// Un soleil à mi-hauteur, venu du sud-est.
const glm::vec3 kTowardSun = glm::normalize(glm::vec3(0.5F, 0.7F, 0.5F));

Camera GameCamera(const glm::vec3& look_at) {
  Camera camera;
  camera.look_at = look_at;
  camera.distance = kGameViewDistance;
  camera.pitch_degrees = kGameElevationDegrees;
  camera.fov_degrees = kGameFovDegrees;
  return camera;
}

glm::vec3 Projected(const SunShadowFrame& frame, const glm::vec3& world) {
  const glm::vec4 point = frame.view_projection * glm::vec4(world, 1.0F);
  return glm::vec3(point) / point.w;
}

bool InsideCube(const glm::vec3& ndc) {
  return std::abs(ndc.x) <= 1.0F && std::abs(ndc.y) <= 1.0F && std::abs(ndc.z) <= 1.0F;
}

}  // namespace

TEST_CASE("L'ombre du soleil couvre le point de visée et ce qui l'entoure", "[render][sun]") {
  const Camera camera = GameCamera({1000.0F, 0.0F, 1000.0F});
  const SunShadowFrame frame =
      FrameSunShadow(kTowardSun, camera, kAspectRatio, kBoundsMin, kBoundsMax, kMapTexels);

  CHECK(InsideCube(Projected(frame, camera.look_at)));
  CHECK(InsideCube(Projected(frame, camera.look_at + glm::vec3(100.0F, 0.0F, 100.0F))));
  // Des texels plus fins qu'une unité à la distance du jeu.
  CHECK(frame.texel_world > 0.0F);
  CHECK(frame.texel_world < 1.0F);
}

TEST_CASE("Le décor qui dépasse le terrain tient dans la profondeur, plus près du soleil",
          "[render][sun]") {
  const Camera camera = GameCamera({1000.0F, 0.0F, 1000.0F});
  const SunShadowFrame frame =
      FrameSunShadow(kTowardSun, camera, kAspectRatio, kBoundsMin, kBoundsMax, kMapTexels);

  const glm::vec3 ground(1000.0F, kBoundsMin.y, 1000.0F);
  const glm::vec3 tower_top(1000.0F, kBoundsMax.y + kSunShadowCasterRise * 0.5F, 1000.0F);
  const glm::vec3 ground_depth = Projected(frame, ground);
  const glm::vec3 tower_depth = Projected(frame, tower_top);
  CHECK(std::abs(ground_depth.z) <= 1.0F);
  CHECK(std::abs(tower_depth.z) <= 1.0F);
  CHECK(tower_depth.z < ground_depth.z);
}

TEST_CASE("Le cadre de l'ombre avance par texels entiers", "[render][sun]") {
  const glm::vec3 look_at(1000.0F, 0.0F, 1000.0F);
  const SunShadowFrame before = FrameSunShadow(kTowardSun, GameCamera(look_at), kAspectRatio,
                                               kBoundsMin, kBoundsMax, kMapTexels);
  // Un glissement de caméra bien plus court qu'un texel ne déplace pas la grille.
  const glm::vec3 slid = look_at + glm::vec3(before.texel_world * 0.1F, 0.0F, 0.0F);
  const SunShadowFrame after = FrameSunShadow(kTowardSun, GameCamera(slid), kAspectRatio,
                                              kBoundsMin, kBoundsMax, kMapTexels);

  const glm::vec3 fixed_point(1010.0F, 0.0F, 990.0F);
  const glm::vec3 seen_before = Projected(before, fixed_point);
  const glm::vec3 seen_after = Projected(after, fixed_point);
  // Une différence de position, en texels : un nombre entier.
  const float texels_x = (seen_after.x - seen_before.x) * 0.5F * static_cast<float>(kMapTexels);
  const float texels_y = (seen_after.y - seen_before.y) * 0.5F * static_cast<float>(kMapTexels);
  constexpr float kTolerance = 1e-2F;
  CHECK(std::abs(texels_x - std::round(texels_x)) < kTolerance);
  CHECK(std::abs(texels_y - std::round(texels_y)) < kTolerance);
}

TEST_CASE("Un soleil au zénith garde une vue définie", "[render][sun]") {
  const Camera camera = GameCamera({1000.0F, 0.0F, 1000.0F});
  const SunShadowFrame frame =
      FrameSunShadow({0.0F, 1.0F, 0.0F}, camera, kAspectRatio, kBoundsMin, kBoundsMax, kMapTexels);
  const glm::vec3 seen = Projected(frame, camera.look_at);
  CHECK(std::isfinite(seen.x));
  CHECK(InsideCube(seen));
}

TEST_CASE("L'ombre laisse à un sprite le rapport de l'ambiante à la pleine lumière du sol",
          "[render][sun]") {
  ro::render::SceneLight light;
  light.direction = kTowardSun;
  light.ambient = glm::vec3(0.4F);
  light.diffuse = glm::vec3(1.0F);
  const glm::vec3 shade = ro::render::SunShadeOf(light);
  CHECK(shade.r > 0.0F);
  CHECK(shade.r < 1.0F);
  CHECK(shade.r == shade.g);

  // Sans diffuse, le soleil n'apporte rien : son ombre ne retire rien.
  light.diffuse = glm::vec3(0.0F);
  const glm::vec3 unlit = ro::render::SunShadeOf(light);
  constexpr float kTolerance = 1e-5F;
  CHECK(std::abs(unlit.r - 1.0F) < kTolerance);
}

TEST_CASE("Sous une ambiante pleine, l'ombre du soleil retire encore sa part", "[render][sun]") {
  // La lumière de yuno : ambiante et diffuse pleines, qui saturent le sol.
  ro::render::SceneLight light;
  light.direction = kTowardSun;
  light.ambient = glm::vec3(1.0F);
  light.diffuse = glm::vec3(1.0F);
  const glm::vec3 shade = ro::render::SunShadeOf(light);
  CHECK(shade.r < 1.0F);
  constexpr float kTolerance = 1e-5F;
  CHECK(std::abs(shade.r - ro::render::kBrightestSkyAmbient) < kTolerance);
}
