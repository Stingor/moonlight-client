// L'OUTIL CASES DU MODE ÉDITEUR : le pinceau qui bloque ou libère les cases de
// la grille de marche, et l'emprise du modèle désigné. Voir docs/adr/0131.
//
// Un appui sur le sol commence un glissé ; chaque image en est un pas, qui
// bloque ou libère les cases sous le pinceau ; le relâcher en fait UNE entrée
// de l'historique. Le serveur bloque les mêmes cases, à l'enregistrement.

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <utility>
#include <vector>

#include "ro/core/i18n.h"
#include "ro/mapedit/blocked_cells.h"
#include "ro/render/model_pick.h"
#include "ro/render/terrain_mesh.h"

#include "world/map_editor_reasons.h"
#include "world/map_editor_session.h"

namespace moonclient {
namespace {

[[nodiscard]] ro::mapedit::CellBlock BlockOf(ro::ui::MapEditorCellBlock block) {
  return block == ro::ui::MapEditorCellBlock::kFence ? ro::mapedit::CellBlock::kFence
                                                     : ro::mapedit::CellBlock::kWall;
}

}  // namespace

std::vector<std::array<std::int32_t, 2>> MapEditorSession::CellsUnderBrush(
    const std::array<std::int32_t, 2>& cell) const {
  const float radius = cell_brush_.radius_cells;
  const auto reach = static_cast<std::int32_t>(std::floor(radius));
  std::vector<std::array<std::int32_t, 2>> cells;
  for (std::int32_t step_y = -reach; step_y <= reach; ++step_y) {
    for (std::int32_t step_x = -reach; step_x <= reach; ++step_x) {
      if (static_cast<float>(step_x * step_x + step_y * step_y) <= radius * radius) {
        cells.push_back({cell[0] + step_x, cell[1] + step_y});
      }
    }
  }
  return cells;
}

bool MapEditorSession::GrabCells() {
  if (!cell_pointer_.has_value()) {
    return false;
  }
  cell_stroke_ = true;
  return true;
}

bool MapEditorSession::ReleaseCells() {
  if (!cell_stroke_) {
    return false;
  }
  cell_stroke_ = false;
  queued_.emplace_back(ro::ui::map_editor::EndCellStroke{});
  return true;
}

void MapEditorSession::StrokeCells(bool free, MapEditStore& store) {
  if (!active_ || !cell_stroke_ || !cell_pointer_.has_value()) {
    return;
  }
  const ro::mapedit::MapEdit& before = store.Document(map_name_);
  ro::mapedit::MapEdit after = before;
  for (const auto& [cell_x, cell_y] : CellsUnderBrush(*cell_pointer_)) {
    // Une case négative ne s'écrit pas : le document la refuserait.
    if (cell_x < 0 || cell_y < 0) {
      continue;
    }
    if (free) {
      after.FreeCell(cell_x, cell_y);
    } else {
      after.BlockCell({.cell_x = cell_x, .cell_y = cell_y, .block = BlockOf(cell_brush_.block)});
    }
  }
  if (after == before) {
    return;
  }
  if (!gesture_before_.has_value()) {
    gesture_before_ = before;
  }
  store.Set(map_name_, WithReasons(std::move(after)));
}

MapEditorSession::Outcome MapEditorSession::ApplyCells(const ro::ui::map_editor::SetCellBrush& set,
                                                       const ro::render::MapScene& /*scene*/,
                                                       MapEditStore& /*store*/) {
  using Limits = ro::ui::MapEditorCellBrush;
  cell_brush_ = set.brush;
  cell_brush_.radius_cells =
      std::clamp(set.brush.radius_cells, Limits::kLeastRadiusCells, Limits::kGreatestRadiusCells);
  return {};
}

MapEditorSession::Outcome MapEditorSession::ApplyCells(
    const ro::ui::map_editor::EndCellStroke& /*end*/, const ro::render::MapScene& /*scene*/,
    MapEditStore& store) {
  if (gesture_before_.has_value()) {
    Record(ro::core::Tr("Peindre les cases"), *gesture_before_, store.Document(map_name_));
    gesture_before_.reset();
  }
  return {};
}

MapEditorSession::Outcome MapEditorSession::ApplyCells(
    const ro::ui::map_editor::ClearCells& /*clear*/, const ro::render::MapScene& /*scene*/,
    MapEditStore& store) {
  const ro::mapedit::MapEdit before = store.Document(map_name_);
  if (before.blocked_cells().empty()) {
    return {};
  }
  ro::mapedit::MapEdit after = before;
  after.ClearBlockedCells();
  store.Set(map_name_, WithReasons(std::move(after)));
  Record(ro::core::Tr("Libérer toutes les cases"), before, store.Document(map_name_));
  return {};
}

MapEditorSession::Outcome MapEditorSession::ApplyCells(const ro::ui::map_editor::SetCellsWhy& why,
                                                       const ro::render::MapScene& /*scene*/,
                                                       MapEditStore& store) {
  const ro::mapedit::MapEdit before = store.Document(map_name_);
  // Sans case bloquée, il n'y a rien à motiver.
  if (before.blocked_cells().empty() || before.blocked_cells_why() == why.why) {
    return {};
  }
  ro::mapedit::MapEdit after = before;
  after.SetBlockedCellsWhy(why.why);
  store.Set(map_name_, WithReasons(std::move(after)));
  Record(ro::core::Tr("Raison des cases"), before, store.Document(map_name_));
  return {};
}

MapEditorSession::Outcome MapEditorSession::ApplyCells(
    const ro::ui::map_editor::BlockModelFootprint& /*footprint*/, const ro::render::MapScene& scene,
    MapEditStore& store) {
  if (selection_.empty()) {
    return {};
  }
  const std::optional<ro::render::ScenePose> pose =
      ro::render::FindScenePose(scene, *selection_.model_index);
  if (!pose.has_value()) {
    return {};
  }
  // LA BOÎTE POSÉE AU SOL : ses huit coins, ramenés en cases de la grille de
  // marche, dont l'enveloppe est l'emprise.
  std::vector<std::array<float, 2>> corners;
  for (const glm::vec3& corner : ro::render::PoseCorners(*pose)) {
    corners.push_back(ro::render::FractionalCellAt(scene.gnd, corner.x, corner.z));
  }
  const ro::mapedit::MapEdit before = store.Document(map_name_);
  ro::mapedit::MapEdit after = before;
  for (const auto& [cell_x, cell_y] : ro::mapedit::CellsUnderFootprint(corners)) {
    if (cell_x >= 0 && cell_y >= 0) {
      after.BlockCell({.cell_x = cell_x, .cell_y = cell_y, .block = BlockOf(cell_brush_.block)});
    }
  }
  if (after == before) {
    return {};
  }
  store.Set(map_name_, WithReasons(std::move(after)));
  Record(ro::core::Tr("Bloquer l'emprise du modèle"), before, store.Document(map_name_));
  return {};
}

void MapEditorSession::ViewCells(const ro::mapedit::MapEdit& document,
                                 ro::ui::MapEditorView& view) const {
  view.cells.brush = cell_brush_;
  view.cells.blocked = document.blocked_cells().size();
  view.cells.why = document.blocked_cells_why();
  view.cells.model_selected = !selection_.empty();
  view.cells.pointer = cell_pointer_;
}

}  // namespace moonclient
