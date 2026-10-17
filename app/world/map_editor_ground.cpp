// L'OUTIL RELIEF DU MODE ÉDITEUR : la brosse qui lève, abaisse, aplanit ou
// adoucit le sol. Voir docs/adr/0118, phase 3.
//
// Un appui sur le sol commence un glissé ; chaque image en est un pas, qui
// ajoute ses levées au document ; le relâcher en fait UNE entrée de
// l'historique.

#include <algorithm>
#include <cmath>
#include <utility>

#include "ro/core/i18n.h"

#include "world/map_editor_reasons.h"
#include "world/map_editor_session.h"

namespace moonclient {
namespace {

// Le plus long pas de brosse, en secondes : une image qui a traîné ne lève pas
// d'un coup tout ce qu'elle a manqué.
constexpr double kLongestStrokeSeconds = 0.1;
constexpr double kMillisecondsPerSecond = 1000.0;

}  // namespace

ro::mapedit::GroundBrush MapEditorSession::Brush() const {
  // Le rayon se règle en cases et se compte en cubes.
  ro::mapedit::GroundBrush brush{.radius_cubes = brush_.radius_cells / ro::ui::kCellsPerCube,
                                 .units_per_second = brush_.units_per_second};
  switch (brush_.mode) {
    case ro::ui::MapEditorBrushMode::kRaise:
      brush.mode = ro::mapedit::GroundBrushMode::kRaise;
      break;
    case ro::ui::MapEditorBrushMode::kFlatten:
      brush.mode = ro::mapedit::GroundBrushMode::kFlatten;
      break;
    case ro::ui::MapEditorBrushMode::kSmooth:
      brush.mode = ro::mapedit::GroundBrushMode::kSmooth;
      break;
    case ro::ui::MapEditorBrushMode::kPoints:
      // Le mode Sommets ne brosse pas : son rayon borne seulement les points
      // qu'il montre.
      break;
  }
  return brush;
}

void MapEditorSession::SetGroundAllowed(bool allowed) {
  ground_allowed_ = allowed;
  if (allowed || tool_ != ro::ui::MapEditorTool::kRelief) {
    return;
  }
  // UN GLISSÉ EN COURS s'abandonne : son relief ne se voit plus, et le document
  // garde ce qu'il a déjà reçu.
  if (ground_stroke_.has_value() || (drag_.has_value() && drag_->ground_points)) {
    ground_stroke_.reset();
    drag_.reset();
    gesture_before_.reset();
  }
  tool_ = ro::ui::MapEditorTool::kMove;
}

bool MapEditorSession::GrabGround() {
  if (!ground_allowed_ || !ground_pointer_.has_value()) {
    return false;
  }
  ground_stroke_ = GroundStroke{};
  return true;
}

void MapEditorSession::StrokeGround(double now_ms, bool lower, const ro::render::MapScene& scene,
                                    MapEditStore& store) {
  if (!active_ || !ground_stroke_.has_value() || !ground_pointer_.has_value()) {
    return;
  }
  const auto [center_x, center_y] = *ground_pointer_;
  GroundStroke& stroke = *ground_stroke_;
  // APLANIR VISE LA HAUTEUR SOUS LE CENTRE AU PREMIER PAS : celle du point de
  // la grille le plus proche.
  if (!stroke.flatten_height.has_value()) {
    stroke.flatten_height =
        ro::mapedit::CornerPointHeight(scene.gnd, static_cast<std::int32_t>(std::lround(center_x)),
                                       static_cast<std::int32_t>(std::lround(center_y)))
            .value_or(0.0F);
  }
  // LE PREMIER PAS ne fait que poser l'horloge : il n'a pas encore duré.
  const double since_ms = stroke.last_ms.has_value() ? now_ms - *stroke.last_ms : 0.0;
  stroke.last_ms = now_ms;
  const double seconds = std::clamp(since_ms / kMillisecondsPerSecond, 0.0, kLongestStrokeSeconds);
  if (!(seconds > 0.0)) {
    return;
  }
  const std::vector<ro::mapedit::GroundEdit> lifts =
      ro::mapedit::BrushStroke(scene.gnd, Brush(), center_x, center_y, static_cast<float>(seconds),
                               lower, *stroke.flatten_height);
  if (lifts.empty()) {
    return;
  }
  const ro::mapedit::MapEdit& before = store.Document(map_name_);
  if (!gesture_before_.has_value()) {
    gesture_before_ = before;
  }
  ro::mapedit::MapEdit after = before;
  for (const ro::mapedit::GroundEdit& lift : lifts) {
    after.LiftGround(lift);
  }
  store.Set(map_name_, WithReasons(std::move(after)));
}

void MapEditorSession::EndGroundStroke(MapEditStore& store) {
  if (!gesture_before_.has_value()) {
    return;
  }
  Record(ro::core::Tr("Retoucher le relief"), *gesture_before_, store.Document(map_name_));
  gesture_before_.reset();
}

void MapEditorSession::RevertGround(MapEditStore& store) {
  const ro::mapedit::MapEdit before = store.Document(map_name_);
  if (before.ground().empty()) {
    return;
  }
  ro::mapedit::MapEdit after = before;
  after.ClearGround();
  store.Set(map_name_, WithReasons(after));
  Record(ro::core::Tr("Rétablir le relief"), before, store.Document(map_name_));
}

void MapEditorSession::SetGroundWhy(std::string why, MapEditStore& store) {
  const ro::mapedit::MapEdit before = store.Document(map_name_);
  // Sans retouche du relief, il n'y a rien à motiver.
  if (before.ground().empty() || before.ground_why() == why) {
    return;
  }
  ro::mapedit::MapEdit after = before;
  after.SetGroundWhy(std::move(why));
  store.Set(map_name_, WithReasons(after));
  Record(ro::core::Tr("Raison du relief"), before, store.Document(map_name_));
}

void MapEditorSession::SetBrush(const ro::ui::MapEditorBrush& brush) {
  using Limits = ro::ui::MapEditorBrush;
  brush_ = brush;
  brush_.radius_cells =
      std::clamp(brush.radius_cells, Limits::kLeastRadiusCells, Limits::kGreatestRadiusCells);
  brush_.units_per_second = std::clamp(brush.units_per_second, Limits::kLeastUnitsPerSecond,
                                       Limits::kGreatestUnitsPerSecond);
}

void MapEditorSession::ViewGround(const ro::mapedit::MapEdit& document,
                                  ro::ui::MapEditorView& view) const {
  view.ground.allowed = ground_allowed_;
  view.ground.brush = brush_;
  view.ground.edited_points = document.ground().size();
  view.ground.why = document.ground_why();
  view.ground.remesh_ms = ground_remesh_ms_;
  view.ground.remesh_live = ground_remesh_live_;
}

}  // namespace moonclient
