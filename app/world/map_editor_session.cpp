#include "world/map_editor_session.h"

#include <array>
#include <cstdio>
#include <format>
#include <memory>
#include <type_traits>
#include <utility>
#include <variant>

#include "ro/core/i18n.h"
#include "ro/vfs/encoding.h"

#include "world/map_editor_reasons.h"

namespace moonclient {

std::array<float, 3> MapEditorNudge(NudgeDirection direction, bool shift_held) {
  const float step = shift_held ? kNudgeWideUnits : kNudgeUnits;
  switch (direction) {
    case NudgeDirection::kLeft:
      return {-step, 0.0F, 0.0F};
    case NudgeDirection::kRight:
      return {step, 0.0F, 0.0F};
    case NudgeDirection::kUp:
      return {0.0F, 0.0F, step};
    case NudgeDirection::kDown:
      return {0.0F, 0.0F, -step};
  }
  return {};
}

std::optional<ro::ui::MapEditorIntent> MapEditorShortcut(char key, bool shift_held) {
  switch (key) {
    case 'z':
      return shift_held ? ro::ui::MapEditorIntent(ro::ui::map_editor::Redo{})
                        : ro::ui::MapEditorIntent(ro::ui::map_editor::Undo{});
    case 'y':
      return ro::ui::map_editor::Redo{};
    case 's':
      return ro::ui::map_editor::Save{};
    case 'd':
      return ro::ui::map_editor::Duplicate{};
    case 'f':
      return ro::ui::map_editor::Focus{};
    case '1':
      return ro::ui::map_editor::SetTool{ro::ui::MapEditorTool::kMove};
    case '2':
      return ro::ui::map_editor::SetTool{ro::ui::MapEditorTool::kRotate};
    case '3':
      return ro::ui::map_editor::SetTool{ro::ui::MapEditorTool::kScale};
    case '4':
      return ro::ui::map_editor::SetTool{ro::ui::MapEditorTool::kVertices};
    case '5':
      return ro::ui::map_editor::SetTool{ro::ui::MapEditorTool::kRelief};
    default:
      return std::nullopt;
  }
}
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
  view_point_.reset();
  gesture_before_.reset();
  drag_.reset();
  ground_stroke_.reset();
  handles_.clear();
}

void MapEditorSession::SetGizmo(const GizmoFrame& frame, std::vector<GizmoHandle> handles) {
  // ⚠ PENDANT UN GLISSÉ, la poignée saisie garde le repère de son appui : le
  // pivot qui suit le modèle ferait sinon dériver le glissé.
  gizmo_frame_ = frame;
  handles_ = std::move(handles);
}

std::vector<ro::ui::MapEditorLine> MapEditorSession::GizmoTrace() const {
  return GizmoLines(handles_,
                    drag_.has_value() ? std::optional<std::size_t>(drag_->handle) : std::nullopt);
}

bool MapEditorSession::Grab(glm::vec2 pointer, const ro::ui::MapEditorView& view, bool extend) {
  // LE MODE SOMMETS DU RELIEF prend tout appui : sa poignée, un point, ou le
  // vide qui vide la sélection.
  if (active_ && ground_points_mode()) {
    return GrabGroundPoint(pointer, extend);
  }
  // LA BROSSE DE RELIEF prend tout appui sur le sol : elle n'a pas de poignée.
  if (active_ && tool_ == ro::ui::MapEditorTool::kRelief) {
    return GrabGround();
  }
  return active_ && ((view.selected.has_value() && GrabTool(pointer, view)) ||
                     GrabLightHandle(pointer, view) || GrabEffectHandle(pointer, view) ||
                     GrabMapEffectHandle(pointer, view) || GrabSpriteHandle(pointer, view) ||
                     GrabSoundHandle(pointer, view) || GrabLight(pointer) || GrabEffect(pointer) ||
                     GrabMapEffect(pointer) || GrabSprite(pointer) || GrabSound(pointer));
}

