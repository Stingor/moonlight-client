#include "arena_ground_synthetic.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/survivors_terrain.h"

#include "support/grf_builder.h"
#include "support/image_builder.h"
#include "support/map_builder.h"
#include "support/model_builder.h"

namespace moonclient::test_support {
namespace {

using ro::test_support::BmpPixel;
using ro::test_support::BuildBmp;
using ro::test_support::BuildGrf;
using ro::test_support::BuildRsm;
using ro::test_support::BuildRsw;
using ro::test_support::GrfFileSpec;
using ro::test_support::RsmBuildOptions;
using ro::test_support::RsmFaceSpec;
using ro::test_support::RsmMeshSpec;
using ro::test_support::RswBuildOptions;

// 32 texels : 8 par cube, assez fin pour que la terre des chemins se compose.
constexpr std::int32_t kTextureSide = 32;

constexpr std::array<GroundModelChoice, 1> kPyramid{{
    {.model = "essai\\pyramide.rsm", .scale = 1.0F, .weight = 1, .shade_cells = 2.0F},
}};
constexpr std::array<PieceModelSlot, 1> kCentred{{
    {.east_cells = 1.0F, .north_cells = 1.0F, .models = kPyramid},
}};
constexpr std::array<PieceModelSlot, 1> kWindmillCentre{{
    {.east_cells = 3.0F, .north_cells = 3.0F, .models = kPyramid},
}};
constexpr std::array<PieceModelSlot, 1> kWellCentre{{
    {.east_cells = 1.5F, .north_cells = 1.5F, .models = kPyramid},
}};
constexpr std::array<PieceModelSlot, 1> kRuinsWall{{
    {.east_cells = 1.5F, .north_cells = 3.5F, .models = kPyramid},
}};
constexpr std::array<PieceModelSlot, 1> kThicketCentre{{
    {.east_cells = 0.8F, .north_cells = 1.0F, .models = kPyramid},
}};
constexpr std::array<PieceLook, 7> kPieces{{
    {.key = "rock", .slots = kCentred},
    {.key = "tree", .slots = kCentred},
    {.key = "grove", .slots = kCentred},
    {.key = "thicket", .slots = kThicketCentre},
    {.key = "windmill", .slots = kWindmillCentre},
    {.key = "well", .slots = kWellCentre},
    {.key = "ruins", .slots = kRuinsWall},
}};
// Les chemins se peignent de la terre unie, carreau par carreau.
constexpr std::array<GroundTextureLook, 7> kTextures{{
    {.texture = "essai\\herbe.bmp", .weight = 3},
    {.texture = "essai\\prairie.bmp", .role = GroundTextureRole::kScatter},
    {.texture = "essai\\terre.bmp", .role = GroundTextureRole::kPatch},
    {.texture = "essai\\terre.bmp", .role = GroundTextureRole::kPathFull},
    {.texture = "essai\\terre.bmp", .role = GroundTextureRole::kPathEdge},
    {.texture = "essai\\terre.bmp", .role = GroundTextureRole::kPathOuter},
    {.texture = "essai\\terre.bmp", .role = GroundTextureRole::kPathInner},
}};
constexpr BiomeLook kLook{.terrain_key = "midgard_plains",
                          .reference_map = kSyntheticReferenceMap,
                          .textures = kTextures,
                          .pieces = kPieces,
                          .seeds = {},
                          .seeds_per_chunk = 0,
                          .patch_per_mille = 800,
                          .scatter_per_mille = 100};

std::vector<std::uint8_t> Pyramid() {
  RsmMeshSpec mesh;
  mesh.name = "corps";
  mesh.texture_indices = {0};
  // La base au sol, la pointe vers le haut (l'axe vertical du fichier descend).
  mesh.vertices = {
      {-4.0F, 0.0F, -4.0F}, {4.0F, 0.0F, -4.0F}, {0.0F, 0.0F, 4.0F}, {0.0F, -24.0F, 0.0F}};
  mesh.texture_vertices = {{0.0F, 0.0F}, {1.0F, 0.0F}, {0.0F, 1.0F}};
  const auto face = [](std::uint16_t a, std::uint16_t b, std::uint16_t c) {
    return RsmFaceSpec{.vertex_ids = {a, b, c},
                       .texture_vertex_ids = {0, 1, 2},
                       .texture_id = 0,
                       .two_sided = true};
  };
  mesh.faces = {face(0, 1, 3), face(1, 2, 3), face(2, 0, 3), face(0, 2, 1)};
  RsmBuildOptions options;
  options.textures = {"essai\\ecorce.bmp"};
  options.main_mesh_names = {"corps"};
  options.meshes = {mesh};
  return BuildRsm(options);
}

std::vector<std::uint8_t> Plain(BmpPixel color) {
  return BuildBmp(
      kTextureSide, kTextureSide,
      std::vector<BmpPixel>(static_cast<std::size_t>(kTextureSide * kTextureSide), color));
}

std::vector<std::uint8_t> Checker(BmpPixel light, BmpPixel dark) {
  std::vector<BmpPixel> pixels;
  for (std::int32_t y = 0; y < kTextureSide; ++y) {
    for (std::int32_t x = 0; x < kTextureSide; ++x)
      pixels.push_back((x + y) % 2 == 0 ? light : dark);
  }
  return BuildBmp(kTextureSide, kTextureSide, pixels);
}

}  // namespace

const BiomeLook& SyntheticPrairieLook() { return kLook; }

void FillSyntheticPrairie(const ro::test_support::TempDirectory& client) {
  RswBuildOptions reference;
  reference.light_longitude = 135;
  reference.light_latitude = 50;
  reference.light_ambient = {0.4F, 0.4F, 0.4F};
  reference.light_diffuse = {1.0F, 1.0F, 0.9F};
  client.WriteFile("essai.grf",
                   BuildGrf({GrfFileSpec{"data\\essai_ref.rsw", BuildRsw(reference)},
                             GrfFileSpec{"data\\model\\essai\\pyramide.rsm", Pyramid()},
                             GrfFileSpec{"data\\texture\\essai\\ecorce.bmp",
                                         Plain({.red = 40, .green = 90, .blue = 30})},
                             GrfFileSpec{"data\\texture\\essai\\herbe.bmp",
                                         Checker({.red = 90, .green = 170, .blue = 60},
                                                 {.red = 80, .green = 150, .blue = 55})},
                             GrfFileSpec{"data\\texture\\essai\\prairie.bmp",
                                         Plain({.red = 110, .green = 165, .blue = 70})},
                             GrfFileSpec{"data\\texture\\essai\\terre.bmp",
                                         Plain({.red = 140, .green = 105, .blue = 65})}}));
}

const ro::sim::TerrainRule& ShippedPlains() {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  const ro::sim::TerrainRule* found = nullptr;
  for (const ro::sim::ArenaRule& arena : rules.arenas) {
    if (arena.terrain_id != 0 && found == nullptr) {
      found = ro::sim::FindTerrain(rules, arena.terrain_id);
    }
  }
  REQUIRE(found != nullptr);
  return *found;
}

}  // namespace moonclient::test_support
