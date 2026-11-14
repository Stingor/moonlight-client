#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "ro/render/model_pose_boxes.h"
#include "ro/render/point_shadow.h"
#include "ro/render/point_shadow_pass.h"
#include "ro/render/terrain_depth_chunks.h"
#include "ro/render/world_frustum.h"

using ro::render::BuildTerrainDepthChunks;
using ro::render::kTerrainDepthChunkWorld;
using ro::render::kVeilOpaque;
using ro::render::ShadowSight;
using ro::render::WorldBox;
using ro::render::WorldFrustum;

namespace {

const glm::vec3 kLight{100.0F, 20.0F, 100.0F};
constexpr float kFarWorld = 50.0F;
// La face +X d'OpenGL.
constexpr std::size_t kPositiveX = 0;

ShadowSight SightOfPositiveX() {
  const std::array<glm::mat4, ro::render::kCubeFaceCount> faces =
      ro::render::PointShadowFaceViewProjections(kLight, kFarWorld);
  return ShadowSight{
      .frustum = WorldFrustum::Of(faces[kPositiveX]), .center = kLight, .radius = kFarWorld};
}

// Une boîte d'un côté de deux unités, centrée sur `center`.
WorldBox BoxAround(glm::vec3 center) {
  return WorldBox{.lowest = center - glm::vec3(1.0F), .highest = center + glm::vec3(1.0F)};
}

}  // namespace

TEST_CASE("Une face d'ombre ne voit que devant elle, dans la portée de sa lumière",
          "[render][shadow_sight]") {
  const ShadowSight sight = SightOfPositiveX();
  CHECK(sight.Sees(BoxAround(kLight + glm::vec3(30.0F, 0.0F, 0.0F))));
  // Derrière la lumière, sur le côté hors du champ de 90°, ou au-delà de la portée.
  CHECK_FALSE(sight.Sees(BoxAround(kLight - glm::vec3(30.0F, 0.0F, 0.0F))));
  CHECK_FALSE(sight.Sees(BoxAround(kLight + glm::vec3(10.0F, 0.0F, 30.0F))));
  CHECK_FALSE(sight.Sees(BoxAround(kLight + glm::vec3(80.0F, 0.0F, 0.0F))));
  // Une sphère qui déborde dans le champ y compte.
  CHECK(sight.SeesSphere(kLight + glm::vec3(20.0F, 0.0F, 25.0F), 10.0F));
  CHECK_FALSE(sight.SeesSphere(kLight + glm::vec3(20.0F, 0.0F, 40.0F), 5.0F));
}

TEST_CASE("Une boîte portée par une pose l'est tout entière", "[render][shadow_sight]") {
  const WorldBox unit{.lowest = glm::vec3(-1.0F), .highest = glm::vec3(1.0F)};
  const glm::mat4 turned = glm::rotate(glm::translate(glm::mat4(1.0F), glm::vec3(10.0F)),
                                       glm::radians(45.0F), glm::vec3(0.0F, 1.0F, 0.0F));
  const WorldBox moved = ro::render::TransformedBox(unit, turned);
  CHECK(moved.lowest.x < 10.0F - 1.4F);
  CHECK(moved.highest.x > 10.0F + 1.4F);
  CHECK(moved.lowest.y == 9.0F);
}

TEST_CASE("Le sol se range en blocs, chaque triangle dans un seul", "[render][shadow_sight]") {
  // Deux triangles loin l'un de l'autre, dans deux blocs, sur deux surfaces.
  ro::render::TerrainMesh mesh;
  const auto vertex = [](float x, float z) {
    ro::render::TerrainVertex made;
    made.position = {x, 0.0F, z};
    return made;
  };
  const float far = kTerrainDepthChunkWorld * 3.0F;
  mesh.vertices = {vertex(1.0F, 1.0F), vertex(2.0F, 1.0F),      vertex(1.0F, 2.0F),
                   vertex(far, far),   vertex(far + 1.0F, far), vertex(far, far + 1.0F)};
  mesh.surfaces = {{.texture_index = 0, .first_vertex = 0, .vertex_count = 3},
                   {.texture_index = 1, .first_vertex = 3, .vertex_count = 3}};
  const ro::render::TerrainDepthChunks chunks = BuildTerrainDepthChunks(mesh);
  REQUIRE(chunks.chunks.size() == 2);
  CHECK(chunks.indices.size() == 6);
  for (const ro::render::TerrainDepthChunk& chunk : chunks.chunks) {
    REQUIRE(chunk.runs.size() == 1);
    CHECK(chunk.runs[0].index_count == 3);
  }
  CHECK(chunks.chunks[0].box.highest.x == 2.0F);
  CHECK(chunks.chunks[1].box.lowest.x == far);
}

