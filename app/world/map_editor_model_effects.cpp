// LES EFFETS DE MODÈLE DE L'ÉDITEUR DE CARTE : voir docs/adr/0129.
//
// Un effet s'assigne au fichier de la pose désignée, et toutes ses poses le
// lèvent, sur toutes les cartes : les flammes de tous les braseros. Il se règle
// sur la pose désignée — sa position est celle qu'il y prend — et s'écrit dans
// le repère de la pose, dans le document commun `kModelLightsDocument`, comme
// une lumière de modèle. L'extinction sur une pose est celle de ses lumières.

#include <algorithm>
#include <array>
#include <cstdio>
#include <optional>
#include <string>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/mapedit/model_light.h"

#include "world/map_editor_reasons.h"
#include "world/map_editor_session.h"

namespace moonclient {
namespace {

// La place d'un nom de geste ou d'une nouvelle, zéro final compris.
constexpr std::size_t kLineCapacity = 192;
using Line = std::array<char, kLineCapacity>;

constexpr std::string_view kBook = ro::mapedit::kModelLightsDocument;

}  // namespace

std::optional<std::size_t> MapEditorSession::ChosenModelEffect(const ro::render::MapScene& scene,
                                                               const MapEditStore& store) const {
  if (!selected_model_effect_.has_value() || SelectedPose(scene) == nullptr ||
      *selection_.model_index >= scene.model_keys.size()) {
    return std::nullopt;
  }
  const std::vector<ro::mapedit::ModelEffect>& carried = store.Document(kBook).model_effects();
  if (*selected_model_effect_ >= carried.size() ||
      ro::mapedit::ModelFileKey(carried[*selected_model_effect_].model_file) !=
          scene.model_keys[*selection_.model_index]) {
    return std::nullopt;
  }
  return selected_model_effect_;
}

void MapEditorSession::ViewModelEffects(const ro::render::MapScene& scene,
                                        const MapEditStore& store,
                                        ro::ui::MapEditorView& view) const {
  const ro::formats::RswModel* pose = SelectedPose(scene);
  if (pose == nullptr || *selection_.model_index >= scene.model_keys.size()) {
    return;
  }
  const std::string& key = scene.model_keys[*selection_.model_index];
  const std::vector<ro::mapedit::ModelEffect>& carried = store.Document(kBook).model_effects();
  for (std::size_t slot = 0; slot < carried.size(); ++slot) {
    if (ro::mapedit::ModelFileKey(carried[slot].model_file) != key) {
      continue;
    }
    ro::ui::MapEditorEffect shown = ShownEffect(carried[slot].effect);
    shown.position = ro::mapedit::ModelLightPosition(*pose, carried[slot].effect.position);
    view.model_effects.push_back(ro::ui::MapEditorModelEffect{.slot = slot, .effect = shown});
  }
  view.selected_model_effect = ChosenModelEffect(scene, store);
}

bool MapEditorSession::GrabModelEffectHandle(glm::vec2 pointer, const ro::ui::MapEditorView& view) {
  const auto shown = std::ranges::find_if(view.model_effects,
                                          [&view](const ro::ui::MapEditorModelEffect& carried) {
                                            return view.selected_model_effect == carried.slot;
                                          });
  if (shown == view.model_effects.end()) {
    return false;
  }
  const std::optional<std::size_t> grabbed = GrabbedHandle(handles_, pointer);
  if (!grabbed.has_value()) {
    return false;
  }
  const ro::ui::map_editor::Place start{.position = shown->effect.position, .released = false};
  drag_ = Drag{.handle = *grabbed,
               .grabbed = handles_[*grabbed],
               .frame = gizmo_frame_,
               .from = pointer,
               .start = start,
               .last = start,
               .model_effect = shown->slot};
  return true;
}

MapEditorSession::Outcome MapEditorSession::ApplyModelEffect(
    const ro::ui::map_editor::AddModelEffect& asked, const ro::render::MapScene& scene,
    MapEditStore& store) {
  const ro::formats::RswModel* pose = SelectedPose(scene);
  if (pose == nullptr) {
    status_ = ro::core::Tr("Désignez d'abord un modèle.");
    return {};
  }
  const std::array<float, 3> position = asked.position.value_or(AboveDropPoint(pose->position));
  const ro::mapedit::MapEdit before = store.Document(kBook);
  ro::mapedit::MapEdit after = before;
  const std::string file = SelectedFile(scene);
  after.AddModelEffect(ro::mapedit::ModelEffect{
      .model_file = file,
      .effect = NewEffect(asked.number, ro::mapedit::ModelLightOffset(*pose, position))});
  store.Set(kBook, WithReasons(after));
  selected_model_effect_ = after.model_effects().size() - 1;
  selected_model_light_.reset();
  Line label{};
  std::snprintf(label.data(), label.size(), ro::core::Tr("Assigner un effet à %s"), file.c_str());
  RecordModelLights(label.data(), before, after);
  std::snprintf(label.data(), label.size(),
                ro::core::Tr("Toutes les poses de %s portent cet effet, sur toutes les cartes."),
                file.c_str());
  status_ = label.data();
  return {};
}

MapEditorSession::Outcome MapEditorSession::ApplyModelEffect(
    const ro::ui::map_editor::SelectModelEffect& select, const ro::render::MapScene& /*scene*/,
    MapEditStore& /*store*/) {
  selected_model_effect_ = select.slot;
  selected_model_light_.reset();
  return {};
}

MapEditorSession::Outcome MapEditorSession::ApplyModelEffect(
    const ro::ui::map_editor::SetModelEffect& set, const ro::render::MapScene& scene,
    MapEditStore& store) {
  const ro::formats::RswModel* pose = SelectedPose(scene);
  const ro::mapedit::MapEdit before = store.Document(kBook);
  if (pose == nullptr || set.slot >= before.model_effects().size()) {
    return {};
  }
  if (!model_effect_gesture_before_.has_value()) {
    model_effect_gesture_before_ = before;
  }
  ro::mapedit::ModelEffect carried = before.model_effects()[set.slot];
  const std::array<float, 3> offset = carried.effect.position;
  carried.effect = DocumentEffect(set.effect);
  // LA POSITION NE SE RECONVERTIT QUE SI ELLE A BOUGÉ : l'aller et retour par
  // la pose arrondirait l'écart d'un effet dont on ne règle que la cadence.
  carried.effect.position = set.effect.position == ro::mapedit::ModelLightPosition(*pose, offset)
                                ? offset
                                : ro::mapedit::ModelLightOffset(*pose, set.effect.position);
  ro::mapedit::MapEdit after = before;
  after.SetModelEffect(set.slot, std::move(carried));
  if (!(after == before)) {
    store.Set(kBook, WithReasons(after));
  }
  if (set.released) {
    Line label{};
    std::snprintf(label.data(), label.size(), ro::core::Tr("Régler l'effet de modèle %zu"),
                  set.slot);
    RecordModelLights(label.data(), *model_effect_gesture_before_, store.Document(kBook));
    model_effect_gesture_before_.reset();
  }
  return {};
}

MapEditorSession::Outcome MapEditorSession::ApplyModelEffect(
    const ro::ui::map_editor::MoveModelEffect& move, const ro::render::MapScene& scene,
    MapEditStore& store) {
  const std::vector<ro::mapedit::ModelEffect>& carried = store.Document(kBook).model_effects();
  if (move.slot >= carried.size()) {
    return {};
  }
  ro::ui::map_editor::SetModelEffect set{.slot = move.slot,
                                         .effect = ShownEffect(carried[move.slot].effect),
                                         .released = move.released};
  set.effect.position = move.native;
  return ApplyModelEffect(set, scene, store);
}

MapEditorSession::Outcome MapEditorSession::ApplyModelEffect(
    const ro::ui::map_editor::RemoveModelEffect& remove, const ro::render::MapScene& /*scene*/,
    MapEditStore& store) {
  const ro::mapedit::MapEdit before = store.Document(kBook);
  if (remove.slot >= before.model_effects().size()) {
    return {};
  }
  ro::mapedit::MapEdit after = before;
  after.RemoveModelEffect(remove.slot);
  store.Set(kBook, WithReasons(after));
  selected_model_effect_.reset();
  Line label{};
  std::snprintf(label.data(), label.size(), ro::core::Tr("Retirer l'effet de modèle %zu"),
                remove.slot);
  RecordModelLights(label.data(), before, after);
  return {};
}

}  // namespace moonclient