bool MapEditorSession::GrabTool(glm::vec2 pointer, const ro::ui::MapEditorView& view) {
  if (tool_ == ro::ui::MapEditorTool::kVertices) {
    return GrabVertex(pointer);
  }
  if (view.selected_model_light.has_value()) {
    return GrabModelLightHandle(pointer, view);
  }
  const std::optional<std::size_t> grabbed = GrabbedHandle(handles_, pointer);
  if (!grabbed.has_value()) {
    return false;
  }
  const ro::ui::map_editor::Place start{.position = view.position,
                                        .rotation = view.rotation,
                                        .scale = view.scale,
                                        .hidden = view.hidden,
                                        .released = false};
  drag_ = Drag{.handle = *grabbed,
               .grabbed = handles_[*grabbed],
               .frame = gizmo_frame_,
               .from = pointer,
               .start = start,
               .last = start};
  return true;
}

void MapEditorSession::DragTo(glm::vec2 pointer, bool snap) {
  if (!drag_.has_value()) {
    return;
  }
  // UN SOMMET, UNE LUMIÈRE se déplacent comme une pose, à la flèche.
  const ro::ui::MapEditorTool tool =
      drag_->vertex.has_value() || drag_->light.has_value() || drag_->model_light.has_value() ||
              drag_->effect.has_value() || drag_->map_effect.has_value() ||
              drag_->sprite.has_value() || drag_->sound.has_value() || drag_->ground_points
          ? ro::ui::MapEditorTool::kMove
          : tool_;
  drag_->last = Dragged(tool, drag_->grabbed, drag_->frame, drag_->start, pointer - drag_->from);
  if (snap && !drag_->ground_points) {
    drag_->last = Snapped(tool, drag_->start, drag_->last);
  }
  drag_->last.released = false;
  if (drag_->ground_points) {
    DragGroundPoints(snap);
    return;
  }
  if (drag_->sprite.has_value()) {
    queued_.emplace_back(ro::ui::map_editor::MoveSprite{
        .slot = *drag_->sprite, .native = drag_->last.position, .released = false});
    return;
  }
  if (drag_->sound.has_value()) {
    queued_.emplace_back(ro::ui::map_editor::MoveSound{
        .key = *drag_->sound, .native = drag_->last.position, .released = false});
    return;
  }
  if (drag_->map_effect.has_value()) {
    queued_.emplace_back(ro::ui::map_editor::MoveMapEffect{
        .index = *drag_->map_effect, .native = drag_->last.position, .released = false});
    return;
  }
  if (drag_->effect.has_value()) {
    queued_.emplace_back(ro::ui::map_editor::MoveEffect{
        .slot = *drag_->effect, .native = drag_->last.position, .released = false});
    return;
  }
  if (drag_->light.has_value()) {
    queued_.emplace_back(ro::ui::map_editor::MoveLight{
        .slot = *drag_->light, .native = drag_->last.position, .released = false});
    return;
  }
  if (drag_->model_light.has_value()) {
    queued_.emplace_back(ro::ui::map_editor::MoveModelLight{
        .slot = *drag_->model_light, .native = drag_->last.position, .released = false});
    return;
  }
  if (drag_->vertex.has_value()) {
    queued_.emplace_back(ro::ui::map_editor::MoveVertex{.node = drag_->vertex->first,
                                                        .vertex = drag_->vertex->second,
                                                        .native = drag_->last.position,
                                                        .released = false});
    return;
  }
  queued_.emplace_back(drag_->last);
}

