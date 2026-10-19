// LES LUMIÈRES PONCTUELLES DE L'ÉDITEUR DE CARTE : voir docs/adr/0119.
//
// Une lumière s'ajoute où regarde la caméra, un peu au-dessus du sol, et se
// règle depuis le panneau ; chaque geste compte dans l'historique, un glissé
// au relâcher seulement.

#include <algorithm>
#include <array>
#include <cstdio>
#include <optional>
#include <utility>
#include <vector>

#include "ro/core/i18n.h"
#include "ro/mapedit/model_light.h"

#include "world/map_editor_reasons.h"
#include "world/map_editor_session.h"

namespace moonclient {
namespace {

// La place d'un nom de geste, zéro final compris.
constexpr std::size_t kLabelCapacity = 192;
// UNE LUMIÈRE NEUVE : une lanterne. Sa hauteur au-dessus du point visé — un y
// négatif monte, dans le RSW —, sa portée en unités du RSW (cinq carreaux du
// sol) et sa couleur chaude, deux fois la pleine intensité : sur une texture
// sombre de nuit, une lumière de 1 se devine à peine. Elle brille aussitôt, de
// jour comme de nuit : on la voit en la posant.
constexpr float kNewLightHeight = 15.0F;
constexpr float kNewLightRange = 50.0F;
constexpr std::array<float, 3> kNewLightColor{2.0F, 1.7F, 1.2F};
// Une lumière neuve ne luit qu'à la nuit, du soir au matin.
constexpr float kNewLightFromHour = 17.0F;
constexpr float kNewLightToHour = 9.0F;
// La plus petite portée qu'une lumière garde : le document refuse une portée
// nulle, et la relecture rejetterait tout le document avec elle.
constexpr float kSmallestLightRange = 0.5F;
// Une composante de couleur ni négative, ni plus de quatre fois la pleine
// intensité.
constexpr float kLargestLightColor = 4.0F;

}  // namespace

ro::ui::MapEditorLight ShownLight(const ro::mapedit::AddedLight& light) {
  return ro::ui::MapEditorLight{.position = light.position,
                                .color = light.color,
                                .range = light.range,
                                .night_only = light.night_only,
                                .hours = ShownHours(light.hours),
                                .flicker = light.flicker,
                                .shadow = light.shadow,
                                .why = light.why};
}

ro::mapedit::AddedLight DocumentLight(const ro::ui::MapEditorLight& light) {
  return ro::mapedit::AddedLight{.position = light.position,
                                 .color = light.color,
                                 .range = light.range,
                                 .night_only = light.night_only,
                                 .hours = DocumentHours(light.hours),
                                 .flicker = light.flicker,
                                 .shadow = light.shadow,
                                 .why = light.why};
}

ro::mapedit::AddedLight NewLight(const std::array<float, 3>& position) {
  return ro::mapedit::AddedLight{
      .position = position,
      .color = kNewLightColor,
      .range = kNewLightRange,
      .night_only = true,
      .hours = ro::mapedit::LitHours{.from_hour = kNewLightFromHour, .to_hour = kNewLightToHour},
      .why = {}};
}

std::array<float, 3> AboveDropPoint(const std::array<float, 3>& point) {
  return {point[0], point[1] - kNewLightHeight, point[2]};
}

ro::mapedit::AddedLight BoundedLight(ro::mapedit::AddedLight light) {
  light.range = std::max(light.range, kSmallestLightRange);
  for (float& component : light.color) {
    component = std::clamp(component, 0.0F, kLargestLightColor);
  }
  return light;
}

ro::ui::MapEditorHours ShownHours(const std::optional<ro::mapedit::LitHours>& hours) {
  const ro::mapedit::LitHours shown = hours.value_or(ro::mapedit::LitHours());
  return ro::ui::MapEditorHours{
      .chosen = hours.has_value(), .from_hour = shown.from_hour, .to_hour = shown.to_hour};
}

std::optional<ro::mapedit::LitHours> DocumentHours(const ro::ui::MapEditorHours& hours) {
  if (!hours.chosen) {
    return std::nullopt;
  }
  return ro::mapedit::LitHours{
      .from_hour = std::clamp(hours.from_hour, 0.0F, ro::mapedit::kLitHoursPerDay),
      .to_hour = std::clamp(hours.to_hour, 0.0F, ro::mapedit::kLitHoursPerDay)};
}

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
  ChooseLight(picked);
  return true;
}

bool MapEditorSession::GrabLightHandle(glm::vec2 pointer, const ro::ui::MapEditorView& view) {
  if (!view.selected_light.has_value() || *view.selected_light >= view.lights.size()) {
    return false;
  }
  const std::optional<std::size_t> grabbed = GrabbedHandle(handles_, pointer);
  if (!grabbed.has_value()) {
    return false;
  }
  const ro::ui::map_editor::Place start{.position = view.lights[*view.selected_light].position,
                                        .released = false};
  drag_ = Drag{.handle = *grabbed,
               .grabbed = handles_[*grabbed],
               .frame = gizmo_frame_,
               .from = pointer,
               .start = start,
               .last = start,
               .light = view.selected_light};
  return true;
}

