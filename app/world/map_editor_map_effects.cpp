// LES EFFETS DE LA CARTE DANS L'ÉDITEUR : les objets d'effet du RSW, listés et
// retouchés — déplacés, masqués, réservés à la nuit, dotés d'une lueur. Voir
// docs/adr/0119.
//
// Une retouche désigne son objet par son rang, son numéro et sa position
// d'origine ; elle disparaît quand elle ne change plus rien et n'a pas de
// raison.

#include <array>
#include <cmath>
#include <cstdio>
#include <optional>
#include <utility>
#include <vector>

#include "ro/core/i18n.h"

#include "world/map_editor_reasons.h"
#include "world/map_editor_session.h"

namespace moonclient {
namespace {

// La place d'un nom de geste, zéro final compris.
constexpr std::size_t kLabelCapacity = 192;
// Une copie se pose à côté de l'original : deux carreaux du sol.
constexpr float kDuplicateShift = 20.0F;
// En deçà, une position est tenue pour celle d'origine.
constexpr float kBackHome = ro::mapedit::kOriginTolerance;

bool SamePlace(const std::array<float, 3>& left, const std::array<float, 3>& right) {
  for (std::size_t axis = 0; axis < left.size(); ++axis) {
    if (std::abs(left[axis] - right[axis]) > kBackHome) {
      return false;
    }
  }
  return true;
}

}  // namespace

void MapEditorSession::ViewMapEffects(const ro::render::MapScene& scene,
                                      const ro::mapedit::MapEdit& document,
                                      ro::ui::MapEditorView& view) const {
  const std::vector<ro::formats::RswEffect>& objects = scene.rsw.effects();
  view.map_effects.reserve(objects.size());
  for (std::size_t index = 0; index < objects.size(); ++index) {
    ro::ui::MapEditorMapEffect row{.effect_index = index,
                                   .number = objects[index].number,
                                   .origin = objects[index].position,
                                   .position = objects[index].position};
    const ro::mapedit::EffectEdit* retouch = document.EffectEditAt(index);
    if (retouch != nullptr && ro::mapedit::EffectEditTargets(*retouch, objects[index])) {
      row.edited = true;
      row.position = retouch->position.value_or(row.origin);
      row.hidden = retouch->hidden;
      row.night_only = retouch->night_only;
      row.hours = ShownHours(retouch->hours);
      row.glows = retouch->glow.has_value();
      if (retouch->glow.has_value()) {
        row.glow_color = retouch->glow->color;
        row.glow_range = retouch->glow->range;
        row.glow_flicker = retouch->glow->flicker;
        row.glow_shadow = retouch->glow->shadow;
      }
      row.why = retouch->why;
    }
    view.map_effects.push_back(std::move(row));
  }
  if (selected_map_effect_.has_value() && *selected_map_effect_ < view.map_effects.size()) {
    view.selected_map_effect = selected_map_effect_;
  }
}

std::vector<ro::ui::MapEditorLine> MapEditorSession::MapEffectTrace() const {
  std::vector<ro::ui::MapEditorLine> marks;
  marks.reserve(map_effect_dots_.size());
  for (const LightDot& dot : map_effect_dots_) {
    marks.push_back(ro::ui::MapEditorLine{.from = dot.at,
                                          .to = dot.at,
                                          .kind = selected_map_effect_ == dot.slot
                                                      ? ro::ui::MapEditorLineKind::kEffectSelected
                                                      : ro::ui::MapEditorLineKind::kEffect});
  }
  return marks;
}

bool MapEditorSession::GrabMapEffect(glm::vec2 pointer) {
  float nearest = kGizmoGrabPixels;
  std::optional<std::size_t> picked;
  for (const LightDot& dot : map_effect_dots_) {
    const float distance = glm::length(dot.at - pointer);
    if (distance <= nearest) {
      nearest = distance;
      picked = dot.slot;
    }
  }
  if (!picked.has_value()) {
    return false;
  }
  ChooseMapEffect(picked);
  return true;
}

bool MapEditorSession::GrabMapEffectHandle(glm::vec2 pointer, const ro::ui::MapEditorView& view) {
  if (!view.selected_map_effect.has_value() ||
      *view.selected_map_effect >= view.map_effects.size()) {
    return false;
  }
  const std::optional<std::size_t> grabbed = GrabbedHandle(handles_, pointer);
  if (!grabbed.has_value()) {
    return false;
  }
  const ro::ui::map_editor::Place start{
      .position = view.map_effects[*view.selected_map_effect].position, .released = false};
  drag_ = Drag{.handle = *grabbed,
               .grabbed = handles_[*grabbed],
               .frame = gizmo_frame_,
               .from = pointer,
               .start = start,
               .last = start,
               .map_effect = view.selected_map_effect};
  return true;
}

void MapEditorSession::ChooseMapEffect(std::optional<std::size_t> index) {
  selected_map_effect_ = index;
  if (index.has_value()) {
    selection_.Clear();
    vertex_.reset();
    selected_light_.reset();
    selected_effect_.reset();
    selected_sprite_.reset();
  }
}

MapEditorSession::Outcome MapEditorSession::SetMapEffect(
    const ro::ui::map_editor::SetMapEffect& set, const ro::render::MapScene& scene,
    MapEditStore& store) {
  const std::vector<ro::formats::RswEffect>& objects = scene.rsw.effects();
  if (set.index >= objects.size()) {
    return {};
  }
  const ro::formats::RswEffect& object = objects[set.index];
  const ro::mapedit::MapEdit before = store.Document(map_name_);
  if (!gesture_before_.has_value()) {
    gesture_before_ = before;
  }
  ro::mapedit::EffectEdit retouch{.effect_index = set.index,
                                  .number = object.number,
                                  .origin = object.position,
                                  .position = std::nullopt,
                                  .hidden = set.effect.hidden,
                                  .night_only = set.effect.night_only,
                                  .hours = DocumentHours(set.effect.hours),
                                  .glow = std::nullopt,
                                  .why = set.effect.why};
  if (!SamePlace(set.effect.position, object.position)) {
    retouch.position = set.effect.position;
  }
  if (set.effect.glows) {
    retouch.glow = ro::mapedit::EffectGlow{.color = set.effect.glow_color,
                                           .range = std::max(set.effect.glow_range, kBackHome),
                                           .flicker = set.effect.glow_flicker,
                                           .shadow = set.effect.glow_shadow};
  }
  ro::mapedit::MapEdit after = before;
  // Une raison seule ne garde pas la retouche : elle reçoit d'office une raison
  // générique, voir `WithReasons`.
  const bool changes_nothing = !retouch.position.has_value() && !retouch.hidden &&
                               !retouch.night_only && !retouch.glow.has_value();
  if (changes_nothing) {
    after.RemoveEffectEdit(set.index);
  } else {
    after.SetEffectEdit(std::move(retouch));
  }
  if (!(after == before)) {
    store.Set(map_name_, WithReasons(after));
  }
  if (set.released) {
    std::array<char, kLabelCapacity> label{};
    std::snprintf(label.data(), label.size(), ro::core::Tr("Retoucher l'effet %zu"), set.index);
    Record(label.data(), *gesture_before_, store.Document(map_name_));
    gesture_before_.reset();
  }
  return {};
}

MapEditorSession::Outcome MapEditorSession::RevertMapEffect(std::size_t index,
                                                            MapEditStore& store) {
  const ro::mapedit::MapEdit before = store.Document(map_name_);
  if (before.EffectEditAt(index) == nullptr) {
    return {};
  }
  ro::mapedit::MapEdit after = before;
  after.RemoveEffectEdit(index);
  store.Set(map_name_, WithReasons(after));
  std::array<char, kLabelCapacity> label{};
  std::snprintf(label.data(), label.size(), ro::core::Tr("Rétablir l'effet %zu"), index);
  Record(label.data(), before, after);
  return {};
}

MapEditorSession::Outcome MapEditorSession::MoveMapEffect(
    const ro::ui::map_editor::MoveMapEffect& move, const ro::render::MapScene& scene,
    MapEditStore& store) {
  ro::ui::MapEditorView view;
  ViewMapEffects(scene, store.Document(map_name_), view);
  if (move.index >= view.map_effects.size()) {
    return {};
  }
  ro::ui::map_editor::SetMapEffect set{
      .index = move.index, .effect = view.map_effects[move.index], .released = move.released};
  set.effect.position = move.native;
  return SetMapEffect(set, scene, store);
}

MapEditorSession::Outcome MapEditorSession::NudgeMapEffect(const std::array<float, 3>& delta,
                                                           const ro::render::MapScene& scene,
                                                           MapEditStore& store) {
  ro::ui::MapEditorView view;
  ViewMapEffects(scene, store.Document(map_name_), view);
  if (!selected_map_effect_.has_value() || *selected_map_effect_ >= view.map_effects.size()) {
    return {};
  }
  std::array<float, 3> position = view.map_effects[*selected_map_effect_].position;
  for (std::size_t axis = 0; axis < position.size(); ++axis) {
    position[axis] += delta[axis];
  }
  return MoveMapEffect(
      ro::ui::map_editor::MoveMapEffect{
          .index = *selected_map_effect_, .native = position, .released = true},
      scene, store);
}

MapEditorSession::Outcome MapEditorSession::HideMapEffect(const ro::render::MapScene& scene,
                                                          MapEditStore& store) {
  ro::ui::MapEditorView view;
  ViewMapEffects(scene, store.Document(map_name_), view);
  if (!selected_map_effect_.has_value() || *selected_map_effect_ >= view.map_effects.size()) {
    return {};
  }
  ro::ui::map_editor::SetMapEffect set{.index = *selected_map_effect_,
                                       .effect = view.map_effects[*selected_map_effect_],
                                       .released = true};
  set.effect.hidden = !set.effect.hidden;
  return SetMapEffect(set, scene, store);
}

MapEditorSession::Outcome MapEditorSession::DuplicateMapEffect(const ro::render::MapScene& scene,
                                                               MapEditStore& store) {
  const std::vector<ro::formats::RswEffect>& objects = scene.rsw.effects();
  if (!selected_map_effect_.has_value() || *selected_map_effect_ >= objects.size()) {
    return {};
  }
  ro::ui::MapEditorView view;
  ViewMapEffects(scene, store.Document(map_name_), view);
  const ro::formats::RswEffect& object = objects[*selected_map_effect_];
  const ro::ui::MapEditorMapEffect& shown = view.map_effects[*selected_map_effect_];
  const ro::mapedit::MapEdit before = store.Document(map_name_);
  ro::mapedit::MapEdit after = before;
  ro::mapedit::AddedEffect copy{.number = object.number,
                                .position = shown.position,
                                .emit_speed = object.emit_speed,
                                .param = object.param,
                                .night_only = shown.night_only,
                                .hours = DocumentHours(shown.hours),
                                .glow = std::nullopt,
                                .why = {}};
  copy.position[0] += kDuplicateShift;
  if (shown.glows) {
    copy.glow = ro::mapedit::EffectGlow{.color = shown.glow_color,
                                        .range = shown.glow_range,
                                        .flicker = shown.glow_flicker,
                                        .shadow = shown.glow_shadow};
  }
  after.AddEffect(std::move(copy));
  store.Set(map_name_, WithReasons(after));
  ChooseEffect(after.effects().size() - 1);
  Record(ro::core::Tr("Ajouter un effet"), before, after);
  return {};
}

}  // namespace moonclient
