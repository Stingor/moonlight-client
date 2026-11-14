// LES CASES BLOQUÉES : rangées, écrites, relues, refusées, et posées sur la
// grille de marche. Voir docs/adr/0131.

#include "ro/mapedit/blocked_cells.h"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/gat.h"
#include "ro/mapedit/map_edit.h"

#include "support/map_builder.h"

using ro::mapedit::BlockedCell;
using ro::mapedit::CellBlock;
using ro::mapedit::MapEdit;

namespace {

constexpr std::int32_t kSide = 4;
constexpr std::int32_t kWallType = 1;
constexpr std::int32_t kFenceType = 5;
constexpr std::uint32_t kWaterFlag = ro::formats::kGatWaterFlag;

// Une grille de 4 × 4 cases marchables, sauf la case (0, 0), un mur, et la case
// (1, 0), une crevasse.
ro::formats::GatMap Walk() {
  std::vector<ro::test_support::GatCellSpec> cells(static_cast<std::size_t>(kSide * kSide));
  cells[0].type = kWallType;
  cells[1].type = kFenceType;
  cells[2].type = static_cast<std::int32_t>(kWaterFlag);
  auto gat = ro::formats::GatMap::Parse(ro::test_support::BuildGat(kSide, kSide, cells));
  REQUIRE(gat.has_value());
  return std::move(*gat);
}

}  // namespace

TEST_CASE("Cases bloquées : une seule par case, rangées par rangée puis par colonne", "[mapedit]") {
  MapEdit edit;
  edit.BlockCell({.cell_x = 3, .cell_y = 1});
  edit.BlockCell({.cell_x = 0, .cell_y = 2});
  edit.BlockCell({.cell_x = 1, .cell_y = 1, .block = CellBlock::kFence});
  edit.BlockCell({.cell_x = 3, .cell_y = 1, .block = CellBlock::kFence});
  REQUIRE(edit.blocked_cells().size() == 3);
  CHECK(edit.blocked_cells()[0] == BlockedCell{1, 1, CellBlock::kFence});
  CHECK(edit.blocked_cells()[1] == BlockedCell{3, 1, CellBlock::kFence});
  CHECK(edit.blocked_cells()[2] == BlockedCell{0, 2, CellBlock::kWall});
  CHECK(edit.BlockedCellAt(0, 2) != nullptr);
  CHECK(edit.BlockedCellAt(2, 0) == nullptr);
  CHECK_FALSE(edit.empty());
  edit.FreeCell(3, 1);
  edit.FreeCell(2, 2);
  CHECK(edit.blocked_cells().size() == 2);
  edit.ClearBlockedCells();
  CHECK(edit.empty());
}

TEST_CASE("Cases bloquées : le document les écrit et les relit à l'identique", "[mapedit]") {
  MapEdit edit;
  edit.BlockCell({.cell_x = 150, .cell_y = 180});
  edit.BlockCell({.cell_x = 152, .cell_y = 181, .block = CellBlock::kFence});
  edit.SetBlockedCellsWhy("les colonnes de la place");
  const std::string text = ro::mapedit::MapEditToYaml(edit, "prontera");
  CHECK(text.find("    - [150, 180, wall]\n    - [152, 181, fence]\n") != std::string::npos);
  auto reread = ro::mapedit::ParseMapEdit(text, "prontera");
  REQUIRE(reread.has_value());
  CHECK(*reread == edit);
}

TEST_CASE("Cases bloquées : une section mal formée refuse le document", "[mapedit]") {
  const auto parse = [](const std::string& cells, std::string_view map = "m",
                        const std::string& why = "x") {
    return ro::mapedit::ParseMapEdit("version: 1\nmap: '" + std::string(map) +
                                         "'\nmodels: []\nblocked_cells:\n  why: \"" + why +
                                         "\"\n  cells:\n" + cells,
                                     map);
  };
  CHECK(parse("    - [1, 2, wall]\n").has_value());
  CHECK_FALSE(parse("    - [1, 2, wall]\n", "m", "").has_value());
  CHECK_FALSE(parse("    - [1, 2]\n").has_value());
  CHECK_FALSE(parse("    - [-1, 2, wall]\n").has_value());
  CHECK_FALSE(parse("    - [1, 2, rock]\n").has_value());
  CHECK_FALSE(parse("    - [1, 2, wall]\n    - [1, 2, fence]\n").has_value());
  CHECK_FALSE(parse("    - [1, 2, wall]\n", ro::mapedit::kModelLightsDocument).has_value());
}

TEST_CASE("Cases bloquées : l'emprise prend les cases dont le centre tombe dedans", "[mapedit]") {
  using Cell = std::array<std::int32_t, 2>;
  // Un losange autour de la case (5, 5), en double comme les coins haut et bas
  // d'une boîte : la case et ses quatre voisines.
  const std::vector<std::array<float, 2>> diamond = {
      {3.5F, 5.0F}, {5.0F, 3.5F}, {6.5F, 5.0F}, {5.0F, 6.5F},
      {3.5F, 5.0F}, {5.0F, 3.5F}, {6.5F, 5.0F}, {5.0F, 6.5F},
  };
  CHECK(ro::mapedit::CellsUnderFootprint(diamond) ==
        std::vector<Cell>{{5, 4}, {4, 5}, {5, 5}, {6, 5}, {5, 6}});
  // Un rectangle droit, bords sur des centres : ils comptent.
  const std::vector<std::array<float, 2>> box = {
      {1.0F, 1.0F}, {3.0F, 1.0F}, {3.0F, 2.0F}, {1.0F, 2.0F}};
  CHECK(ro::mapedit::CellsUnderFootprint(box).size() == 6);
  // Des points alignés n'ont pas d'aire.
  const std::vector<std::array<float, 2>> line = {{1.0F, 1.0F}, {2.0F, 2.0F}, {3.0F, 3.0F}};
  CHECK(ro::mapedit::CellsUnderFootprint(line).empty());
}

TEST_CASE("Cases bloquées : posées sur la grille, elles ne font que bloquer", "[mapedit]") {
  ro::formats::GatMap gat = Walk();
  const std::vector<BlockedCell> cells = {
      {.cell_x = 0, .cell_y = 0, .block = CellBlock::kFence},  // un mur reste un mur
      {.cell_x = 1, .cell_y = 0, .block = CellBlock::kWall},   // une crevasse devient un mur
      {.cell_x = 2, .cell_y = 0, .block = CellBlock::kFence},  // l'eau marchable se bloque
      {.cell_x = 3, .cell_y = 3, .block = CellBlock::kWall},
      {.cell_x = 4, .cell_y = 0, .block = CellBlock::kWall},  // hors de la grille
  };
  const ro::mapedit::MapEditReport report = ro::mapedit::BlockGatCells(cells, gat);
  CHECK(report.applied == 4);
  CHECK(report.refused.size() == 1);
  CHECK(gat.at(0, 0).type == kWallType);
  CHECK(gat.at(1, 0).type == kWallType);
  CHECK(gat.at(2, 0).type == kFenceType);
  CHECK(gat.at(3, 3).type == kWallType);
  CHECK(gat.at(3, 0).type == 0);
}