void MapEditorSession::Release() {
  if (ground_stroke_.has_value()) {
    ground_stroke_.reset();
    queued_.emplace_back(ro::ui::map_editor::EndGroundStroke{});
    return;
  }
  if (!drag_.has_value()) {
    return;
  }
  ro::ui::map_editor::Place last = drag_->last;
  last.released = true;
  if (drag_->ground_points) {
    queued_.emplace_back(
        ro::ui::map_editor::LiftGroundPoints{.lift = last.position[1], .released = true});
  } else if (drag_->sprite.has_value()) {
    queued_.emplace_back(ro::ui::map_editor::MoveSprite{
        .slot = *drag_->sprite, .native = last.position, .released = true});
  } else if (drag_->sound.has_value()) {
    queued_.emplace_back(ro::ui::map_editor::MoveSound{
        .key = *drag_->sound, .native = last.position, .released = true});
  } else if (drag_->map_effect.has_value()) {
    queued_.emplace_back(ro::ui::map_editor::MoveMapEffect{
        .index = *drag_->map_effect, .native = last.position, .released = true});
  } else if (drag_->effect.has_value()) {
    queued_.emplace_back(ro::ui::map_editor::MoveEffect{
        .slot = *drag_->effect, .native = last.position, .released = true});
  } else if (drag_->light.has_value()) {
    queued_.emplace_back(ro::ui::map_editor::MoveLight{
        .slot = *drag_->light, .native = last.position, .released = true});
  } else if (drag_->model_light.has_value()) {
    queued_.emplace_back(ro::ui::map_editor::MoveModelLight{
        .slot = *drag_->model_light, .native = last.position, .released = true});
  } else if (drag_->vertex.has_value()) {
    queued_.emplace_back(ro::ui::map_editor::MoveVertex{.node = drag_->vertex->first,
                                                        .vertex = drag_->vertex->second,
                                                        .native = last.position,
                                                        .released = true});
  } else {
    queued_.emplace_back(last);
  }
  drag_.reset();
}

std::vector<ro::ui::MapEditorIntent> MapEditorSession::TakeQueued() {
  return std::exchange(queued_, {});
}

void MapEditorSession::FollowMap(std::string_view map_name, const ro::render::MapScene& scene) {
  FollowMapSounds(map_name, scene);
  if (map_name == map_name_ && model_files_.size() == scene.unfixed_models.size()) {
    return;
  }
  original_count_ = scene.unfixed_models.size();
  model_files_.clear();
  model_files_.reserve(scene.unfixed_models.size());
  for (const ro::formats::RswModel& model : scene.unfixed_models) {
    model_files_.push_back(ro::vfs::Cp949ToUtf8Lossy(model.model_name));
  }
  if (map_name == map_name_) {
    return;
  }
  map_name_ = std::string(map_name);
  ForgetDocument();
  status_.clear();
}

