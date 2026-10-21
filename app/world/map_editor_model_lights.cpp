// LES LUMIÈRES DE MODÈLE DE L'ÉDITEUR DE CARTE : voir docs/adr/0126.
//
// Une lumière s'assigne au fichier de la pose désignée, et toutes ses poses la
// portent, sur toutes les cartes. Elle se règle sur la pose désignée — sa
// position est celle qu'elle y prend — et s'écrit dans le repère de la pose,
// dans le document commun `kModelLightsDocument`. L'extinction sur une pose,
// elle, est une retouche de la carte.

#include <algorithm>
#include <array>
#include <cstdio>
#include <optional>
#include <string>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/mapedit/model_light.h"
#include "ro/vfs/encoding.h"

#include "world/map_editor_reasons.h"
#include "world/map_editor_session.h"

namespace moonclient {
namespace {

// La place d'un nom de geste ou d'une nouvelle, zéro final compris.
constexpr std::size_t kLineCapacity = 192;
using Line = std::array<char, kLineCapacity>;

constexpr std::string_view kBook = ro::mapedit::kModelLightsDocument;

}  // namespace

const ro::formats::RswModel* MapEditorSession::SelectedPose(
    const ro::render::MapScene& scene) const {
  if (selection_.empty() || *selection_.model_index >= scene.rsw.models().size()) {
    return nullptr;
  }
  return &scene.rsw.models()[*selection_.model_index];
}

std::string MapEditorSession::SelectedFile(const ro::render::MapScene& scene) const {
  const ro::formats::RswModel* pose = SelectedPose(scene);
  return pose != nullptr ? ro::vfs::Cp949ToUtf8Lossy(pose->model_name) : std::string();
}

std::optional<std::size_t> MapEditorSession::ChosenModelLight(const ro::render::MapScene& scene,
                                                              const MapEditStore& store) const {
  if (!selected_model_light_.has_value() || SelectedPose(scene) == nullptr ||
      *selection_.model_index >= scene.model_keys.size()) {
    return std::nullopt;
  }
  const std::vector<ro::mapedit::ModelLight>& carried = store.Document(kBook).model_lights();
  if (*selected_model_light_ >= carried.size() ||
      ro::mapedit::ModelFileKey(carried[*selected_model_light_].model_file) !=
          scene.model_keys[*selection_.model_index]) {
    return std::nullopt;
  }
  return selected_model_light_;
}

void MapEditorSession::ViewModelLights(const ro::render::MapScene& scene, const MapEditStore& store,
                                       ro::ui::MapEditorView& view) const {
  const ro::formats::RswModel* pose = SelectedPose(scene);
  if (pose == nullptr || *selection_.model_index >= scene.model_keys.size()) {
    return;
  }
  const std::string& key = scene.model_keys[*selection_.model_index];
  const std::vector<ro::mapedit::ModelLight>& carried = store.Document(kBook).model_lights();
  for (std::size_t slot = 0; slot < carried.size(); ++slot) {
    if (ro::mapedit::ModelFileKey(carried[slot].model_file) != key) {
      continue;
    }
    ro::ui::MapEditorLight shown = ShownLight(carried[slot].light);
    shown.position = ro::mapedit::ModelLightPosition(*pose, carried[slot].light.position);
    view.model_lights.push_back(ro::ui::MapEditorModelLight{.slot = slot, .light = shown});
  }
  view.selected_model_light = ChosenModelLight(scene, store);
  view.selected_unlit = std::ranges::binary_search(scene.unlit_models, *selection_.model_index);
}

bool MapEditorSession::GrabModelLightHandle(glm::vec2 pointer, const ro::ui::MapEditorView& view) {
  const auto shown =
      std::ranges::find_if(view.model_lights, [&view](const ro::ui::MapEditorModelLight& carried) {
        return view.selected_model_light == carried.slot;
      });
  if (shown == view.model_lights.end()) {
    return false;
  }
  const std::optional<std::size_t> grabbed = GrabbedHandle(handles_, pointer);
  if (!grabbed.has_value()) {
    return false;
  }
  const ro::ui::map_editor::Place start{.position = shown->light.position, .released = false};
  drag_ = Drag{.handle = *grabbed,
               .grabbed = handles_[*grabbed],
               .frame = gizmo_frame_,
               .from = pointer,
               .start = start,
               .last = start,
               .model_light = shown->slot};
  return true;
}

MapEditorSession::Outcome MapEditorSession::AddModelLight(
    const ro::ui::map_editor::AddModelLight& asked, const ro::render::MapScene& scene,
    MapEditStore& store) {
  const ro::formats::RswModel* pose = SelectedPose(scene);
  if (pose == nullptr) {
    status_ = ro::core::Tr("Désignez d'abord un modèle.");
    return {};
  }
  const std::array<float, 3> position = asked.position.value_or(AboveDropPoint(pose->position));
  ro::mapedit::AddedLight light = NewLight(ro::mapedit::ModelLightOffset(*pose, position));
  const ro::mapedit::MapEdit before = store.Document(kBook);
  ro::mapedit::MapEdit after = before;
  const std::string file = SelectedFile(scene);
  after.AddModelLight(ro::mapedit::ModelLight{.model_file = file, .light = std::move(light)});
  store.Set(kBook, WithReasons(after));
  selected_model_light_ = after.model_lights().size() - 1;
  selected_model_effect_.reset();
  Line label{};
  std::snprintf(label.data(), label.size(), ro::core::Tr("Assigner une lumière à %s"),
                file.c_str());
  RecordModelLights(label.data(), before, after);
  std::snprintf(
      label.data(), label.size(),
      ro::core::Tr("Toutes les poses de %s portent cette lumière, sur toutes les cartes."),
      file.c_str());
  status_ = label.data();
  return {};
}

MapEditorSession::Outcome MapEditorSession::SetModelLight(
    const ro::ui::map_editor::SetModelLight& set, const ro::render::MapScene& scene,
    MapEditStore& store) {
  const ro::formats::RswModel* pose = SelectedPose(scene);
  const ro::mapedit::MapEdit before = store.Document(kBook);
  if (pose == nullptr || set.slot >= before.model_lights().size()) {
    return {};
  }
  if (!model_light_gesture_before_.has_value()) {
    model_light_gesture_before_ = before;
  }
  ro::mapedit::ModelLight carried = before.model_lights()[set.slot];
  const std::array<float, 3> offset = carried.light.position;
  carried.light = BoundedLight(DocumentLight(set.light));
  // LA POSITION NE SE RECONVERTIT QUE SI ELLE A BOUGÉ : l'aller et retour par
  // la pose arrondirait l'écart d'une lumière dont on ne règle que la couleur.
  carried.light.position = set.light.position == ro::mapedit::ModelLightPosition(*pose, offset)
                               ? offset
                               : ro::mapedit::ModelLightOffset(*pose, set.light.position);
  ro::mapedit::MapEdit after = before;
  after.SetModelLight(set.slot, std::move(carried));
  if (!(after == before)) {
    store.Set(kBook, WithReasons(after));
  }
  if (set.released) {
    Line label{};
    std::snprintf(label.data(), label.size(), ro::core::Tr("Régler la lumière de modèle %zu"),
                  set.slot);
    RecordModelLights(label.data(), *model_light_gesture_before_, store.Document(kBook));
    model_light_gesture_before_.reset();
  }
  return {};
}

MapEditorSession::Outcome MapEditorSession::MoveModelLight(
    const ro::ui::map_editor::MoveModelLight& move, const ro::render::MapScene& scene,
    MapEditStore& store) {
  const std::vector<ro::mapedit::ModelLight>& carried = store.Document(kBook).model_lights();
  if (move.slot >= carried.size()) {
    return {};
  }
  ro::ui::map_editor::SetModelLight set{
      .slot = move.slot, .light = ShownLight(carried[move.slot].light), .released = move.released};
  set.light.position = move.native;
  return SetModelLight(set, scene, store);
}

MapEditorSession::Outcome MapEditorSession::RemoveModelLight(std::size_t slot,
                                                             MapEditStore& store) {
  const ro::mapedit::MapEdit before = store.Document(kBook);
  if (slot >= before.model_lights().size()) {
    return {};
  }
  ro::mapedit::MapEdit after = before;
  after.RemoveModelLight(slot);
  store.Set(kBook, WithReasons(after));
  selected_model_light_.reset();
  Line label{};
  std::snprintf(label.data(), label.size(), ro::core::Tr("Retirer la lumière de modèle %zu"), slot);
  RecordModelLights(label.data(), before, after);
  return {};
}

MapEditorSession::Outcome MapEditorSession::SetModelUnlit(bool unlit,
                                                          const ro::render::MapScene& scene,
                                                          MapEditStore& store) {
  if (selection_.empty()) {
    return {};
  }
  const std::size_t rank = *selection_.model_index;
  const ro::mapedit::MapEdit before = store.Document(map_name_);
  ro::mapedit::MapEdit after = before;
  if (const std::optional<std::size_t> slot = AddedSlot(scene)) {
    if (*slot >= before.added().size()) {
      return {};
    }
    ro::mapedit::AddedModel added = before.added()[*slot];
    added.unlit = unlit;
    after.SetAdded(*slot, std::move(added));
  } else {
    if (rank >= scene.unfixed_models.size()) {
      return {};
    }
    const ro::formats::RswModel& original = scene.unfixed_models[rank];
    ro::mapedit::ModelEdit edit;
    if (const ro::mapedit::ModelEdit* existing = before.ModelAt(rank)) {
      edit = *existing;
    } else {
      edit = ro::mapedit::ModelEdit{.model_index = rank,
                                    .model_file = ro::vfs::Cp949ToUtf8Lossy(original.model_name),
                                    .origin = original.position};
      if (const auto pending = pending_why_.find(rank); pending != pending_why_.end()) {
        edit.why = pending->second;
      }
    }
    edit.unlit = unlit;
    // UNE RETOUCHE QUI NE CHANGE PLUS RIEN quitte le document.
    if (!edit.position.has_value() && !edit.rotation.has_value() && !edit.scale.has_value() &&
        !edit.hidden && !edit.opacity.has_value() && !edit.unlit) {
      after.RemoveModel(rank);
    } else {
      after.SetModel(std::move(edit));
    }
  }
  if (after == before) {
    return {};
  }
  store.Set(map_name_, WithReasons(after));
  Line label{};
  std::snprintf(label.data(), label.size(),
                unlit ? ro::core::Tr("Éteindre les lumières du modèle %zu")
                      : ro::core::Tr("Rallumer les lumières du modèle %zu"),
                rank);
  Record(label.data(), before, after);
  return {.replace_models = true};
}

}  // namespace moonclient