TEST_CASE("Un bloc réunit les triangles d'une surface et garde l'ordre de la grille",
          "[render][shadow_sight]") {
  ro::render::TerrainMesh mesh;
  const auto vertex = [](float x, float z) {
    ro::render::TerrainVertex made;
    made.position = {x, 0.0F, z};
    return made;
  };
  const float next = kTerrainDepthChunkWorld;
  // La surface 0 : un triangle dans la colonne 1, deux dans la colonne 0.
  // La surface 1 : un triangle encore dans la colonne 0.
  mesh.vertices = {vertex(next + 1.0F, 1.0F), vertex(next + 2.0F, 1.0F), vertex(next + 1.0F, 2.0F),
                   vertex(1.0F, 1.0F),        vertex(2.0F, 1.0F),        vertex(1.0F, 2.0F),
                   vertex(3.0F, 1.0F),        vertex(4.0F, 1.0F),        vertex(3.0F, 2.0F),
                   vertex(5.0F, 1.0F),        vertex(6.0F, 1.0F),        vertex(5.0F, 2.0F)};
  mesh.surfaces = {{.texture_index = 0, .first_vertex = 0, .vertex_count = 9},
                   {.texture_index = 1, .first_vertex = 9, .vertex_count = 3}};
  const ro::render::TerrainDepthChunks chunks = BuildTerrainDepthChunks(mesh);
  REQUIRE(chunks.chunks.size() == 2);
  REQUIRE(chunks.chunks[0].runs.size() == 2);
  CHECK(chunks.chunks[0].runs[0].surface == 0);
  CHECK(chunks.chunks[0].runs[0].index_count == 6);
  CHECK(chunks.chunks[0].runs[1].surface == 1);
  CHECK(chunks.chunks[0].runs[1].first_index == 6);
  REQUIRE(chunks.chunks[1].runs.size() == 1);
  CHECK(chunks.chunks[1].runs[0].first_index == 9);
  CHECK(chunks.indices == std::vector<std::uint32_t>{3, 4, 5, 6, 7, 8, 9, 10, 11, 0, 1, 2});
}

TEST_CASE("La grille nomme tous les blocs qu'une sphère peut toucher, trous et débords compris",
          "[render][shadow_sight]") {
  // Un sol troué, dont de longs triangles débordent de leur bloc.
  constexpr int kPlacesPerSide = 12;
  constexpr float kOverhangWorld = 35.0F;
  constexpr int kHoleStride = 3;
  ro::render::TerrainMesh mesh;
  const auto add_vertex = [&mesh](float x, float z) {
    ro::render::TerrainVertex made;
    made.position = {x, 0.0F, z};
    mesh.vertices.push_back(made);
  };
  for (int column = 0; column < kPlacesPerSide; ++column) {
    for (int row = 0; row < kPlacesPerSide; ++row) {
      if ((column + row) % kHoleStride == 0) {
        continue;
      }
      // Les coins sont symétriques autour du milieu du bloc, où tombe le centre.
      const float x = (static_cast<float>(column) + 0.5F) * kTerrainDepthChunkWorld;
      const float z = (static_cast<float>(row) + 0.5F) * kTerrainDepthChunkWorld;
      add_vertex(x - kOverhangWorld, z);
      add_vertex(x + kOverhangWorld, z);
      add_vertex(x, z);
    }
  }
  mesh.surfaces = {{.texture_index = 0,
                    .first_vertex = 0,
                    .vertex_count = static_cast<std::uint32_t>(mesh.vertices.size())}};
  const ro::render::TerrainDepthChunks chunks = BuildTerrainDepthChunks(mesh);
  REQUIRE_FALSE(chunks.chunks.empty());
  CHECK(chunks.grid.overhang_world > 0.0F);

  constexpr int kCentersPerSide = 8;
  constexpr int kRadii = 3;
  const float side = static_cast<float>(kPlacesPerSide) * kTerrainDepthChunkWorld;
  for (int x_rank = -1; x_rank <= kCentersPerSide; ++x_rank) {
    for (int z_rank = -1; z_rank <= kCentersPerSide; ++z_rank) {
      for (int radius_rank = 0; radius_rank < kRadii; ++radius_rank) {
        const glm::vec3 center(static_cast<float>(x_rank) * side / kCentersPerSide, 0.0F,
                               static_cast<float>(z_rank) * side / kCentersPerSide);
        const float radius = 5.0F + static_cast<float>(radius_rank) * kTerrainDepthChunkWorld;
        std::vector<std::uint32_t> near;
        chunks.grid.ForEachNear(center, radius,
                                [&near](std::uint32_t rank) { near.push_back(rank); });
        CHECK(std::is_sorted(near.begin(), near.end()));
        for (std::uint32_t rank = 0; rank < chunks.chunks.size(); ++rank) {
          if (ro::render::SphereMeetsBox(center, radius, chunks.chunks[rank].box)) {
            CHECK(std::binary_search(near.begin(), near.end(), rank));
          }
        }
      }
    }
  }
}

