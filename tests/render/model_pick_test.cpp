// LE MODÈLE DU DÉCOR SOUS UN RAYON : ce que l'établi désigne d'un clic.

#include "ro/render/model_pick.h"

#include <array>
#include <optional>
#include <utility>

#include <catch2/catch_test_macros.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "ro/formats/gnd.h"
#include "ro/formats/rsw.h"

#include "support/map_builder.h"

using ro::render::MapScene;
using ro::render::ModelInstances;
using ro::render::PickSceneModel;
using ro::render::ScreenRay;

namespace {

MapScene EmptyScene() {
  auto gnd = ro::formats::GndMap::Parse(ro::test_support::BuildGnd({}));
  auto rsw = ro::formats::RswScene::Parse(ro::test_support::BuildRsw({}));
  REQUIRE(gnd.has_value());
  REQUIRE(rsw.has_value());
  return MapScene{.gnd = std::move(*gnd), .rsw = std::move(*rsw)};
}

// Un modèle d'emprise ±`half` autour de son origine, posé une fois en `at`.
ModelInstances Box(float half, const glm::vec3& at, std::size_t rank) {
  ModelInstances instances;
  instances.mesh.bounds_min = {-half, -half, -half};
  instances.mesh.bounds_max = {half, half, half};
  instances.transforms.push_back(glm::translate(glm::mat4(1.0F), at));
  instances.rsw_ranks.push_back(rank);
  return instances;
}

// Un rayon qui descend à la verticale sur `x`, `z`.
ScreenRay DownOn(float x, float z) {
  return ScreenRay{.origin = {x, 100.0F, z}, .direction = {0.0F, -1.0F, 0.0F}};
}

}  // namespace

TEST_CASE("Désignation : la plus petite boîte traversée gagne", "[render][model_pick]") {
  MapScene scene = EmptyScene();
  // Une falaise qui englobe un petit pont.
  scene.models.push_back(Box(20.0F, {0.0F, 0.0F, 0.0F}, 5));
  scene.models.push_back(Box(2.0F, {3.0F, 0.0F, 0.0F}, 7));
  CHECK(PickSceneModel(scene, DownOn(3.0F, 0.0F)) == std::optional<std::size_t>(7));
  // À côté du pont, seule la falaise est traversée.
  CHECK(PickSceneModel(scene, DownOn(-10.0F, 0.0F)) == std::optional<std::size_t>(5));
  // Hors des deux, rien.
  CHECK_FALSE(PickSceneModel(scene, DownOn(50.0F, 0.0F)).has_value());
}

TEST_CASE("Désignation : une boîte derrière l'œil ne se désigne pas", "[render][model_pick]") {
  MapScene scene = EmptyScene();
  scene.models.push_back(Box(2.0F, {0.0F, 0.0F, 0.0F}, 3));
  const ScreenRay away{.origin = {0.0F, 100.0F, 0.0F}, .direction = {0.0F, 1.0F, 0.0F}};
  CHECK_FALSE(PickSceneModel(scene, away).has_value());
}

TEST_CASE("Désignation : une pose sans rang de RSW ne se désigne pas", "[render][model_pick]") {
  MapScene scene = EmptyScene();
  scene.models.push_back(Box(2.0F, {0.0F, 0.0F, 0.0F}, ro::render::kNoRswRank));
  CHECK_FALSE(PickSceneModel(scene, DownOn(0.0F, 0.0F)).has_value());
}

TEST_CASE("Désignation : une pose animée se désigne aussi", "[render][model_pick]") {
  MapScene scene = EmptyScene();
  ModelInstances mill = Box(2.0F, {0.0F, 0.0F, 0.0F}, 0);
  mill.animated.push_back({.transforms = {mill.transforms[0]}, .rsw_ranks = {9}});
  mill.transforms.clear();
  mill.rsw_ranks.clear();
  scene.models.push_back(std::move(mill));
  CHECK(PickSceneModel(scene, DownOn(0.0F, 0.0F)) == std::optional<std::size_t>(9));
}

TEST_CASE("Désignation : la pose d'un rang se retrouve, avec les coins de sa boîte",
          "[render][model_pick]") {
  MapScene scene = EmptyScene();
  scene.models.push_back(Box(2.0F, {10.0F, 0.0F, 0.0F}, 4));
  const std::optional<ro::render::ScenePose> pose = ro::render::FindScenePose(scene, 4);
  REQUIRE(pose.has_value());
  const std::array<glm::vec3, 8> corners = ro::render::PoseCorners(*pose);
  // Le coin bas et le coin haut, portés par la translation.
  CHECK(corners[0] == glm::vec3(8.0F, -2.0F, -2.0F));
  CHECK(corners[7] == glm::vec3(12.0F, 2.0F, 2.0F));
  CHECK_FALSE(ro::render::FindScenePose(scene, 5).has_value());
}
