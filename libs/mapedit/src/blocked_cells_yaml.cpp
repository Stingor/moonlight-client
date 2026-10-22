#include "blocked_cells_yaml.h"

#include <cstdint>
#include <format>
#include <optional>

#include "ground_yaml.h"

namespace ro::mapedit::detail {
namespace {

// Une case : sa colonne, sa rangée, sa sorte.
constexpr std::size_t kCellFields = 3;

constexpr std::string_view kWallName = "wall";
constexpr std::string_view kFenceName = "fence";

[[nodiscard]] std::optional<CellBlock> BlockNamed(std::string_view name) {
  if (name == kWallName) {
    return CellBlock::kWall;
  }
  if (name == kFenceName) {
    return CellBlock::kFence;
  }
  return std::nullopt;
}

[[nodiscard]] std::string_view NameOf(CellBlock block) {
  return block == CellBlock::kWall ? kWallName : kFenceName;
}

}  // namespace

std::string ReadBlockedCells(const YAML::Node& whole, std::string_view map_name, MapEdit& edit) {
  const YAML::Node section = whole["blocked_cells"];
  if (!section) {
    return {};
  }
  if (map_name == kModelLightsDocument) {
    return "« blocked_cells » n'appartient qu'au document d'une carte";
  }
  if (!section.IsMap()) {
    return "« blocked_cells » n'est pas une table";
  }
  const YAML::Node why = section["why"];
  const YAML::Node cells = section["cells"];
  if (!why || !cells) {
    return "« blocked_cells » : « why » et « cells » sont dus";
  }
  if (!cells.IsSequence()) {
    return "« blocked_cells » : « cells » n'est pas une liste";
  }
  for (std::size_t rank = 0; rank < cells.size(); ++rank) {
    const YAML::Node cell = cells[rank];
    if (!cell.IsSequence() || cell.size() != kCellFields) {
      return std::format("« blocked_cells », case {} : [colonne, rangée, sorte] attendu", rank);
    }
    const auto cell_x = cell[0].as<std::int32_t>();
    const auto cell_y = cell[1].as<std::int32_t>();
    if (cell_x < 0 || cell_y < 0) {
      return std::format("« blocked_cells », case {} : ({}, {}) est négative", rank, cell_x,
                         cell_y);
    }
    const std::string name = cell[2].as<std::string>();
    const std::optional<CellBlock> block = BlockNamed(name);
    if (!block.has_value()) {
      return std::format("« blocked_cells », case {} : « {} » n'est ni « {} » ni « {} »", rank,
                         name, kWallName, kFenceName);
    }
    if (edit.BlockedCellAt(cell_x, cell_y) != nullptr) {
      return std::format("« blocked_cells », case {} : ({}, {}) revient", rank, cell_x, cell_y);
    }
    edit.BlockCell(BlockedCell{.cell_x = cell_x, .cell_y = cell_y, .block = *block});
  }
  edit.SetBlockedCellsWhy(why.as<std::string>());
  if (!edit.blocked_cells().empty() && edit.blocked_cells_why().empty()) {
    return "« blocked_cells » : « why » est vide";
  }
  return {};
}

std::string BlockedCellsYaml(const MapEdit& edit) {
  if (edit.blocked_cells().empty()) {
    return {};
  }
  std::string text =
      std::format("blocked_cells:\n  why: {}\n  cells:\n", DoubleQuoted(edit.blocked_cells_why()));
  for (const BlockedCell& cell : edit.blocked_cells()) {
    text += std::format("    - [{}, {}, {}]\n", cell.cell_x, cell.cell_y, NameOf(cell.block));
  }
  return text;
}

}  // namespace ro::mapedit::detail
