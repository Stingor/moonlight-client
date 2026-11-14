#include "ro/render/point_lights.h"

#include <cstddef>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <glm/glm.hpp>

using ro::render::PointLight;
using ro::render::PointLightingAt;
using ro::render::PointLightsByDistance;
using ro::render::PointLightSet;

namespace {

constexpr float kRangeWorld = 50.0F;
const glm::vec3 kWhite{1.0F};

// Une lumière blanche posée à `distance_world` de l'origine, sur l'axe X.
PointLight LightAt(float distance_world) {
  return PointLight{.world_position = glm::vec3(distance_world, 0.0F, 0.0F),
                    .color = kWhite,
                    .range_world = kRangeWorld};
}

}  // namespace

TEST_CASE("PointLightsByDistance range les lumières de la plus proche à la plus lointaine",
          "[render][point_lights]") {
  const std::vector<PointLight> lights = {LightAt(30.0F), LightAt(-10.0F), LightAt(20.0F)};
  const PointLightSet set = PointLightsByDistance(lights, glm::vec3(0.0F));
  REQUIRE(set.lights.size() == lights.size());
  CHECK(set.lights[0].world_position.x == -10.0F);
  CHECK(set.lights[1].world_position.x == 20.0F);
  CHECK(set.lights[2].world_position.x == 30.0F);
}

TEST_CASE("PointLightsByDistance mesure la distance depuis l'oeil", "[render][point_lights]") {
  const std::vector<PointLight> lights = {LightAt(0.0F), LightAt(100.0F)};
  const PointLightSet set = PointLightsByDistance(lights, glm::vec3(90.0F, 0.0F, 0.0F));
  REQUIRE(set.lights.size() == lights.size());
  CHECK(set.lights[0].world_position.x == 100.0F);
}

TEST_CASE("PointLightsByDistance n'a pas de borne", "[render][point_lights]") {
  constexpr std::size_t kManyLights = 100;
  std::vector<PointLight> lights;
  // Les plus lointaines d'abord : l'ordre d'arrivée ne décide pas.
  for (std::size_t rank = kManyLights; rank > 0; --rank) {
    lights.push_back(LightAt(static_cast<float>(rank)));
  }
  const PointLightSet set = PointLightsByDistance(lights, glm::vec3(0.0F));
  REQUIRE(set.lights.size() == kManyLights);
  for (std::size_t rank = 0; rank < set.lights.size(); ++rank) {
    CHECK(set.lights[rank].world_position.x == static_cast<float>(rank + 1));
  }
}

TEST_CASE("PointLightsByDistance écarte les lumières sans couleur ou sans portée",
          "[render][point_lights]") {
  PointLight dark = LightAt(1.0F);
  dark.color = glm::vec3(0.0F);
  PointLight without_range = LightAt(2.0F);
  without_range.range_world = 0.0F;
  PointLight negative_range = LightAt(3.0F);
  negative_range.range_world = -kRangeWorld;
  const std::vector<PointLight> lights = {dark, without_range, negative_range, LightAt(40.0F)};
  const PointLightSet set = PointLightsByDistance(lights, glm::vec3(0.0F));
  REQUIRE(set.lights.size() == 1);
  CHECK(set.lights[0].world_position.x == 40.0F);
}

TEST_CASE("PointLightsByDistance rend un ensemble vide sans lumière", "[render][point_lights]") {
  CHECK(PointLightsByDistance({}, glm::vec3(0.0F)).lights.empty());
}

TEST_CASE("PointLightingAt rend la couleur pleine au centre et rien au-delà de la portée",
          "[render][point_lights]") {
  PointLightSet set;
  set.lights = {LightAt(0.0F)};
  CHECK(PointLightingAt(set, glm::vec3(0.0F)) == kWhite);
  CHECK(PointLightingAt(set, glm::vec3(kRangeWorld, 0.0F, 0.0F)) == glm::vec3(0.0F));
  CHECK(PointLightingAt(set, glm::vec3(0.0F, 0.0F, 2.0F * kRangeWorld)) == glm::vec3(0.0F));
}

TEST_CASE("PointLightingAt atténue par le carré de ce qui reste de la portée",
          "[render][point_lights]") {
  PointLightSet set;
  set.lights = {LightAt(0.0F)};
  // À mi-portée, il reste une moitié : un quart de la couleur.
  constexpr float kQuarter = 0.25F;
  const glm::vec3 lit = PointLightingAt(set, glm::vec3(0.0F, kRangeWorld / 2.0F, 0.0F));
  CHECK(lit.r == Catch::Approx(kQuarter));
  CHECK(lit.g == Catch::Approx(kQuarter));
  CHECK(lit.b == Catch::Approx(kQuarter));
}

TEST_CASE("PointLightingAt somme les sources", "[render][point_lights]") {
  PointLightSet set;
  set.lights = {LightAt(0.0F), LightAt(0.0F)};
  set.lights[1].color = glm::vec3(0.0F, 1.0F, 0.0F);
  CHECK(PointLightingAt(set, glm::vec3(0.0F)) == glm::vec3(1.0F, 2.0F, 1.0F));
  CHECK(PointLightingAt(PointLightSet(), glm::vec3(0.0F)) == glm::vec3(0.0F));
}
