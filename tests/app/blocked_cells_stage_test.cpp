// LES CASES BLOQUÉES DANS LA CARTE EN COURS : ce que le document d'édition
// bloque, la grille de marche le refuse et le quadrillage de GreyWorld le
// peint. Voir docs/adr/0131.

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <glm/glm.hpp>

#include "ro/formats/gat.h"
#include "ro/formats/gnd.h"
#include "ro/formats/rsw.h"
#include "ro/mapedit/map_edit.h"
#include "ro/render/grey_world.h"
#include "ro/render/ground_quad.h"
#include "ro/render/map_scene.h"
#include "ro/render/skill_area.h"
#include "ro/render/terrain_mesh.h"
#include "ro/sim/walk_map.h"

#include "support/map_builder.h"
#include "world/map_stage.h"

using moonclient::BlockStageCells;
using moonclient::ComposeStageGreyWorld;
using moonclient::MapStage;
using moonclient::PlannedMapOf;
using ro::mapedit::CellBlock;
using ro::mapedit::MapEdit;
using ro::render::GreyWorldLook;
using ro::render::GroundQuad;

namespace {

// Quatre cubes sur quatre, donc huit cases sur huit, toutes sur du sol.
constexpr std::int32_t kCubesPerSide = 4;
constexpr std::int32_t kCellsPerSide = 8;
constexpr std::int32_t kWallType = 1;
// Le mur du fichier, puis ce que le document bloque : un mur et une barrière
// sur du sol marchable.
constexpr std::int32_t kFileWallX = 2;
constexpr std::int32_t kFileWallY = 2;
constexpr std::int32_t kAddedWallX = 4;
constexpr std::int32_t kAddedWallY = 5;
constexpr std::int32_t kAddedFenceX = 6;
constexpr std::int32_t kAddedFenceY = 1;
// Le joueur au milieu : la plus petite portée du quadrillage couvre la carte.
constexpr std::int32_t kCenter = 3;

ro::render::MapScene GroundedMap() {
  ro::test_support::GndBuildOptions ground;
  ground.width = kCubesPerSide;
  ground.height = kCubesPerSide;
  ground.texture_paths = {"sol.bmp"};
  ground.tile_count = 1;
  ground.cubes.assign(static_cast<std::size_t>(kCubesPerSide * kCubesPerSide),
                      ro::test_support::GndCubeSpec{.tile_up = 0});
  auto gnd = ro::formats::GndMap::Parse(ro::test_support::BuildGnd(ground));
  auto rsw = ro::formats::RswScene::Parse(ro::test_support::BuildRsw({}));
  REQUIRE(gnd.has_value());
  REQUIRE(rsw.has_value());
  return ro::render::MapScene{.gnd = std::move(*gnd), .rsw = std::move(*rsw)};
}

// Une carte et sa grille, comme une carte lue : tout marchable, sauf un mur.
MapStage StageWithGrid() {
  std::vector<ro::test_support::GatCellSpec> cells(
      static_cast<std::size_t>(kCellsPerSide * kCellsPerSide));
  cells[static_cast<std::size_t>(kFileWallY * kCellsPerSide + kFileWallX)].type = kWallType;
  auto gat =
      ro::formats::GatMap::Parse(ro::test_support::BuildGat(kCellsPerSide, kCellsPerSide, cells));
  REQUIRE(gat.has_value());
  MapStage stage{"cases", GroundedMap(), ro::sim::WalkMap::FromGat(*gat)};
  stage.gat = std::move(*gat);
  return stage;
}

MapEdit BlockingDocument() {
  MapEdit document;
  document.BlockCell({.cell_x = kAddedWallX, .cell_y = kAddedWallY, .block = CellBlock::kWall});
  document.BlockCell({.cell_x = kAddedFenceX, .cell_y = kAddedFenceY, .block = CellBlock::kFence});
  return document;
}

GreyWorldLook EnabledLook() {
  GreyWorldLook look;
  look.enabled = true;
  return look;
}

// La couleur du quad posé sur cette case, ou rien quand aucun ne s'y pose.
[[nodiscard]] std::optional<std::array<std::uint8_t, 4>> ColorAt(
    const MapStage& stage, const std::vector<GroundQuad>& quads, std::int32_t cell_x,
    std::int32_t cell_y) {
  const std::array<float, 3> center = ro::render::CellPositionAt(
      stage.scene.gnd, static_cast<float>(cell_x), static_cast<float>(cell_y));
  constexpr float kCorners = 4.0F;
  for (const GroundQuad& quad : quads) {
    float x = 0.0F;
    float z = 0.0F;
    for (const glm::vec3& corner : quad.corners) {
      x += corner.x;
      z += corner.z;
    }
    if (x / kCorners == Catch::Approx(center[0]) && z / kCorners == Catch::Approx(center[2])) {
      return quad.color;
    }
  }
  return std::nullopt;
}

}  // namespace

