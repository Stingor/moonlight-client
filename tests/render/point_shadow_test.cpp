#include "ro/render/point_shadow.h"

#include <array>
#include <cmath>
#include <cstddef>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <glm/glm.hpp>

#include "ro/render/point_lights.h"

using Catch::Approx;
using ro::render::CubeFaceForward;
using ro::render::kCubeFaceCount;
using ro::render::kPointShadowClearanceWorld;
using ro::render::PointShadowDepth;
using ro::render::PointShadowFaceViewProjections;
using ro::render::PointShadowFar;

namespace {

constexpr float kFarWorld = 80.0F;
const glm::vec3 kLight{50.0F, 15.0F, -455.0F};

// Les coordonnées normalisées de `world` vues par `view_projection`.
glm::vec3 Normalized(const glm::mat4& view_projection, glm::vec3 world) {
  const glm::vec4 clip = view_projection * glm::vec4(world, 1.0F);
  return glm::vec3(clip) / clip.w;
}

// LA TABLE DES TEXTURES CUBIQUES d'OpenGL : pour une direction `ray` qui tombe
// sur la face `face`, la position du texel sur la face, de −1 à 1 — ce que
// la face doit avoir peint à cet endroit.
glm::vec2 CubeTableCoordinates(std::size_t face, glm::vec3 ray) {
  switch (face) {
    case 0:
      return glm::vec2(-ray.z, -ray.y) / std::abs(ray.x);
    case 1:
      return glm::vec2(ray.z, -ray.y) / std::abs(ray.x);
    case 2:
      return glm::vec2(ray.x, ray.z) / std::abs(ray.y);
    case 3:
      return glm::vec2(ray.x, -ray.z) / std::abs(ray.y);
    case 4:
      return glm::vec2(ray.x, -ray.y) / std::abs(ray.z);
    default:
      return glm::vec2(-ray.x, -ray.y) / std::abs(ray.z);
  }
}

}  // namespace

TEST_CASE("Chaque face de la carte cubique regarde son axe", "[render][point_shadow]") {
  const std::array<glm::mat4, kCubeFaceCount> faces =
      PointShadowFaceViewProjections(kLight, kFarWorld);
  constexpr float kAheadWorld = 20.0F;
  for (std::size_t face = 0; face < kCubeFaceCount; ++face) {
    CAPTURE(face);
    const glm::vec3 ahead = Normalized(faces[face], kLight + CubeFaceForward(face) * kAheadWorld);
    CHECK(ahead.x == Approx(0.0F).margin(1e-5F));
    CHECK(ahead.y == Approx(0.0F).margin(1e-5F));
    CHECK(ahead.z > -1.0F);
    CHECK(ahead.z < 1.0F);
    // Derrière la lumière, le point n'est pas devant la face.
    const glm::vec4 behind =
        faces[face] * glm::vec4(kLight - CubeFaceForward(face) * kAheadWorld, 1.0F);
    CHECK(behind.w < 0.0F);
  }
}

TEST_CASE("Une face peint chaque direction où la texture cubique la lira",
          "[render][point_shadow]") {
  const std::array<glm::mat4, kCubeFaceCount> faces =
      PointShadowFaceViewProjections(kLight, kFarWorld);
  // Un rayon de biais dans chaque face : l'axe, plus deux écarts inégaux sur
  // les deux autres axes, pour qu'une face retournée ou couchée se voie.
  constexpr float kAxisWorld = 10.0F;
  const glm::vec3 slant{3.0F, 6.0F, -4.0F};
  for (std::size_t face = 0; face < kCubeFaceCount; ++face) {
    CAPTURE(face);
    const glm::vec3 axis = CubeFaceForward(face);
    const glm::vec3 ray = (axis * kAxisWorld) + (slant - (axis * glm::dot(slant, axis)));
    const glm::vec3 painted = Normalized(faces[face], kLight + ray);
    const glm::vec2 read = CubeTableCoordinates(face, ray);
    CHECK(painted.x == Approx(read.x).margin(1e-5F));
    CHECK(painted.y == Approx(read.y).margin(1e-5F));
  }
}

TEST_CASE("PointShadowDepth rend la profondeur que la face écrit", "[render][point_shadow]") {
  const std::array<glm::mat4, kCubeFaceCount> faces =
      PointShadowFaceViewProjections(kLight, kFarWorld);
  CHECK(PointShadowDepth(kPointShadowClearanceWorld, kFarWorld) == Approx(0.0F).margin(1e-5F));
  CHECK(PointShadowDepth(kFarWorld, kFarWorld) == Approx(1.0F).margin(1e-5F));
  constexpr std::array<float, 3> kDistances = {9.0F, 12.5F, 60.0F};
  for (std::size_t face = 0; face < kCubeFaceCount; ++face) {
    for (const float distance_world : kDistances) {
      CAPTURE(face, distance_world);
      const glm::vec3 painted =
          Normalized(faces[face], kLight + CubeFaceForward(face) * distance_world);
      CHECK((painted.z * 0.5F) + 0.5F ==
            Approx(PointShadowDepth(distance_world, kFarWorld)).margin(1e-5F));
    }
  }
}

TEST_CASE("Ce qui touche la lumière, dans son dégagement, n'est pas peint dans sa carte",
          "[render][point_shadow]") {
  const std::array<glm::mat4, kCubeFaceCount> faces =
      PointShadowFaceViewProjections(kLight, kFarWorld);
  // Un écart de biais, petit devant le dégagement : le point reste dans la
  // face, à côté de l'axe.
  const glm::vec3 slant{0.25F, 0.5F, -0.25F};
  constexpr float kInsideShare = 0.9F;
  constexpr float kOutsideShare = 1.1F;
  for (std::size_t face = 0; face < kCubeFaceCount; ++face) {
    CAPTURE(face);
    const glm::vec3 axis = CubeFaceForward(face);
    const glm::vec3 aside = slant - (axis * glm::dot(slant, axis));
    const glm::vec4 inside =
        faces[face] *
        glm::vec4(kLight + aside + (axis * kPointShadowClearanceWorld * kInsideShare), 1.0F);
    const glm::vec4 outside =
        faces[face] *
        glm::vec4(kLight + aside + (axis * kPointShadowClearanceWorld * kOutsideShare), 1.0F);
    // En deçà du plan proche, OpenGL découpe : la vasque d'un brasero, posée
    // autour de la flamme, n'ombre rien.
    CHECK(inside.z < -inside.w);
    CHECK(outside.z > -outside.w);
  }
}

TEST_CASE("PointShadowFar garde la portée, sans s'approcher du plan proche",
          "[render][point_shadow]") {
  CHECK(PointShadowFar(kFarWorld) == kFarWorld);
  CHECK(PointShadowFar(0.0F) > kPointShadowClearanceWorld);
}
