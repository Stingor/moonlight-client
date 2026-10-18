// LES EFFETS DE CARTE DE L'ÉDITEUR : voir docs/adr/0119.
//
// Un effet s'ajoute par son numéro où regarde la caméra, se règle depuis le
// panneau et se déplace à la flèche ; chaque geste compte dans l'historique,
// un glissé au relâcher seulement.

#include <algorithm>
#include <array>
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
// UN EFFET NEUF prend la cadence et les paramètres des flammes de pvp_n_5-5 :
// ce sont ceux d'un semeur ordinaire, que la plupart des effets acceptent.
constexpr float kNewEffectEmitSpeed = 125.0F;
constexpr std::array<float, 4> kNewEffectParam{0.6F, 1.0F, 0.0F, 0.0F};
// La plus petite portée que garde une lueur : le document refuse une portée
// nulle.
constexpr float kSmallestGlowRange = 0.5F;

ro::ui::MapEditorEffect ShownEffect(const ro::mapedit::AddedEffect& effect) {
  ro::ui::MapEditorEffect shown{.number = effect.number,
                                .position = effect.position,
                                .emit_speed = effect.emit_speed,
                                .param = effect.param,
                                .night_only = effect.night_only,
                                .hours = ShownHours(effect.hours),
                                .glows = effect.glow.has_value(),
                                .why = effect.why};
  if (effect.glow.has_value()) {
    shown.glow_color = effect.glow->color;
    shown.glow_range = effect.glow->range;
    shown.glow_flicker = effect.glow->flicker;
    shown.glow_shadow = effect.glow->shadow;
  }
  return shown;
}

ro::mapedit::AddedEffect DocumentEffect(const ro::ui::MapEditorEffect& shown) {
  ro::mapedit::AddedEffect effect{.number = shown.number,
                                  .position = shown.position,
                                  .emit_speed = shown.emit_speed,
                                  .param = shown.param,
                                  .night_only = shown.night_only,
                                  .hours = DocumentHours(shown.hours),
                                  .glow = std::nullopt,
                                  .why = shown.why};
  if (shown.glows) {
    effect.glow = ro::mapedit::EffectGlow{.color = shown.glow_color,
                                          .range = std::max(shown.glow_range, kSmallestGlowRange),
                                          .flicker = shown.glow_flicker,
                                          .shadow = shown.glow_shadow};
  }
  return effect;
}

}  // namespace

std::vector<ro::ui::MapEditorLine> MapEditorSession::EffectTrace() const {
  std::vector<ro::ui::MapEditorLine> marks;
  marks.reserve(effect_dots_.size());
  for (const LightDot& dot : effect_dots_) {
    marks.push_back(ro::ui::MapEditorLine{.from = dot.at,
                                          .to = dot.at,
                                          .kind = selected_effect_ == dot.slot
                                                      ? ro::ui::MapEditorLineKind::kEffectSelected
                                                      : ro::ui::MapEditorLineKind::kEffect});
  }
  return marks;
}

bool MapEditorSession::GrabEffect(glm::vec2 pointer) {
  float nearest = kGizmoGrabPixels;
  std::optional<std::size_t> picked;
  for (const LightDot& dot : effect_dots_) {
    const float distance = glm::length(dot.at - pointer);
    if (distance <= nearest) {
      nearest = distance;
      picked = dot.slot;
    }
  }
  if (!picked.has_value()) {
    return false;
  }
  ChooseEffect(picked);
  return true;
}

bool MapEditorSession::GrabEffectHandle(glm::vec2 pointer, const ro::ui::MapEditorView& view) {
  if (!view.selected_effect.has_value() || *view.selected_effect >= view.effects.size()) {
    return false;
  }
  const std::optional<std::size_t> grabbed = GrabbedHandle(handles_, pointer);
  if (!grabbed.has_value()) {
    return false;
  }
  const ro::ui::map_editor::Place start{.position = view.effects[*view.selected_effect].position,
                                        .released = false};
  drag_ = Drag{.handle = *grabbed,
               .grabbed = handles_[*grabbed],
               .frame = gizmo_frame_,
               .from = pointer,
               .start = start,
               .last = start,
               .effect = view.selected_effect};
  return true;
}

