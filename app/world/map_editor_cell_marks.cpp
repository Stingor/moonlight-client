// L'OUTIL CASES DU MODE ÉDITEUR, à l'écran : la case sous la souris, et les
// cases bloquées tracées au sol — grises celles de la carte, rouges et orange
// celles du document —, avec celles que le pinceau couvre en cyan.

#include "world/map_editor_cell_marks.h"

#include <cmath>

#include "ro/render/actor_pick.h"
#include "ro/render/ground_pick.h"
#include "ro/render/terrain_mesh.h"

namespace moonclient {
namespace {

// Jusqu'où, en cases autour du pinceau, les cases bloquées se tracent : assez
// pour voir le mur qu'on peint, sans tracer toute la carte.
constexpr std::int32_t kMarkReachCells = 24;
// Le carré d'une case, en cases depuis son centre : un peu moins d'une demie,
// pour que deux cases voisines restent deux carrés.
constexpr float kMarkHalfCells = 0.42F;
// Un peu au-dessus du sol, en unités du monde : posé dessus, le carré s'y
// enfoncerait par endroits.
constexpr float kMarkLift = 0.3F;

// Le carré de la case (x, y), au sol, en quatre segments ; rien quand un coin
// tombe derrière l'œil.
void MarkCell(const MapStage& stage, std::int32_t cell_x, std::int32_t cell_y,
              ro::ui::MapEditorLineKind kind, const ro::render::Camera& camera, float aspect_ratio,
              float width, float height, std::vector<ro::ui::MapEditorLine>& lines) {
  constexpr std::array<std::array<float, 2>, 4> kCorners = {{
      {-kMarkHalfCells, -kMarkHalfCells},
      {kMarkHalfCells, -kMarkHalfCells},
      {kMarkHalfCells, kMarkHalfCells},
      {-kMarkHalfCells, kMarkHalfCells},
  }};
  std::array<glm::vec2, 4> screen{};
  for (std::size_t corner = 0; corner < kCorners.size(); ++corner) {
    const float x = static_cast<float>(cell_x) + kCorners[corner][0];
    const float y = static_cast<float>(cell_y) + kCorners[corner][1];
    const std::array<float, 3> world = ro::render::CellPositionAt(stage.scene.gnd, x, y);
    const float ground = ro::render::WalkableHeightAt(*stage.gat, x, y);
    const std::optional<glm::vec2> at = ro::render::ProjectToScreen(
        camera, aspect_ratio, glm::vec3(world[0], ground + kMarkLift, world[2]), width, height);
    if (!at.has_value()) {
      return;
    }
    screen[corner] = *at;
  }
  for (std::size_t corner = 0; corner < screen.size(); ++corner) {
    lines.push_back(ro::ui::MapEditorLine{
        .from = screen[corner], .to = screen[(corner + 1) % screen.size()], .kind = kind});
  }
}

}  // namespace

std::optional<std::array<std::int32_t, 2>> PointedCell(const MapStage& stage,
                                                       const ro::render::Camera& camera,
                                                       float aspect_ratio, float screen_x,
                                                       float screen_y, float viewport_width,
                                                       float viewport_height) {
  if (!stage.gat.has_value()) {
    return std::nullopt;
  }
  const ro::formats::GatMap& gat = *stage.gat;
  const std::optional<std::array<float, 3>> hit =
      ro::render::PickGround(stage.scene.gnd, &gat, camera, aspect_ratio, screen_x, screen_y,
                             viewport_width, viewport_height);
  if (!hit.has_value()) {
    return std::nullopt;
  }
  const std::array<float, 2> fractional =
      ro::render::FractionalCellAt(stage.scene.gnd, (*hit)[0], (*hit)[2]);
  const auto cell_x = static_cast<std::int32_t>(std::lround(fractional[0]));
  const auto cell_y = static_cast<std::int32_t>(std::lround(fractional[1]));
  if (!gat.InBounds(cell_x, cell_y)) {
    return std::nullopt;
  }
  return std::array<std::int32_t, 2>{cell_x, cell_y};
}

std::vector<ro::ui::MapEditorLine> CellMarks(const MapStage& stage,
                                             const ro::mapedit::MapEdit& document,
                                             const MapEditorSession& session,
                                             const ro::render::Camera& camera, float aspect_ratio,
                                             float viewport_width, float viewport_height) {
  std::vector<ro::ui::MapEditorLine> lines;
  if (!session.active() || session.tool() != ro::ui::MapEditorTool::kCells ||
      !stage.gat.has_value()) {
    return lines;
  }
  // AUTOUR DU PINCEAU, ou de ce que regarde la caméra quand la souris quitte
  // le sol.
  const std::array<float, 2> look =
      ro::render::FractionalCellAt(stage.scene.gnd, camera.look_at.x, camera.look_at.z);
  const std::array<std::int32_t, 2> center = session.cell_pointer().value_or(
      std::array<std::int32_t, 2>{static_cast<std::int32_t>(std::lround(look[0])),
                                  static_cast<std::int32_t>(std::lround(look[1]))});
  const ro::formats::GatMap& gat = *stage.gat;
  for (std::int32_t cell_y = center[1] - kMarkReachCells; cell_y <= center[1] + kMarkReachCells;
       ++cell_y) {
    for (std::int32_t cell_x = center[0] - kMarkReachCells; cell_x <= center[0] + kMarkReachCells;
         ++cell_x) {
      if (!gat.InBounds(cell_x, cell_y)) {
        continue;
      }
      ro::ui::MapEditorLineKind kind = ro::ui::MapEditorLineKind::kCellMap;
      if (const ro::mapedit::BlockedCell* blocked = document.BlockedCellAt(cell_x, cell_y)) {
        kind = blocked->block == ro::mapedit::CellBlock::kWall
                   ? ro::ui::MapEditorLineKind::kCellWall
                   : ro::ui::MapEditorLineKind::kCellFence;
      } else if (stage.walk_map.Passable(cell_x, cell_y)) {
        continue;
      }
      MarkCell(stage, cell_x, cell_y, kind, camera, aspect_ratio, viewport_width, viewport_height,
               lines);
    }
  }
  if (const std::optional<std::array<std::int32_t, 2>>& pointer = session.cell_pointer();
      pointer.has_value()) {
    for (const auto& [cell_x, cell_y] : session.CellsUnderBrush(*pointer)) {
      if (gat.InBounds(cell_x, cell_y)) {
        MarkCell(stage, cell_x, cell_y, ro::ui::MapEditorLineKind::kCellBrush, camera, aspect_ratio,
                 viewport_width, viewport_height, lines);
      }
    }
  }
  return lines;
}

}  // namespace moonclient
