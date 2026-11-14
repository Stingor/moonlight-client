#include "ro/render/model_bake.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <glm/gtc/matrix_transform.hpp>

using Catch::Approx;
using ro::render::BakedModel;
using ro::render::BakeInstances;
using ro::render::ModelMesh;
using ro::render::ModelSurface;
using ro::render::ModelVertex;

namespace {

constexpr float kQuarterTurnRadians = glm::half_pi<float>();
constexpr std::int32_t kFirstFamily = 0x0104;
constexpr std::uint32_t kTriangleVertices = 3;

// Trois triangles : deux sur la texture 1, un sur la texture 0, DANS CET ORDRE
// — la cuisson doit les regrouper.
ModelMesh ThreeTriangles() {
  ModelMesh mesh;
  for (int vertex = 0; vertex < 9; ++vertex) {
    mesh.vertices.push_back(ModelVertex{.position = {static_cast<float>(vertex), 0.0F, 0.0F},
                                        .texture_uv = {0.25F, 0.75F},
                                        .normal = {1.0F, 0.0F, 0.0F}});
  }
  mesh.surfaces = {
      ModelSurface{.texture_index = 1, .first_vertex = 0, .vertex_count = kTriangleVertices},
      ModelSurface{.texture_index = 0, .first_vertex = 3, .vertex_count = kTriangleVertices},
      ModelSurface{.texture_index = 1, .first_vertex = 6, .vertex_count = kTriangleVertices}};
  return mesh;
}

}  // namespace

TEST_CASE("Cuisson : une suite par texture, toutes poses confondues", "[render][model]") {
  const std::vector<glm::mat4> poses = {glm::mat4(1.0F),
                                        glm::translate(glm::mat4(1.0F), glm::vec3(0, 0, 50))};
  const BakedModel baked = BakeInstances(ThreeTriangles(), poses, kFirstFamily);
  REQUIRE(baked.runs.size() == 2);
  CHECK(baked.runs[0].texture_index == 0);
  CHECK(baked.runs[0].first_vertex == 0);
  CHECK(baked.runs[0].vertex_count == kTriangleVertices * 2);
  CHECK(baked.runs[1].texture_index == 1);
  CHECK(baked.runs[1].first_vertex == kTriangleVertices * 2);
  CHECK(baked.runs[1].vertex_count == kTriangleVertices * 2 * 2);
  CHECK(baked.vertices.size() == 9 * 2);
}

TEST_CASE("Cuisson : la position prend la pose entière, la normale sans translation",
          "[render][model]") {
  // Ce que fait le nuancier : `u_model * position`, et `mat3(u_model) * normale`.
  const glm::mat4 pose = glm::rotate(glm::translate(glm::mat4(1.0F), glm::vec3(10, 20, 30)),
                                     kQuarterTurnRadians, glm::vec3(0, 1, 0));
  ModelMesh mesh = ThreeTriangles();
  mesh.surfaces.resize(1);
  const BakedModel baked = BakeInstances(mesh, std::array{pose}, kFirstFamily);
  REQUIRE(baked.vertices.size() == kTriangleVertices);
  const ModelVertex& second = baked.vertices[1];
  const glm::vec4 expected = pose * glm::vec4(1, 0, 0, 1);
  CHECK(second.position[0] == Approx(expected.x));
  CHECK(second.position[1] == Approx(expected.y));
  CHECK(second.position[2] == Approx(expected.z));
  // Un quart de tour autour de Y envoie +X sur −Z ; la translation n'y est pas.
  CHECK(second.normal[0] == Approx(0.0F).margin(1e-6));
  CHECK(second.normal[1] == Approx(0.0F).margin(1e-6));
  CHECK(second.normal[2] == Approx(-1.0F));
  CHECK(second.texture_uv == std::array{0.25F, 0.75F});
}

TEST_CASE("Cuisson : une surface qui déborde du maillage est ignorée", "[render][model]") {
  ModelMesh mesh = ThreeTriangles();
  mesh.surfaces.push_back(ModelSurface{.texture_index = 2, .first_vertex = 8, .vertex_count = 3});
  const BakedModel baked = BakeInstances(mesh, std::array{glm::mat4(1.0F)}, kFirstFamily);
  CHECK(baked.vertices.size() == 9);
  for (const auto& run : baked.runs) {
    CHECK(run.texture_index != 2);
  }
}

TEST_CASE("Cuisson : chaque sommet porte le côté que sa face montre dans SA pose",
          "[render][model]") {
  // Deux faces simples et une double. Une pose ordinaire de la première famille
  // montre le sens trigonométrique, une pose en miroir l'autre ; la seconde
  // famille retourne la règle. Une face double se montre des deux côtés.
  using ro::render::FaceSide;
  ModelMesh mesh = ThreeTriangles();
  mesh.two_sided = {0, 1, 0};
  const std::vector<glm::mat4> poses = {glm::mat4(1.0F),
                                        glm::scale(glm::mat4(1.0F), glm::vec3(-1, 1, 1))};
  const auto side_of = [](const BakedModel& baked, std::size_t pose, std::size_t surface) {
    return baked.sides[baked.surfaces[surface].first_vertex +
                       pose * baked.surfaces[surface].vertices_per_pose];
  };
  const BakedModel first = BakeInstances(mesh, poses, kFirstFamily);
  REQUIRE(first.sides.size() == first.vertices.size());
  // La surface 0 est la texture 0, soit la deuxième face — la double.
  CHECK(side_of(first, 0, 0) == FaceSide::kBoth);
  CHECK(side_of(first, 0, 1) == FaceSide::kCounterClockwise);
  CHECK(side_of(first, 1, 1) == FaceSide::kClockwise);

  constexpr std::int32_t kSecondFamily = 0x0202;
  const BakedModel second = BakeInstances(mesh, poses, kSecondFamily);
  CHECK(side_of(second, 0, 1) == FaceSide::kCounterClockwise);
  CHECK(side_of(second, 1, 1) == FaceSide::kClockwise);
  const BakedModel ordinary = BakeInstances(
      mesh, std::array{glm::scale(glm::mat4(1.0F), glm::vec3(1, -1, 1))}, kSecondFamily);
  CHECK(side_of(ordinary, 0, 1) == FaceSide::kClockwise);
}

TEST_CASE("Cuisson : sans pose figée, rien à cuire", "[render][model]") {
  const BakedModel baked = BakeInstances(ThreeTriangles(), {}, kFirstFamily);
  CHECK(baked.vertices.empty());
  CHECK(baked.runs.empty());
}
