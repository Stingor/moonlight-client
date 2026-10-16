#include "world/map_editor_session.h"

#include <array>
#include <cstdio>
#include <type_traits>
#include <utility>
#include <variant>

#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/core/utf8_path.h"
#include "ro/vfs/encoding.h"

namespace moonclient {
namespace {

// La place d'un nom de geste ou d'une phrase d'état, zéro final compris.
constexpr std::size_t kLineCapacity = 192;
using Line = std::array<char, kLineCapacity>;

// Le placement d'origine du modèle de rang `rank`, ou rien hors de la liste.
const ro::formats::RswModel* OriginalOf(const ro::render::MapScene& scene, std::size_t rank) {
  return rank < scene.unfixed_models.size() ? &scene.unfixed_models[rank] : nullptr;
}

}  // namespace

void MapEditorSession::Leave() noexcept {
  active_ = false;
  gesture_before_.reset();
}

void MapEditorSession::FollowMap(std::string_view map_name) {
  if (map_name == map_name_) {
    return;
  }
  map_name_ = std::string(map_name);
  history_.Clear();
  selection_.Clear();
  gesture_before_.reset();
  pending_why_.clear();
  status_.clear();
}

void MapEditorSession::Pick(std::optional<std::size_t> model_index) {
  if (!active_) {
    return;
  }
  if (model_index.has_value()) {
    selection_.model_index = model_index;
    status_.clear();
  } else {
    status_ = ro::core::Tr("Aucun modèle sous le clic.");
  }
}

MapEditorSession::Outcome MapEditorSession::Apply(const ro::ui::MapEditorIntent& intent,
                                                  const ro::render::MapScene& scene,
                                                  MapEditStore& store) {
  namespace intents = ro::ui::map_editor;
  return std::visit(
      [&](const auto& asked) -> Outcome {
        using Asked = std::decay_t<decltype(asked)>;
        if constexpr (std::is_same_v<Asked, intents::Toggle>) {
          if (active_) {
            Leave();
          } else {
            active_ = true;
          }
          return {};
        } else if constexpr (std::is_same_v<Asked, intents::Undo>) {
          std::optional<ro::mapedit::MapEdit> before = history_.Undo();
          if (!before.has_value()) {
            return {};
          }
          store.Set(map_name_, std::move(*before));
          return {.replace_models = true};
        } else if constexpr (std::is_same_v<Asked, intents::Redo>) {
          std::optional<ro::mapedit::MapEdit> after = history_.Redo();
          if (!after.has_value()) {
            return {};
          }
          store.Set(map_name_, std::move(*after));
          return {.replace_models = true};
        } else if constexpr (std::is_same_v<Asked, intents::Select>) {
          selection_.model_index = asked.model_index;
          return {};
        } else if constexpr (std::is_same_v<Asked, intents::Place>) {
          return Place(asked, scene, store);
        } else if constexpr (std::is_same_v<Asked, intents::SetWhy>) {
          return SetWhy(asked.why, store);
        } else if constexpr (std::is_same_v<Asked, intents::Revert>) {
          return Revert(store);
        } else {
          Save(store);
          return {};
        }
      },
      intent);
}

MapEditorSession::Outcome MapEditorSession::Place(const ro::ui::map_editor::Place& place,
                                                  const ro::render::MapScene& scene,
                                                  MapEditStore& store) {
  if (selection_.empty()) {
    return {};
  }
  const std::size_t rank = *selection_.model_index;
  const ro::formats::RswModel* original = OriginalOf(scene, rank);
  if (original == nullptr) {
    return {};
  }
  const ro::mapedit::MapEdit before = store.Document(map_name_);
  if (!gesture_before_.has_value()) {
    gesture_before_ = before;
  }
  // SEULS LES CHAMPS QUI S'ÉCARTENT DU PLACEMENT D'ORIGINE s'écrivent.
  ro::mapedit::ModelEdit wanted;
  wanted.model_index = rank;
  wanted.model_file = ro::vfs::Cp949ToUtf8Lossy(original->model_name);
  wanted.origin = original->position;
  if (place.position != original->position) {
    wanted.position = place.position;
  }
  if (place.rotation != original->rotation) {
    wanted.rotation = place.rotation;
  }
  if (place.scale != original->scale) {
    wanted.scale = place.scale;
  }
  wanted.hidden = place.hidden;
  const ro::mapedit::ModelEdit* existing = before.ModelAt(rank);
  if (existing != nullptr) {
    wanted.why = existing->why;
  } else if (const auto pending = pending_why_.find(rank); pending != pending_why_.end()) {
    wanted.why = pending->second;
  }
  const bool idle = !wanted.position.has_value() && !wanted.rotation.has_value() &&
                    !wanted.scale.has_value() && !wanted.hidden;
  ro::mapedit::MapEdit after = before;
  if (idle) {
    after.RemoveModel(rank);
  } else {
    after.SetModel(std::move(wanted));
  }
  Outcome outcome;
  if (!(after == before)) {
    store.Set(map_name_, after);
    outcome.replace_models = true;
  }
  if (place.released) {
    Line label{};
    std::snprintf(label.data(), label.size(), ro::core::Tr("Placer le modèle %zu"), rank);
    Record(label.data(), *gesture_before_, store.Document(map_name_));
    gesture_before_.reset();
  }
  return outcome;
}

MapEditorSession::Outcome MapEditorSession::SetWhy(std::string why, MapEditStore& store) {
  if (selection_.empty()) {
    return {};
  }
  const std::size_t rank = *selection_.model_index;
  const ro::mapedit::MapEdit before = store.Document(map_name_);
  const ro::mapedit::ModelEdit* existing = before.ModelAt(rank);
  if (existing == nullptr) {
    // Pas encore de retouche : la raison l'attendra.
    pending_why_[rank] = std::move(why);
    return {};
  }
  if (existing->why == why) {
    return {};
  }
  ro::mapedit::ModelEdit edit = *existing;
  edit.why = std::move(why);
  ro::mapedit::MapEdit after = before;
  after.SetModel(std::move(edit));
  store.Set(map_name_, after);
  Line label{};
  std::snprintf(label.data(), label.size(), ro::core::Tr("Raison du modèle %zu"), rank);
  Record(label.data(), before, after);
  // Une raison seule ne repose rien.
  return {};
}

MapEditorSession::Outcome MapEditorSession::Revert(MapEditStore& store) {
  if (selection_.empty()) {
    return {};
  }
  const std::size_t rank = *selection_.model_index;
  const ro::mapedit::MapEdit before = store.Document(map_name_);
  if (before.ModelAt(rank) == nullptr) {
    return {};
  }
  ro::mapedit::MapEdit after = before;
  after.RemoveModel(rank);
  store.Set(map_name_, after);
  Line label{};
  std::snprintf(label.data(), label.size(), ro::core::Tr("Rétablir le modèle %zu"), rank);
  Record(label.data(), before, after);
  return {.replace_models = true};
}

void MapEditorSession::Save(MapEditStore& store) {
  // UNE RETOUCHE SANS RAISON NE S'ÉCRIT PAS : le document la refuserait à la
  // relecture, et toutes les autres avec elle.
  for (const ro::mapedit::ModelEdit& edit : store.Document(map_name_).models()) {
    if (edit.why.empty()) {
      Line refused{};
      std::snprintf(refused.data(), refused.size(),
                    ro::core::Tr("La retouche du modèle %zu n'a pas de raison."), edit.model_index);
      status_ = refused.data();
      return;
    }
  }
  if (auto saved = store.Save(map_name_); !saved) {
    status_ = ro::core::Describe(saved.error());
    ro::core::LogError("édition de {} : {}", map_name_, status_);
    return;
  }
  status_ = ro::core::Tr("Enregistré.");
  ro::core::LogInfo("édition de {} enregistrée dans {}", map_name_,
                    ro::core::Utf8FromPath(store.FileOf(map_name_)));
}

void MapEditorSession::Record(std::string label, const ro::mapedit::MapEdit& before,
                              const ro::mapedit::MapEdit& after) {
  if (!(before == after)) {
    history_.Record(std::move(label), before, after);
  }
}

ro::ui::MapEditorView MapEditorSession::View(const ro::render::MapScene& scene,
                                             const MapEditStore& store) const {
  ro::ui::MapEditorView view;
  view.active = active_;
  view.map_name = map_name_;
  view.undo_label = history_.UndoLabel();
  view.redo_label = history_.RedoLabel();
  view.unsaved = store.Unsaved(map_name_);
  view.status = status_;
  const ro::mapedit::MapEdit& document = store.Document(map_name_);
  for (const ro::mapedit::ModelEdit& edit : document.models()) {
    view.edited.push_back(edit.model_index);
  }
  if (selection_.empty()) {
    return view;
  }
  const std::size_t rank = *selection_.model_index;
  const ro::formats::RswModel* original = OriginalOf(scene, rank);
  if (original == nullptr) {
    return view;
  }
  const ro::mapedit::ModelEdit* edit = document.ModelAt(rank);
  view.selected = rank;
  view.selected_file = ro::vfs::Cp949ToUtf8Lossy(original->model_name);
  view.origin = original->position;
  view.position =
      edit != nullptr ? edit->position.value_or(original->position) : original->position;
  view.rotation =
      edit != nullptr ? edit->rotation.value_or(original->rotation) : original->rotation;
  view.scale = edit != nullptr ? edit->scale.value_or(original->scale) : original->scale;
  view.hidden = edit != nullptr && edit->hidden;
  if (edit != nullptr) {
    view.why = edit->why;
  } else if (const auto pending = pending_why_.find(rank); pending != pending_why_.end()) {
    view.why = pending->second;
  }
  return view;
}

}  // namespace moonclient