void MapEditorSession::ChooseEffect(std::optional<std::size_t> slot) {
  selected_effect_ = slot;
  if (slot.has_value()) {
    selection_.Clear();
    vertex_.reset();
    selected_light_.reset();
    selected_map_effect_.reset();
    selected_sprite_.reset();
    selected_sound_.reset();
  }
}

void MapEditorSession::ViewEffects(const ro::mapedit::MapEdit& document,
                                   ro::ui::MapEditorView& view) const {
  view.effects.reserve(document.effects().size());
  for (const ro::mapedit::AddedEffect& effect : document.effects()) {
    view.effects.push_back(ShownEffect(effect));
  }
  if (selected_effect_.has_value() && *selected_effect_ < view.effects.size()) {
    view.selected_effect = selected_effect_;
  }
}

MapEditorSession::Outcome MapEditorSession::AddEffect(std::int32_t number, MapEditStore& store) {
  const ro::mapedit::MapEdit before = store.Document(map_name_);
  ro::mapedit::MapEdit after = before;
  after.AddEffect(ro::mapedit::AddedEffect{.number = number,
                                           .position = drop_point_,
                                           .emit_speed = kNewEffectEmitSpeed,
                                           .param = kNewEffectParam,
                                           .night_only = false,
                                           .glow = std::nullopt,
                                           .why = {}});
  store.Set(map_name_, WithReasons(after));
  ChooseEffect(after.effects().size() - 1);
  Record(ro::core::Tr("Ajouter un effet"), before, after);
  return {};
}

MapEditorSession::Outcome MapEditorSession::SetEffect(const ro::ui::map_editor::SetEffect& set,
                                                      MapEditStore& store) {
  const ro::mapedit::MapEdit before = store.Document(map_name_);
  if (set.slot >= before.effects().size()) {
    return {};
  }
  if (!gesture_before_.has_value()) {
    gesture_before_ = before;
  }
  ro::mapedit::MapEdit after = before;
  after.SetEffect(set.slot, DocumentEffect(set.effect));
  if (!(after == before)) {
    store.Set(map_name_, WithReasons(after));
  }
  if (set.released) {
    std::array<char, kLabelCapacity> label{};
    std::snprintf(label.data(), label.size(), ro::core::Tr("Régler l'effet %zu"), set.slot);
    Record(label.data(), *gesture_before_, store.Document(map_name_));
    gesture_before_.reset();
  }
  return {};
}

MapEditorSession::Outcome MapEditorSession::MoveEffect(const ro::ui::map_editor::MoveEffect& move,
                                                       MapEditStore& store) {
  const std::vector<ro::mapedit::AddedEffect>& effects = store.Document(map_name_).effects();
  if (move.slot >= effects.size()) {
    return {};
  }
  ro::ui::map_editor::SetEffect set{
      .slot = move.slot, .effect = ShownEffect(effects[move.slot]), .released = move.released};
  set.effect.position = move.native;
  return SetEffect(set, store);
}

MapEditorSession::Outcome MapEditorSession::RemoveEffect(std::size_t slot, MapEditStore& store) {
  const ro::mapedit::MapEdit before = store.Document(map_name_);
  if (slot >= before.effects().size()) {
    return {};
  }
  ro::mapedit::MapEdit after = before;
  after.RemoveEffect(slot);
  store.Set(map_name_, WithReasons(after));
  selected_effect_.reset();
  std::array<char, kLabelCapacity> label{};
  std::snprintf(label.data(), label.size(), ro::core::Tr("Retirer l'effet %zu"), slot);
  Record(label.data(), before, after);
  return {};
}

}  // namespace moonclient