void MapEditorSession::ChooseLight(std::optional<std::size_t> slot) {
  selected_light_ = slot;
  if (slot.has_value()) {
    selection_.Clear();
    vertex_.reset();
    selected_effect_.reset();
    selected_map_effect_.reset();
    selected_sprite_.reset();
    selected_sound_.reset();
  }
}

void MapEditorSession::ChooseModel(std::optional<std::size_t> model_index) {
  if (selection_.model_index != model_index) {
    vertex_.reset();
  }
  selection_.model_index = model_index;
  if (model_index.has_value()) {
    selected_light_.reset();
    selected_effect_.reset();
    selected_map_effect_.reset();
    selected_sprite_.reset();
    selected_sound_.reset();
  }
}

bool MapEditorSession::Deselect() noexcept {
  if (!active_ || drag_.has_value()) {
    return false;
  }
  if (ground_points_mode() && DeselectGroundPoints()) {
    return true;
  }
  if (vertex_.has_value()) {
    vertex_.reset();
    return true;
  }
  // LA LUMIÈRE DE MODÈLE se lâche avant sa pose.
  if (selected_model_light_.has_value() && !selection_.empty()) {
    selected_model_light_.reset();
    return true;
  }
  const bool held = !selection_.empty() || selected_light_.has_value() ||
                    selected_effect_.has_value() || selected_map_effect_.has_value() ||
                    selected_sprite_.has_value() || selected_sound_.has_value();
  selection_.Clear();
  selected_light_.reset();
  selected_effect_.reset();
  selected_map_effect_.reset();
  selected_sprite_.reset();
  selected_sound_.reset();
  return held;
}

MapEditorSession::Outcome MapEditorSession::Focus(const ro::render::MapScene& scene,
                                                  const MapEditStore& store) {
  const std::vector<ro::mapedit::AddedLight>& lights = store.Document(map_name_).lights();
  const std::vector<ro::mapedit::AddedEffect>& effects = store.Document(map_name_).effects();
  const std::vector<ro::mapedit::MapSprite>& sprites = store.Document(map_name_).sprites();
  if (selected_sprite_.has_value() && *selected_sprite_ < sprites.size()) {
    view_point_ = sprites[*selected_sprite_].position;
  } else if (const std::optional<std::array<float, 3>> sound = SoundFocus(scene, store)) {
    view_point_ = sound;
  } else if (selected_map_effect_.has_value() &&
             *selected_map_effect_ < scene.rsw.effects().size()) {
    const ro::mapedit::EffectEdit* retouch =
        store.Document(map_name_).EffectEditAt(*selected_map_effect_);
    view_point_ = retouch != nullptr && retouch->position.has_value()
                      ? *retouch->position
                      : scene.rsw.effects()[*selected_map_effect_].position;
  } else if (selected_effect_.has_value() && *selected_effect_ < effects.size()) {
    view_point_ = effects[*selected_effect_].position;
  } else if (selected_light_.has_value() && *selected_light_ < lights.size()) {
    view_point_ = lights[*selected_light_].position;
  } else if (const std::optional<std::size_t> carried = ChosenModelLight(scene, store)) {
    view_point_ = ro::mapedit::ModelLightPosition(
        *SelectedPose(scene),
        store.Document(ro::mapedit::kModelLightsDocument).model_lights()[*carried].light.position);
  } else if (!selection_.empty() && *selection_.model_index < scene.rsw.models().size()) {
    view_point_ = scene.rsw.models()[*selection_.model_index].position;
  }
  return {};
}

MapEditorSession::Outcome MapEditorSession::MoveLight(const ro::ui::map_editor::MoveLight& move,
                                                      MapEditStore& store) {
  const std::vector<ro::mapedit::AddedLight>& lights = store.Document(map_name_).lights();
  if (move.slot >= lights.size()) {
    return {};
  }
  ro::ui::map_editor::SetLight set{
      .slot = move.slot, .light = ShownLight(lights[move.slot]), .released = move.released};
  set.light.position = move.native;
  return SetLight(set, store);
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

MapEditorSession::Outcome MapEditorSession::AddLight(
    const std::optional<std::array<float, 3>>& position, MapEditStore& store) {
  const ro::mapedit::MapEdit before = store.Document(map_name_);
  ro::mapedit::MapEdit after = before;
  after.AddLight(NewLight(position.value_or(AboveDropPoint(drop_point_))));
  store.Set(map_name_, WithReasons(after));
  ChooseLight(after.lights().size() - 1);
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
  after.SetLight(set.slot, BoundedLight(DocumentLight(set.light)));
  if (!(after == before)) {
    store.Set(map_name_, WithReasons(after));
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
  store.Set(map_name_, WithReasons(after));
  selected_light_.reset();
  std::array<char, kLabelCapacity> label{};
  std::snprintf(label.data(), label.size(), ro::core::Tr("Retirer la lumière %zu"), slot);
  Record(label.data(), before, after);
  return {};
}

}  // namespace moonclient