TEST_CASE("Cases bloquées, carte : la grille de marche refuse ce que le document bloque",
          "[app][mapedit]") {
  MapStage stage = StageWithGrid();
  const ro::sim::WalkMap* planned = PlannedMapOf(stage);
  REQUIRE(planned != nullptr);
  REQUIRE(planned->Passable(kAddedWallX, kAddedWallY));
  REQUIRE(planned->Passable(kAddedFenceX, kAddedFenceY));
  REQUIRE_FALSE(planned->Passable(kFileWallX, kFileWallY));

  MapEdit document = BlockingDocument();
  CHECK(BlockStageCells(stage, document.blocked_cells()));
  // EN PLACE : la grille que le planificateur désigne est restée la même.
  CHECK(PlannedMapOf(stage) == planned);
  CHECK_FALSE(planned->Passable(kAddedWallX, kAddedWallY));
  CHECK_FALSE(planned->Passable(kAddedFenceX, kAddedFenceY));
  CHECK(planned->Passable(kAddedWallX + 1, kAddedWallY));
  // La grille lue garde les types du fichier.
  CHECK(stage.gat->at(kAddedWallX, kAddedWallY).type == 0);
  CHECK(stage.gat->at(kAddedFenceX, kAddedFenceY).type == 0);
  // Rien n'a changé depuis : rien ne se refait.
  CHECK_FALSE(BlockStageCells(stage, document.blocked_cells()));

  // « Libérer » rend la case, et un document vidé rend la grille du fichier,
  // mur compris.
  document.FreeCell(kAddedWallX, kAddedWallY);
  CHECK(BlockStageCells(stage, document.blocked_cells()));
  CHECK(planned->Passable(kAddedWallX, kAddedWallY));
  CHECK_FALSE(planned->Passable(kAddedFenceX, kAddedFenceY));
  document.ClearBlockedCells();
  CHECK(BlockStageCells(stage, document.blocked_cells()));
  CHECK(planned->Passable(kAddedFenceX, kAddedFenceY));
  CHECK_FALSE(planned->Passable(kFileWallX, kFileWallY));
}

TEST_CASE("Cases bloquées, carte : le quadrillage de GreyWorld les peint, et suit le document",
          "[app][mapedit][greyworld]") {
  MapStage stage = StageWithGrid();
  const GreyWorldLook look = EnabledLook();
  const auto walk = ro::render::SkillAreaColorBytes(look.walk_color);
  const auto wall = ro::render::SkillAreaColorBytes(look.wall_color);
  const auto snipe = ro::render::SkillAreaColorBytes(look.snipe_color);
  std::vector<GroundQuad> quads;

  // La carte du fichier : un seul mur.
  ComposeStageGreyWorld(stage, kCenter, kCenter, look, quads);
  REQUIRE(quads.size() == static_cast<std::size_t>(kCellsPerSide * kCellsPerSide));
  CHECK(ColorAt(stage, quads, kFileWallX, kFileWallY) == wall);
  CHECK(ColorAt(stage, quads, kAddedWallX, kAddedWallY) == walk);
  CHECK(ColorAt(stage, quads, kAddedFenceX, kAddedFenceY) == walk);

  // Le document bloque : le mur en mur, la barrière en case tirable.
  MapEdit document = BlockingDocument();
  REQUIRE(BlockStageCells(stage, document.blocked_cells()));
  ComposeStageGreyWorld(stage, kCenter, kCenter, look, quads);
  CHECK(ColorAt(stage, quads, kAddedWallX, kAddedWallY) == wall);
  CHECK(ColorAt(stage, quads, kAddedFenceX, kAddedFenceY) == snipe);
  CHECK(ColorAt(stage, quads, kFileWallX, kFileWallY) == wall);

  // 🔴 LE QUADRILLAGE ET LA GRILLE DE MARCHE DISENT LA MÊME CHOSE, case par
  // case : une case peinte « marchable » où l'on ne marche pas est le mensonge
  // que ce banc garde.
  for (std::int32_t cell_y = 0; cell_y < kCellsPerSide; ++cell_y) {
    for (std::int32_t cell_x = 0; cell_x < kCellsPerSide; ++cell_x) {
      const std::optional<std::array<std::uint8_t, 4>> color =
          ColorAt(stage, quads, cell_x, cell_y);
      REQUIRE(color.has_value());
      CHECK((*color == walk) == stage.walk_map.Passable(cell_x, cell_y));
    }
  }

  // Un document vidé rend le quadrillage du fichier.
  document.ClearBlockedCells();
  REQUIRE(BlockStageCells(stage, document.blocked_cells()));
  ComposeStageGreyWorld(stage, kCenter, kCenter, look, quads);
  CHECK(ColorAt(stage, quads, kAddedWallX, kAddedWallY) == walk);
  CHECK(ColorAt(stage, quads, kAddedFenceX, kAddedFenceY) == walk);
  CHECK(ColorAt(stage, quads, kFileWallX, kFileWallY) == wall);
}

TEST_CASE("Cases bloquées, carte : sans grille, ni cases à bloquer ni quadrillage",
          "[app][mapedit][greyworld]") {
  MapStage stage{"sans grille", GroundedMap(), ro::sim::WalkMap()};
  const MapEdit document = BlockingDocument();
  CHECK_FALSE(BlockStageCells(stage, document.blocked_cells()));
  CHECK(PlannedMapOf(stage) == nullptr);
  // Une case laissée par l'image d'avant ne survit pas.
  std::vector<GroundQuad> quads(1);
  ComposeStageGreyWorld(stage, kCenter, kCenter, EnabledLook(), quads);
  CHECK(quads.empty());
}
