// LE MODE SOMMETS DE L'OUTIL RELIEF : les points de la grille des coins se
// désignent un à un et se lèvent à la poignée. Voir docs/adr/0118, phase 3.
//
// Un clic sur un point le désigne ; Maj+clic l'ajoute à la sélection, ou l'en
// retire ; un clic dans le vide la vide. La poignée verticale du désigné lève
// toute la sélection du même écart, et un glissé fait UNE entrée de
// l'historique. Tout passe par les levées du document : `LiftGround`.

#include <algorithm>
#include <cmath>
#include <optional>
#include <span>
#include <utility>

#include "ro/core/i18n.h"

#include "world/map_editor_reasons.h"
#include "world/map_editor_session.h"

namespace moonclient {
namespace {

using CornerPoint = MapEditorSession::CornerPoint;

// L'ordre du document : rangée, puis colonne.
bool Before(const CornerPoint& first, const CornerPoint& second) {
  return first[1] != second[1] ? first[1] < second[1] : first[0] < second[0];
}

// La levée que `edits` donnent au point, ou zéro.
float LiftOf(std::span<const ro::mapedit::GroundEdit> edits, const CornerPoint& point) {
  const auto found = std::ranges::find_if(edits, [&point](const ro::mapedit::GroundEdit& edit) {
    return edit.corner_x == point[0] && edit.corner_y == point[1];
  });
  return found != edits.end() ? found->lift : 0.0F;
}

// LA HAUTEUR DU POINT telle que `document` la donne, dans le sens du monde :
// celle du sol de la scène, moins la levée que ce sol porte déjà, plus celle du
// document. Rien hors de la grille.
std::optional<float> PointHeight(const ro::render::MapScene& scene,
                                 const ro::mapedit::MapEdit& document, const CornerPoint& point) {
  const std::optional<float> shown = ro::mapedit::CornerPointHeight(scene.gnd, point[0], point[1]);
  if (!shown.has_value()) {
    return std::nullopt;
  }
  return *shown - LiftOf(scene.ground_edits, point) + LiftOf(document.ground(), point);
}

}  // namespace

std::vector<ro::ui::MapEditorLine> MapEditorSession::GroundPointTrace() const {
  std::vector<ro::ui::MapEditorLine> dots;
  dots.reserve(ground_dots_.size());
  for (const GroundDot& dot : ground_dots_) {
    ro::ui::MapEditorLineKind kind = ro::ui::MapEditorLineKind::kGroundPoint;
    if (designated_point_ == dot.point) {
      kind = ro::ui::MapEditorLineKind::kGroundPointDesignated;
    } else if (std::ranges::binary_search(selected_points_, dot.point, Before)) {
      kind = ro::ui::MapEditorLineKind::kGroundPointSelected;
    } else if (dot.edited) {
      kind = ro::ui::MapEditorLineKind::kGroundPointEdited;
    }
    dots.push_back(ro::ui::MapEditorLine{.from = dot.at, .to = dot.at, .kind = kind});
  }
  return dots;
}

bool MapEditorSession::GrabGroundPoint(glm::vec2 pointer, bool extend) {
  // LA POIGNÉE D'ABORD, quand un point est désigné : elle lève la sélection.
  if (designated_point_.has_value() && !extend) {
    if (const std::optional<std::size_t> grabbed = GrabbedHandle(handles_, pointer)) {
      const auto designated = std::ranges::find_if(
          ground_dots_, [this](const GroundDot& dot) { return dot.point == designated_point_; });
      const ro::ui::map_editor::Place start{.released = false};
      drag_ = Drag{.handle = *grabbed,
                   .grabbed = handles_[*grabbed],
                   .frame = gizmo_frame_,
                   .from = pointer,
                   .start = start,
                   .last = start,
                   .ground_points = true,
                   .ground_height = designated != ground_dots_.end() ? designated->height : 0.0F};
      return true;
    }
  }
  // SINON LE POINT LE PLUS PROCHE, à portée de saisie.
  float nearest = kGizmoGrabPixels;
  std::optional<CornerPoint> picked;
  for (const GroundDot& dot : ground_dots_) {
    const float distance = glm::length(dot.at - pointer);
    if (distance <= nearest) {
      nearest = distance;
      picked = dot.point;
    }
  }
  // LE VIDE vide la sélection ; Maj tenue, il la garde. L'appui est pris dans
  // les deux cas : il ne désigne pas de modèle en passant.
  if (!picked.has_value()) {
    if (!extend) {
      static_cast<void>(DeselectGroundPoints());
    }
    return true;
  }
  if (!extend) {
    selected_points_ = {*picked};
    designated_point_ = picked;
    return true;
  }
  const auto place = std::ranges::lower_bound(selected_points_, *picked, Before);
  if (place != selected_points_.end() && *place == *picked) {
    selected_points_.erase(place);
    if (designated_point_ == picked) {
      designated_point_ =
          selected_points_.empty() ? std::nullopt : std::optional(selected_points_.back());
    }
    return true;
  }
  selected_points_.insert(place, *picked);
  designated_point_ = picked;
  return true;
}

void MapEditorSession::DragGroundPoints(bool snap) {
  float lift = drag_->last.position[1];
  // Maj tenue, le désigné s'aimante à l'unité entière ; les autres le suivent
  // du même écart.
  if (snap) {
    lift = std::round(drag_->ground_height + lift) - drag_->ground_height;
  }
  drag_->last.position[1] = lift;
  queued_.emplace_back(ro::ui::map_editor::LiftGroundPoints{.lift = lift, .released = false});
}

MapEditorSession::Outcome MapEditorSession::LiftGroundPoints(
    const ro::ui::map_editor::LiftGroundPoints& lift, MapEditStore& store) {
  if (!selected_points_.empty()) {
    const ro::mapedit::MapEdit& current = store.Document(map_name_);
    if (!gesture_before_.has_value()) {
      gesture_before_ = current;
    }
    // LA LEVÉE COMPTE DEPUIS LE DÉBUT DU GESTE : chaque image repart du
    // document d'avant l'appui.
    ro::mapedit::MapEdit after = *gesture_before_;
    for (const CornerPoint& point : selected_points_) {
      after.LiftGround({.corner_x = point[0], .corner_y = point[1], .lift = lift.lift});
    }
    after = WithReasons(std::move(after));
    if (!(after == current)) {
      store.Set(map_name_, std::move(after));
    }
  }
  if (lift.released && gesture_before_.has_value()) {
    Record(ro::core::Tr("Retoucher le relief"), *gesture_before_, store.Document(map_name_));
    gesture_before_.reset();
  }
  return {};
}

MapEditorSession::Outcome MapEditorSession::SetGroundPointHeight(
    const ro::ui::map_editor::SetGroundPointHeight& set, const ro::render::MapScene& scene,
    MapEditStore& store) {
  if (!designated_point_.has_value()) {
    return {};
  }
  const ro::mapedit::MapEdit& base =
      gesture_before_.has_value() ? *gesture_before_ : store.Document(map_name_);
  const std::optional<float> height = PointHeight(scene, base, *designated_point_);
  if (!height.has_value()) {
    return {};
  }
  return LiftGroundPoints({.lift = set.height - *height, .released = set.released}, store);
}

MapEditorSession::Outcome MapEditorSession::RevertGroundPoints(MapEditStore& store) {
  const ro::mapedit::MapEdit before = store.Document(map_name_);
  ro::mapedit::MapEdit after = before;
  for (const CornerPoint& point : selected_points_) {
    const float lift = LiftOf(before.ground(), point);
    after.LiftGround({.corner_x = point[0], .corner_y = point[1], .lift = -lift});
  }
  if (after == before) {
    return {};
  }
  // Le dernier point rétabli emporte la raison du relief.
  if (after.ground().empty()) {
    after.ClearGround();
  }
  store.Set(map_name_, WithReasons(std::move(after)));
  Record(ro::core::Tr("Rétablir des points du relief"), before, store.Document(map_name_));
  return {};
}

MapEditorSession::Outcome MapEditorSession::NudgeGroundPoints(float lift, MapEditStore& store) {
  if (drag_.has_value() || lift == 0.0F) {
    return {};
  }
  return LiftGroundPoints({.lift = lift, .released = true}, store);
}

bool MapEditorSession::DeselectGroundPoints() noexcept {
  const bool held = designated_point_.has_value() || !selected_points_.empty();
  designated_point_.reset();
  selected_points_.clear();
  return held;
}

void MapEditorSession::ViewGroundPoints(const ro::render::MapScene& scene,
                                        const ro::mapedit::MapEdit& document,
                                        ro::ui::MapEditorView& view) const {
  view.ground.selected_points = selected_points_.size();
  view.ground.selected_edited = static_cast<std::size_t>(std::ranges::count_if(
      selected_points_,
      [&](const CornerPoint& point) { return LiftOf(document.ground(), point) != 0.0F; }));
  if (!designated_point_.has_value()) {
    return;
  }
  const CornerPoint& point = *designated_point_;
  view.ground.designated =
      ro::ui::MapEditorGroundPoint{.cells = ro::ui::CellsOfCornerPoint(point[0], point[1]),
                                   .height = PointHeight(scene, document, point).value_or(0.0F),
                                   .edited = LiftOf(document.ground(), point) != 0.0F};
}

}  // namespace moonclient