TEST_CASE("La question sans écriture répond comme le relevé des poses", "[render][shadow_sight]") {
  ro::render::ModelInstances model;
  model.mesh.vertices = {ro::render::ModelVertex{.position = {-1.0F, -1.0F, -1.0F}},
                         ro::render::ModelVertex{.position = {1.0F, 1.0F, 1.0F}}};
  const glm::mat4 seen = glm::translate(glm::mat4(1.0F), kLight + glm::vec3(30.0F, 0.0F, 0.0F));
  const glm::mat4 behind = glm::translate(glm::mat4(1.0F), kLight - glm::vec3(30.0F, 0.0F, 0.0F));
  // Une pose figée vue, et une pose animée qu'on place devant ou derrière.
  model.transforms = {seen};
  model.animated.push_back(ro::render::ModelPlaybackGroup{});
  ro::render::ModelOpacities sighted;
  const auto agree = [&](const ro::render::ModelOpacities* casting, bool expected) {
    const std::array<ro::render::ModelPoseBoxes, 1> boxes = {ro::render::PoseBoxesOf(model)};
    const ShadowSight sight = SightOfPositiveX();
    CHECK(ro::render::SightedCasting(boxes, casting, sight, sighted) == expected);
    CHECK(ro::render::SeesMovingCaster(boxes, casting, sight) == expected);
  };
  // Aucune pose animée : la figée vue ne compte pas.
  agree(nullptr, false);
  model.animated[0].transforms = {behind};
  agree(nullptr, false);
  model.animated[0].transforms = {seen};
  agree(nullptr, true);
  // La pose animée vue ne porte pas ombre.
  const ro::render::ModelOpacities casting = {{kVeilOpaque, 0}};
  agree(&casting, false);
}

TEST_CASE("La clé d'ombre d'un acteur suit ce que le peintre applique à sa silhouette",
          "[render][shadow_sight]") {
  ro::render::ComposedActor composed;
  composed.quads.emplace_back();
  ro::render::DrawnActor drawn;
  drawn.actor = &composed;
  const ro::render::ActorShadowKey still = ro::render::ActorShadowKeyOf(drawn);
  CHECK(still == ro::render::ActorShadowKeyOf(drawn));
  const auto differs = [&](auto change) {
    ro::render::DrawnActor moved = drawn;
    change(moved);
    return !(still == ro::render::ActorShadowKeyOf(moved));
  };
  CHECK(differs([](ro::render::DrawnActor& at) { at.layer_spin_degrees = 90.0F; }));
  CHECK(differs([](ro::render::DrawnActor& at) { at.part_offset_pixels[0].x = 1.0F; }));
  CHECK(differs([](ro::render::DrawnActor& at) { at.screen_offset_pixels.y = 1.0F; }));
  CHECK(differs([](ro::render::DrawnActor& at) { at.grow_sprite_pixels = 1.0F; }));
  CHECK(differs([](ro::render::DrawnActor& at) { at.grow_screen_pixels = 1.0F; }));
  CHECK(differs([](ro::render::DrawnActor& at) { at.shown_parts = 0; }));
  CHECK(differs([](ro::render::DrawnActor& at) { at.tint[3] = 100; }));
  // La couleur de la teinte ne change rien à la silhouette.
  CHECK_FALSE(differs([](ro::render::DrawnActor& at) { at.tint[0] = 100; }));
}

TEST_CASE("Une face ne garde que les poses qu'elle voit et qui portent ombre",
          "[render][shadow_sight]") {
  ro::render::ModelInstances model;
  model.mesh.vertices = {ro::render::ModelVertex{.position = {-1.0F, -1.0F, -1.0F}},
                         ro::render::ModelVertex{.position = {1.0F, 1.0F, 1.0F}}};
  model.transforms = {glm::translate(glm::mat4(1.0F), kLight + glm::vec3(30.0F, 0.0F, 0.0F)),
                      glm::translate(glm::mat4(1.0F), kLight - glm::vec3(30.0F, 0.0F, 0.0F)),
                      glm::translate(glm::mat4(1.0F), kLight + glm::vec3(25.0F, 0.0F, 2.0F))};
  const std::array<ro::render::ModelPoseBoxes, 1> boxes = {ro::render::PoseBoxesOf(model)};
  REQUIRE(boxes[0].boxes.size() == 3);
  ro::render::ModelOpacities sighted;
  CHECK_FALSE(ro::render::SightedCasting(boxes, nullptr, SightOfPositiveX(), sighted));
  REQUIRE(sighted.size() == 1);
  CHECK(sighted[0] == std::vector<std::uint8_t>{kVeilOpaque, 0, kVeilOpaque});
  // La troisième ne porte pas ombre.
  const ro::render::ModelOpacities casting = {{kVeilOpaque, kVeilOpaque, 0}};
  static_cast<void>(ro::render::SightedCasting(boxes, &casting, SightOfPositiveX(), sighted));
  CHECK(sighted[0] == std::vector<std::uint8_t>{kVeilOpaque, 0, 0});
}
