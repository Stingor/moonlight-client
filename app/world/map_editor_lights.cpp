// LES LUMIÈRES PONCTUELLES DE L'ÉDITEUR DE CARTE : voir docs/adr/0119.
//
// Une lumière s'ajoute où regarde la caméra, un peu au-dessus du sol, et se
// règle depuis le panneau ; chaque geste compte dans l'historique, un glissé
// au relâcher seulement.

#include <array>
#include <cstdio>
#include <optional>
#include <utility>
#include <vector>

#include "ro/core/i18n.h"

#include "world/map_editor_session.h"

namespace moonclient {
namespace {

// La place d'un nom de geste, zéro final compris.
constexpr std::size_t kLabelCapacity = 192;
// UNE LUMIÈRE NEUVE : une lanterne. Sa hauteur au-dessus du point visé — un y
// négatif monte, dans le RSW —, sa portée en unités du RSW (cinq carreaux du
// sol) et sa couleur chaude ; elle ne s'allume qu'à la nuit.
constexpr float kNewLightHeight = 15.0F;
constexpr float kNewLightRange = 50.0F;
constexpr std::array<float, 3> kNewLightColor{1.0F, 0.85F, 0.6F};

ro::ui::MapEditorLight ShownLight(const ro::mapedit::AddedLight& light) {
  return ro::ui::MapEditorLight{.position = light.position,
                                .color = light.color,
                                .range = light.range,
                                .night_only = light.night_only,
                                .why = light.why};
}

ro::mapedit::AddedLight DocumentLight(const ro::ui::MapEditorLight& light) {
  return ro::mapedit::AddedLight{.position = light.position,
                                 .color = light.color,
                                 .range = light.range,
                                 .night_only = light.night_only,
                                 .why = light.why};
}

}  // namespace

std::vector<ro::ui::MapEditorLine> MapEditorSession::LightTrace() const {
  std::vector<ro::ui::MapEditorLine> marks;
  marks.reserve(light_dots_.size());
  for (const LightDot& dot : light_dots_) {
    marks.push_back(ro::ui::MapEditorLine{.from = dot.at,
                                          .to = dot.at,
                                          .kind = selected_light_ == dot.slot
                                                      ? ro::ui::MapEditorLineKind::kLightSelected
                                                      : ro::ui::MapEditorLineKind::kLight});
  }
  return marks;
}

bool MapEditorSession::GrabLight(glm::vec2 pointer) {
  float nearest = kGizmoGrabPixels;
  std::optional<std::size_t> picked;
  for (const LightDot& dot : light_dots_) {
    const float distance = glm::length(dot.at - pointer);
    if (distance <= nearest) {
      nearest = distance;
      picked = dot.slot;
    }
  }
  if (!picked.has_value()) {
    return false;
  }
  selected_light_ = picked;
  return true;
}

void MapEditorSession::ViewLights(const ro::mapedit::MapEdit& document,
                                  ro::ui::MapEditorView& view) const {
  view.lights.reserve(document.lights().size());
  for (const ro::mapedit::AddedLight& light : document.lights()) {
    view.lights.push_back(ShownLight(light));
  }
  if (selected_light_.has_value() && *selected_light_ < view.lights.size()) {
    view.selected_light = selected_light_;
  }
}

MapEditorSession::Outcome MapEditorSession::AddLight(MapEditStore& store) {
  const ro::mapedit::MapEdit before = store.Document(map_name_);
  ro::mapedit::MapEdit after = before;
  after.AddLight(ro::mapedit::AddedLight{
      .position = {drop_point_[0], drop_point_[1] - kNewLightHeight, drop_point_[2]},
      .color = kNewLightColor,
      .range = kNewLightRange,
      .night_only = true,
      .why = {}});
  store.Set(map_name_, after);
  selected_light_ = after.lights().size() - 1;
  Record(ro::core::Tr("Ajouter une lumière"), before, after);
  return {};
}

MapEditorSession::Outcome MapEditorSession::SetLight(const ro::ui::map_editor::SetLight& set,
                                                     MapEditStore& store) {
  const ro::mapedit::MapEdit before = store.Document(map_name_);
  if (set.slot >= before.lights().size()) {
    return {};
  }
  if (!gesture_before_.has_value()) {
    gesture_before_ = before;
  }
  ro::mapedit::MapEdit after = before;
  after.SetLight(set.slot, DocumentLight(set.light));
  if (!(after == before)) {
    store.Set(map_name_, after);
  }
  if (set.released) {
    std::array<char, kLabelCapacity> label{};
    std::snprintf(label.data(), label.size(), ro::core::Tr("Régler la lumière %zu"), set.slot);
    Record(label.data(), *gesture_before_, store.Document(map_name_));
    gesture_before_.reset();
  }
  return {};
}

MapEditorSession::Outcome MapEditorSession::RemoveLight(std::size_t slot, MapEditStore& store) {
  const ro::mapedit::MapEdit before = store.Document(map_name_);
  if (slot >= before.lights().size()) {
    return {};
  }
  ro::mapedit::MapEdit after = before;
  after.RemoveLight(slot);
  store.Set(map_name_, after);
  selected_light_.reset();
  std::array<char, kLabelCapacity> label{};
  std::snprintf(label.data(), label.size(), ro::core::Tr("Retirer la lumière %zu"), slot);
  Record(label.data(), before, after);
  return {};
}

}  // namespace moonclient
