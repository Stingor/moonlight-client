// LES GESTES DU CLAVIER SUR CE QUI EST DÉSIGNÉ : pousser, dupliquer, supprimer.
// Voir `MapEditorShortcut` et `TakeMapEditorKey`.

#include <array>
#include <cstdio>
#include <utility>
#include <vector>

#include "ro/core/i18n.h"
#include "ro/vfs/encoding.h"

#include "world/map_editor_reasons.h"
#include "world/map_editor_session.h"

namespace moonclient {
namespace {

// UNE COPIE se pose à côté de l'original, sur l'axe x du RSW, pour se voir
// tout de suite : deux carreaux du sol.
constexpr float kDuplicateShift = 20.0F;
// La place du nom de ce geste, zéro final compris.
constexpr std::size_t kDuplicateLabelCapacity = 192;

}  // namespace

MapEditorSession::Outcome MapEditorSession::Nudge(const std::array<float, 3>& delta,
                                                  const ro::render::MapScene& scene,
                                                  MapEditStore& store) {
  const auto pushed = [&delta](std::array<float, 3> position) {
    for (std::size_t axis = 0; axis < position.size(); ++axis) {
      position[axis] += delta[axis];
    }
    return position;
  };
  // EN MODE SOMMETS DU RELIEF, haut et bas lèvent ou abaissent les points
  // sélectionnés ; gauche et droite ne font rien.
  if (ground_points_mode() && !selected_points_.empty()) {
    return NudgeGroundPoints(delta[2], store);
  }
  if (selected_sprite_.has_value()) {
    return NudgeSprite(delta, store);
  }
  if (selected_map_effect_.has_value()) {
    return NudgeMapEffect(delta, scene, store);
  }
  if (selected_effect_.has_value()) {
    return NudgeEffect(delta, store);
  }
  if (selected_light_.has_value()) {
    const std::vector<ro::mapedit::AddedLight>& lights = store.Document(map_name_).lights();
    if (*selected_light_ >= lights.size()) {
      return {};
    }
    return MoveLight(
        ro::ui::map_editor::MoveLight{.slot = *selected_light_,
                                      .native = pushed(lights[*selected_light_].position),
                                      .released = true},
        store);
  }
  if (selection_.empty() || *selection_.model_index >= scene.rsw.models().size()) {
    return {};
  }
  const std::size_t rank = *selection_.model_index;
  const ro::formats::RswModel& placed = scene.rsw.models()[rank];
  const ro::mapedit::MapEdit& document = store.Document(map_name_);
  bool hidden = false;
  if (rank >= original_count_) {
    const std::size_t slot = rank - original_count_;
    hidden = slot < document.added().size() && document.added()[slot].hidden;
  } else if (const ro::mapedit::ModelEdit* edit = document.ModelAt(rank)) {
    hidden = edit->hidden;
  }
  return Place(ro::ui::map_editor::Place{.position = pushed(placed.position),
                                         .rotation = placed.rotation,
                                         .scale = placed.scale,
                                         .hidden = hidden,
                                         .released = true},
               scene, store);
}

MapEditorSession::Outcome MapEditorSession::Duplicate(const ro::render::MapScene& scene,
                                                      MapEditStore& store) {
  if (selected_sprite_.has_value()) {
    return DuplicateSprite(store);
  }
  if (selected_map_effect_.has_value()) {
    return DuplicateMapEffect(scene, store);
  }
  if (selected_effect_.has_value()) {
    return DuplicateEffect(store);
  }
  // UNE LUMIÈRE DÉSIGNÉE se duplique comme un modèle : à côté d'elle.
  if (selected_light_.has_value()) {
    const ro::mapedit::MapEdit before = store.Document(map_name_);
    if (*selected_light_ >= before.lights().size()) {
      return {};
    }
    ro::mapedit::AddedLight copy = before.lights()[*selected_light_];
    copy.position[0] += kDuplicateShift;
    ro::mapedit::MapEdit after = before;
    after.AddLight(std::move(copy));
    store.Set(map_name_, WithReasons(after));
    ChooseLight(after.lights().size() - 1);
    Record(ro::core::Tr("Ajouter une lumière"), before, after);
    return {};
  }
  if (selection_.empty() || *selection_.model_index >= scene.rsw.models().size()) {
    return {};
  }
  const std::size_t rank = *selection_.model_index;
  const ro::formats::RswModel& placed = scene.rsw.models()[rank];
  const ro::mapedit::MapEdit before = store.Document(map_name_);
  // LA COPIE GARDE L'OPACITÉ du modèle, comme son placement.
  std::optional<float> opacity;
  if (rank >= original_count_) {
    const std::size_t slot = rank - original_count_;
    if (slot < before.added().size()) {
      opacity = before.added()[slot].opacity;
    }
  } else if (const ro::mapedit::ModelEdit* edit = before.ModelAt(rank)) {
    opacity = edit->opacity;
  }
  ro::mapedit::MapEdit after = before;
  after.AddModel(ro::mapedit::AddedModel{
      .model_file = ro::vfs::Cp949ToUtf8Lossy(placed.model_name),
      .position = {placed.position[0] + kDuplicateShift, placed.position[1], placed.position[2]},
      .rotation = placed.rotation,
      .scale = placed.scale,
      .opacity = opacity});
  store.Set(map_name_, WithReasons(after));
  std::array<char, kDuplicateLabelCapacity> label{};
  std::snprintf(label.data(), label.size(), ro::core::Tr("Dupliquer le modèle %zu"), rank);
  Record(label.data(), before, after);
  selection_.model_index = scene.unfixed_models.size() + after.added().size() - 1;
  status_ = ro::core::Tr("Modèle ajouté : dites pourquoi avant d'enregistrer.");
  return {.replace_models = true};
}

MapEditorSession::Outcome MapEditorSession::Delete(const ro::render::MapScene& scene,
                                                   MapEditStore& store) {
  if (selected_sprite_.has_value()) {
    return RemoveSprite(*selected_sprite_, store);
  }
  if (selected_map_effect_.has_value()) {
    return HideMapEffect(scene, store);
  }
  if (selected_effect_.has_value()) {
    return RemoveEffect(*selected_effect_, store);
  }
  if (selected_light_.has_value()) {
    return RemoveLight(*selected_light_, store);
  }
  if (selection_.empty()) {
    return {};
  }
  const std::size_t rank = *selection_.model_index;
  if (rank >= original_count_) {
    return Revert(store);
  }
  // UN MODÈLE DE LA CARTE ne se retire pas : il se masque, ou se démasque, là
  // où l'édition le pose.
  if (rank >= scene.rsw.models().size()) {
    return {};
  }
  const ro::formats::RswModel& placed = scene.rsw.models()[rank];
  const ro::mapedit::ModelEdit* edit = store.Document(map_name_).ModelAt(rank);
  return Place(ro::ui::map_editor::Place{.position = placed.position,
                                         .rotation = placed.rotation,
                                         .scale = placed.scale,
                                         .hidden = edit == nullptr || !edit->hidden,
                                         .released = true},
               scene, store);
}

MapEditorSession::Outcome MapEditorSession::NudgeEffect(const std::array<float, 3>& delta,
                                                        MapEditStore& store) {
  const std::vector<ro::mapedit::AddedEffect>& effects = store.Document(map_name_).effects();
  if (!selected_effect_.has_value() || *selected_effect_ >= effects.size()) {
    return {};
  }
  std::array<float, 3> position = effects[*selected_effect_].position;
  for (std::size_t axis = 0; axis < position.size(); ++axis) {
    position[axis] += delta[axis];
  }
  return MoveEffect(
      ro::ui::map_editor::MoveEffect{
          .slot = *selected_effect_, .native = position, .released = true},
      store);
}

MapEditorSession::Outcome MapEditorSession::DuplicateEffect(MapEditStore& store) {
  const ro::mapedit::MapEdit before = store.Document(map_name_);
  if (!selected_effect_.has_value() || *selected_effect_ >= before.effects().size()) {
    return {};
  }
  ro::mapedit::AddedEffect copy = before.effects()[*selected_effect_];
  copy.position[0] += kDuplicateShift;
  ro::mapedit::MapEdit after = before;
  after.AddEffect(std::move(copy));
  store.Set(map_name_, WithReasons(after));
  ChooseEffect(after.effects().size() - 1);
  Record(ro::core::Tr("Ajouter un effet"), before, after);
  return {};
}

MapEditorSession::Outcome MapEditorSession::QuickModel(QuickModelAction action,
                                                       std::size_t model_index,
                                                       const std::array<float, 3>& here,
                                                       const ro::render::MapScene& scene,
                                                       MapEditStore& store) {
  if (model_index >= scene.rsw.models().size()) {
    return {};
  }
  ChooseModel(model_index);
  switch (action) {
    case QuickModelAction::kOpenEditor:
      active_ = true;
      return {};
    case QuickModelAction::kToggleHidden:
      return Delete(scene, store);
    case QuickModelAction::kMoveHere: {
      const std::array<float, 3>& placed = scene.rsw.models()[model_index].position;
      return Nudge({here[0] - placed[0], here[1] - placed[1], here[2] - placed[2]}, scene, store);
    }
    case QuickModelAction::kRevert:
      return Revert(store);
  }
  return {};
}

}  // namespace moonclient