void MapEditorSession::Pick(std::optional<std::size_t> model_index) {
  if (!active_) {
    return;
  }
  if (model_index.has_value()) {
    ChooseModel(model_index);
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
        } else if constexpr (std::is_same_v<Asked, intents::Undo> ||
                             std::is_same_v<Asked, intents::Redo>) {
          return StepHistory(std::is_same_v<Asked, intents::Redo>, store);
        } else if constexpr (std::is_same_v<Asked, intents::Select>) {
          ChooseModel(asked.model_index);
          return {};
        } else if constexpr (std::is_same_v<Asked, intents::Place>) {
          return Place(asked, scene, store);
        } else if constexpr (std::is_same_v<Asked, intents::SetWhy>) {
          return SetWhy(asked.why, store);
        } else if constexpr (std::is_same_v<Asked, intents::Revert>) {
          return Revert(store);
        } else if constexpr (std::is_same_v<Asked, intents::SetTool>) {
          if (asked.tool == ro::ui::MapEditorTool::kRelief && !ground_allowed_) {
            return {};
          }
          tool_ = asked.tool;
          drag_.reset();
          ground_stroke_.reset();
          return {};
        } else if constexpr (std::is_same_v<Asked, intents::AddModel>) {
          return AddModel(asked, scene, store);
        } else if constexpr (std::is_same_v<Asked, intents::MoveVertex>) {
          return MoveVertex(asked, store);
        } else if constexpr (std::is_same_v<Asked, intents::RevertVertex>) {
          return RevertVertex(store);
        } else if constexpr (std::is_same_v<Asked, intents::SetAllPoses>) {
          return SetAllPoses(asked.all, store);
        } else if constexpr (std::is_same_v<Asked, intents::Duplicate>) {
          return Duplicate(scene, store);
        } else if constexpr (std::is_same_v<Asked, intents::Focus>) {
          return Focus(scene, store);
        } else if constexpr (std::is_same_v<Asked, intents::Delete>) {
          return Delete(scene, store);
        } else if constexpr (std::is_same_v<Asked, intents::Nudge>) {
          return Nudge(asked.delta, scene, store);
        } else if constexpr (std::is_same_v<Asked, intents::AddLight>) {
          return AddLight(asked.position, store);
        } else if constexpr (std::is_same_v<Asked, intents::SelectLight>) {
          ChooseLight(asked.slot);
          return {};
        } else if constexpr (std::is_same_v<Asked, intents::MoveLight>) {
          return MoveLight(asked, store);
        } else if constexpr (std::is_same_v<Asked, intents::SetLight>) {
          return SetLight(asked, store);
        } else if constexpr (std::is_same_v<Asked, intents::SelectMapEffect>) {
          ChooseMapEffect(asked.index);
          return {};
        } else if constexpr (std::is_same_v<Asked, intents::SetMapEffect>) {
          return SetMapEffect(asked, scene, store);
        } else if constexpr (std::is_same_v<Asked, intents::RevertMapEffect>) {
          return RevertMapEffect(asked.index, store);
        } else if constexpr (std::is_same_v<Asked, intents::MoveMapEffect>) {
          return MoveMapEffect(asked, scene, store);
        } else if constexpr (std::is_same_v<Asked, intents::AddEffect>) {
          return AddEffect(asked.number, store);
        } else if constexpr (std::is_same_v<Asked, intents::SelectEffect>) {
          ChooseEffect(asked.slot);
          return {};
        } else if constexpr (std::is_same_v<Asked, intents::SetEffect>) {
          return SetEffect(asked, store);
        } else if constexpr (std::is_same_v<Asked, intents::RemoveEffect>) {
          return RemoveEffect(asked.slot, store);
        } else if constexpr (std::is_same_v<Asked, intents::MoveEffect>) {
          return MoveEffect(asked, store);
        } else if constexpr (std::is_same_v<Asked, intents::AddSprite>) {
          return AddSprite(asked.sprite, store);
        } else if constexpr (std::is_same_v<Asked, intents::SelectSprite>) {
          ChooseSprite(asked.slot);
          return {};
        } else if constexpr (std::is_same_v<Asked, intents::SetSprite>) {
          return SetSprite(asked, store);
        } else if constexpr (std::is_same_v<Asked, intents::RemoveSprite>) {
          return RemoveSprite(asked.slot, store);
        } else if constexpr (std::is_same_v<Asked, intents::MoveSprite>) {
          return MoveSprite(asked, store);
        } else if constexpr (std::is_same_v<Asked, intents::RemoveLight>) {
          return RemoveLight(asked.slot, store);
        } else if constexpr (std::is_same_v<Asked, intents::AddModelLight>) {
          return AddModelLight(asked, scene, store);
        } else if constexpr (std::is_same_v<Asked, intents::SelectModelLight>) {
          selected_model_light_ = asked.slot;
          return {};
        } else if constexpr (std::is_same_v<Asked, intents::SetModelLight>) {
          return SetModelLight(asked, scene, store);
        } else if constexpr (std::is_same_v<Asked, intents::MoveModelLight>) {
          return MoveModelLight(asked, scene, store);
        } else if constexpr (std::is_same_v<Asked, intents::RemoveModelLight>) {
          return RemoveModelLight(asked.slot, store);
        } else if constexpr (std::is_same_v<Asked, intents::SetModelUnlit>) {
          return SetModelUnlit(asked.unlit, scene, store);
        } else if constexpr (std::is_same_v<Asked, intents::SetBrush>) {
          SetBrush(asked.brush);
          return {};
        } else if constexpr (std::is_same_v<Asked, intents::EndGroundStroke>) {
          EndGroundStroke(store);
          return {};
        } else if constexpr (std::is_same_v<Asked, intents::RevertGround>) {
          RevertGround(store);
          return {};
        } else if constexpr (std::is_same_v<Asked, intents::SetGroundWhy>) {
          SetGroundWhy(asked.why, store);
          return {};
        } else if constexpr (std::is_same_v<Asked, intents::LiftGroundPoints>) {
          return LiftGroundPoints(asked, store);
        } else if constexpr (std::is_same_v<Asked, intents::SetGroundPointHeight>) {
          return SetGroundPointHeight(asked, scene, store);
        } else if constexpr (std::is_same_v<Asked, intents::RevertGroundPoints>) {
          return RevertGroundPoints(store);
        } else if constexpr (std::is_same_v<Asked, intents::Overwrite>) {
          return {.publication = Outcome::Publication::kOverwrite};
        } else if constexpr (std::is_same_v<Asked, intents::Reload>) {
          return {.publication = Outcome::Publication::kReload};
        } else if constexpr (kIsSoundIntent<Asked>) {
          return ApplySound(asked, scene, store);
        } else {
          static_assert(std::is_same_v<Asked, intents::Save>, "une intention n'est pas routée");
          std::vector<std::string> saved = Save(store);
          return {.publication =
                      saved.empty() ? Outcome::Publication::kNone : Outcome::Publication::kPublish,
                  .saved_documents = std::move(saved)};
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
  if (const std::optional<std::size_t> slot = AddedSlot(scene)) {
    return PlaceAdded(*slot, place, store);
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
    wanted.opacity = existing->opacity;
    wanted.unlit = existing->unlit;
  } else if (const auto pending = pending_why_.find(rank); pending != pending_why_.end()) {
    wanted.why = pending->second;
  }
  if (place.opacity.has_value()) {
    wanted.opacity = ro::mapedit::DocumentOpacity(*place.opacity);
  }
  const bool idle = !wanted.position.has_value() && !wanted.rotation.has_value() &&
                    !wanted.scale.has_value() && !wanted.hidden && !wanted.opacity.has_value() &&
                    !wanted.unlit;
  ro::mapedit::MapEdit after = before;
  if (idle) {
    after.RemoveModel(rank);
  } else {
    after.SetModel(std::move(wanted));
  }
  Outcome outcome;
  if (!(after == before)) {
    store.Set(map_name_, WithReasons(after));
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

MapEditorSession::Outcome MapEditorSession::PlaceAdded(std::size_t slot,
                                                       const ro::ui::map_editor::Place& place,
                                                       MapEditStore& store) {
  const ro::mapedit::MapEdit before = store.Document(map_name_);
  if (slot >= before.added().size()) {
    return {};
  }
  if (!gesture_before_.has_value()) {
    gesture_before_ = before;
  }
  ro::mapedit::AddedModel added = before.added()[slot];
  added.position = place.position;
  added.rotation = place.rotation;
  added.scale = place.scale;
  added.hidden = place.hidden;
  if (place.opacity.has_value()) {
    added.opacity = ro::mapedit::DocumentOpacity(*place.opacity);
  }
  ro::mapedit::MapEdit after = before;
  after.SetAdded(slot, std::move(added));
  Outcome outcome;
  if (!(after == before)) {
    store.Set(map_name_, WithReasons(after));
    outcome.replace_models = true;
  }
  if (place.released) {
    Line label{};
    std::snprintf(label.data(), label.size(), ro::core::Tr("Placer le modèle %zu"),
                  original_count_ + slot);
    Record(label.data(), *gesture_before_, store.Document(map_name_));
    gesture_before_.reset();
  }
  return outcome;
}

MapEditorSession::Outcome MapEditorSession::AddModel(const ro::ui::map_editor::AddModel& asked,
                                                     const ro::render::MapScene& scene,
                                                     MapEditStore& store) {
  const ro::mapedit::MapEdit before = store.Document(map_name_);
  ro::mapedit::MapEdit after = before;
  after.AddModel(ro::mapedit::AddedModel{.model_file = asked.file,
                                         .position = asked.position.value_or(drop_point_),
                                         .rotation = asked.rotation,
                                         .scale = asked.scale,
                                         .opacity = asked.opacity,
                                         .animation_type = asked.animation_type,
                                         .animation_speed = asked.animation_speed});
  store.Set(map_name_, WithReasons(after));
  Record(ro::core::Tr("Ajouter un modèle"), before, after);
  // LE NOUVEAU EST DÉSIGNÉ : c'est lui qu'on va placer.
  selection_.model_index = scene.unfixed_models.size() + after.added().size() - 1;
  status_ = ro::core::Tr("Modèle ajouté : dites pourquoi avant d'enregistrer.");
  return {.replace_models = true};
}

std::optional<std::size_t> MapEditorSession::AddedSlot(const ro::render::MapScene& scene) const {
  if (selection_.empty() || *selection_.model_index < scene.unfixed_models.size()) {
    return std::nullopt;
  }
  return *selection_.model_index - scene.unfixed_models.size();
}

MapEditorSession::Outcome MapEditorSession::SetWhy(std::string why, MapEditStore& store) {
  if (selection_.empty()) {
    return {};
  }
  const std::size_t rank = *selection_.model_index;
  const ro::mapedit::MapEdit before = store.Document(map_name_);
  // L'OUTIL SOMMETS dit pourquoi le maillage est retouché.
  if (tool_ == ro::ui::MapEditorTool::kVertices) {
    const ro::mapedit::MeshEdit* mesh = before.MeshAt(rank);
    if (mesh == nullptr) {
      pending_mesh_why_[rank] = std::move(why);
      return {};
    }
    if (mesh->why == why) {
      return {};
    }
    ro::mapedit::MeshEdit edited = *mesh;
    edited.why = std::move(why);
    ro::mapedit::MapEdit after = before;
    after.SetMesh(std::move(edited));
    store.Set(map_name_, WithReasons(after));
    Line label{};
    std::snprintf(label.data(), label.size(), ro::core::Tr("Raison du maillage %zu"), rank);
    Record(label.data(), before, after);
    return {};
  }
  if (rank >= original_count_) {
    const std::size_t slot = rank - original_count_;
    if (slot >= before.added().size() || before.added()[slot].why == why) {
      return {};
    }
    ro::mapedit::AddedModel added = before.added()[slot];
    added.why = std::move(why);
    ro::mapedit::MapEdit after = before;
    after.SetAdded(slot, std::move(added));
    store.Set(map_name_, WithReasons(after));
    Line label{};
    std::snprintf(label.data(), label.size(), ro::core::Tr("Raison du modèle %zu"), rank);
    Record(label.data(), before, after);
    return {};
  }
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
  store.Set(map_name_, WithReasons(after));
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
  // UN MODÈLE AJOUTÉ SE RETIRE : il n'a pas de placement d'origine où revenir.
  if (rank >= original_count_) {
    const std::size_t slot = rank - original_count_;
    if (slot >= before.added().size()) {
      return {};
    }
    ro::mapedit::MapEdit after = before;
    after.RemoveAdded(slot);
    store.Set(map_name_, WithReasons(after));
    Line label{};
    std::snprintf(label.data(), label.size(), ro::core::Tr("Retirer le modèle %zu"), rank);
    Record(label.data(), before, after);
    selection_.Clear();
    return {.replace_models = true};
  }
  if (before.ModelAt(rank) == nullptr) {
    return {};
  }
  ro::mapedit::MapEdit after = before;
  after.RemoveModel(rank);
  store.Set(map_name_, WithReasons(after));
  Line label{};
  std::snprintf(label.data(), label.size(), ro::core::Tr("Rétablir le modèle %zu"), rank);
  Record(label.data(), before, after);
  return {.replace_models = true};
}

void MapEditorSession::RefreshSceneRows(const ro::mapedit::MapEdit& document) const {
  if (scene_rows_ != nullptr && rows_document_ == document && rows_map_ == map_name_ &&
      rows_model_count_ == model_files_.size()) {
    return;
  }
  auto rows = std::make_shared<std::vector<ro::ui::MapEditorModelRow>>();
  rows->reserve(model_files_.size() + document.added().size());
  for (std::size_t rank = 0; rank < model_files_.size(); ++rank) {
    rows->push_back(ro::ui::MapEditorModelRow{.model_index = rank, .file = model_files_[rank]});
  }
  for (const ro::mapedit::ModelEdit& edit : document.models()) {
    if (edit.model_index < rows->size()) {
      (*rows)[edit.model_index].edited = true;
      (*rows)[edit.model_index].hidden = edit.hidden;
      (*rows)[edit.model_index].translucent = edit.opacity.has_value();
    }
  }
  // UN MAILLAGE RETOUCHÉ compte aussi : celui de sa pose, ou celui de toutes
  // les poses de son fichier.
  for (const ro::mapedit::MeshEdit& mesh : document.meshes()) {
    for (ro::ui::MapEditorModelRow& row : *rows) {
      if (row.model_index == mesh.model_index || (mesh.all_poses && row.file == mesh.model_file)) {
        row.edited = true;
      }
    }
  }
  edited_ranks_.clear();
  for (const ro::ui::MapEditorModelRow& row : *rows) {
    if (row.edited) {
      edited_ranks_.push_back(row.model_index);
    }
  }
  // Les ajouts, après les modèles du RSW.
  for (std::size_t slot = 0; slot < document.added().size(); ++slot) {
    const ro::mapedit::AddedModel& added = document.added()[slot];
    rows->push_back(ro::ui::MapEditorModelRow{.model_index = model_files_.size() + slot,
                                              .file = added.model_file,
                                              .edited = true,
                                              .hidden = added.hidden,
                                              .added = true,
                                              .translucent = added.opacity.has_value()});
  }
  scene_rows_ = std::move(rows);
  rows_document_ = document;
  rows_map_ = map_name_;
  rows_model_count_ = model_files_.size();
}

ro::ui::MapEditorView MapEditorSession::View(const ro::render::MapScene& scene,
                                             const MapEditStore& store) const {
  ro::ui::MapEditorView view;
  view.active = active_;
  view.tool = tool_;
  view.map_name = map_name_;
  view.map_cubes = ro::ui::MapCubes{.width = scene.gnd.width(), .height = scene.gnd.height()};
  view.undo_label = history_.UndoLabel();
  view.redo_label = history_.RedoLabel();
  view.unsaved = store.Unsaved(map_name_) || store.Unsaved(ro::mapedit::kModelLightsDocument);
  view.status = status_;
  const ro::mapedit::MapEdit& document = store.Document(map_name_);
  RefreshSceneRows(document);
  view.models = scene_rows_;
  view.edited = edited_ranks_;
  ViewLights(document, view);
  ViewEffects(document, view);
  ViewMapEffects(scene, document, view);
  ViewSprites(document, view);
  ViewSounds(scene, document, view);
  ViewGround(document, view);
  ViewGroundPoints(scene, document, view);
  ViewModelLights(scene, store, view);
  if (selection_.empty()) {
    return view;
  }
  const std::size_t rank = *selection_.model_index;
  // L'OUTIL SOMMETS : le maillage du désigné, et son sommet.
  const ro::mapedit::MeshEdit* mesh = document.MeshAt(rank);
  view.moved_vertices = mesh != nullptr ? mesh->vertices.size() : 0;
  view.all_poses = mesh != nullptr ? mesh->all_poses : all_poses_;
  if (vertex_.has_value() && vertex_context_.has_value() &&
      vertex_->first < vertex_context_->nodes.size()) {
    view.selected_vertex =
        std::format("{} #{}", vertex_context_->nodes[vertex_->first].name, vertex_->second);
  }
  if (const std::optional<std::size_t> slot = AddedSlot(scene);
      slot.has_value() && *slot < document.added().size()) {
    const ro::mapedit::AddedModel& added = document.added()[*slot];
    view.selected = rank;
    view.selected_added = true;
    view.selected_file = added.model_file;
    view.origin = added.position;
    view.position = added.position;
    view.rotation = added.rotation;
    view.scale = added.scale;
    view.hidden = added.hidden;
    view.opacity = added.opacity.value_or(ro::mapedit::kFullOpacity);
    view.animation_type = added.animation_type;
    view.animation_speed = added.animation_speed;
    view.why = added.why;
    return view;
  }
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
  view.opacity = edit != nullptr ? edit->opacity.value_or(ro::mapedit::kFullOpacity)
                                 : ro::mapedit::kFullOpacity;
  view.animation_type = original->animation_type;
  view.animation_speed = original->animation_speed;
  if (tool_ == ro::ui::MapEditorTool::kVertices) {
    if (mesh != nullptr) {
      view.why = mesh->why;
    } else if (const auto pending = pending_mesh_why_.find(rank);
               pending != pending_mesh_why_.end()) {
      view.why = pending->second;
    }
  } else if (edit != nullptr) {
    view.why = edit->why;
  } else if (const auto pending = pending_why_.find(rank); pending != pending_why_.end()) {
    view.why = pending->second;
  }
  return view;
}

}  // namespace moonclient
