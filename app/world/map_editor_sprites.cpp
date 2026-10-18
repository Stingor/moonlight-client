// LES SPRITES DE DÉCOR DE L'ÉDITEUR DE CARTE : voir ro/mapedit/map_sprite.h.
//
// Un sprite s'ajoute par son nom ou son numéro où regarde la caméra, se règle
// depuis le panneau et se déplace à la flèche ; chaque geste compte dans
// l'historique, un glissé au relâcher seulement.

#include <algorithm>
#include <array>
#include <cstdio>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ro/core/i18n.h"
#include "ro/mapedit/map_sprite.h"

#include "world/map_editor_reasons.h"
#include "world/map_editor_session.h"

namespace moonclient {
namespace {

// La place d'un nom de geste, zéro final compris.
constexpr std::size_t kLabelCapacity = 192;
// La plus petite cadence et la plus petite échelle que garde un réglage : le
// document refuse zéro.
constexpr float kSmallestFrameSpeed = 0.05F;
constexpr float kSmallestScale = 0.05F;

ro::ui::MapEditorSprite ShownSprite(const ro::mapedit::MapSprite& sprite) {
  return ro::ui::MapEditorSprite{.sprite = sprite.sprite,
                                 .position = sprite.position,
                                 .action = sprite.action,
                                 .frozen = sprite.held_frame.has_value(),
                                 .held_frame = sprite.held_frame.value_or(0),
                                 .frame_speed = sprite.frame_speed,
                                 .direction = sprite.direction,
                                 .turn_degrees_per_second = sprite.turn_degrees_per_second,
                                 .roll_degrees = sprite.roll_degrees,
                                 .roll_degrees_per_second = sprite.roll_degrees_per_second,
                                 .scale = sprite.scale,
                                 .opacity = sprite.opacity.value_or(ro::mapedit::kFullOpacity),
                                 .hidden = sprite.hidden,
                                 .why = sprite.why};
}

// Le sprite que le panneau rend, ramené dans les bornes du document : la
// relecture refuserait sinon tout le document.
ro::mapedit::MapSprite DocumentSprite(const ro::ui::MapEditorSprite& shown) {
  return ro::mapedit::MapSprite{
      .sprite = shown.sprite,
      .position = shown.position,
      .action = shown.action,
      .held_frame = shown.frozen ? std::optional<std::uint32_t>(shown.held_frame) : std::nullopt,
      .frame_speed =
          std::clamp(shown.frame_speed, kSmallestFrameSpeed, ro::mapedit::kLargestSpriteFrameSpeed),
      .direction = shown.direction % ro::mapedit::kSpriteDirectionCount,
      .turn_degrees_per_second = shown.turn_degrees_per_second,
      .roll_degrees = shown.roll_degrees,
      .roll_degrees_per_second = shown.roll_degrees_per_second,
      .scale = std::clamp(shown.scale, kSmallestScale, ro::mapedit::kLargestSpriteScale),
      .opacity = ro::mapedit::DocumentOpacity(shown.opacity),
      .hidden = shown.hidden,
      .why = shown.why};
}

}  // namespace

std::vector<ro::ui::MapEditorLine> MapEditorSession::SpriteTrace() const {
  std::vector<ro::ui::MapEditorLine> marks;
  marks.reserve(sprite_dots_.size());
  for (const LightDot& dot : sprite_dots_) {
    marks.push_back(ro::ui::MapEditorLine{.from = dot.at,
                                          .to = dot.at,
                                          .kind = selected_sprite_ == dot.slot
                                                      ? ro::ui::MapEditorLineKind::kSpriteSelected
                                                      : ro::ui::MapEditorLineKind::kSprite});
  }
  return marks;
}

bool MapEditorSession::GrabSprite(glm::vec2 pointer) {
  float nearest = kGizmoGrabPixels;
  std::optional<std::size_t> picked;
  for (const LightDot& dot : sprite_dots_) {
    const float distance = glm::length(dot.at - pointer);
    if (distance <= nearest) {
      nearest = distance;
      picked = dot.slot;
    }
  }
  if (!picked.has_value()) {
    return false;
  }
  ChooseSprite(picked);
  return true;
}

bool MapEditorSession::GrabSpriteHandle(glm::vec2 pointer, const ro::ui::MapEditorView& view) {
  if (!view.selected_sprite.has_value() || *view.selected_sprite >= view.sprites.size()) {
    return false;
  }
  const std::optional<std::size_t> grabbed = GrabbedHandle(handles_, pointer);
  if (!grabbed.has_value()) {
    return false;
  }
  const ro::ui::map_editor::Place start{.position = view.sprites[*view.selected_sprite].position,
                                        .released = false};
  drag_ = Drag{.handle = *grabbed,
               .grabbed = handles_[*grabbed],
               .frame = gizmo_frame_,
               .from = pointer,
               .start = start,
               .last = start,
               .sprite = view.selected_sprite};
  return true;
}

void MapEditorSession::ChooseSprite(std::optional<std::size_t> slot) {
  selected_sprite_ = slot;
  if (slot.has_value()) {
    selection_.Clear();
    vertex_.reset();
    selected_light_.reset();
    selected_effect_.reset();
    selected_map_effect_.reset();
    selected_sound_.reset();
  }
}

void MapEditorSession::ViewSprites(const ro::mapedit::MapEdit& document,
                                   ro::ui::MapEditorView& view) const {
  view.sprites.reserve(document.sprites().size());
  for (const ro::mapedit::MapSprite& sprite : document.sprites()) {
    view.sprites.push_back(ShownSprite(sprite));
  }
  if (selected_sprite_.has_value() && *selected_sprite_ < view.sprites.size()) {
    view.selected_sprite = selected_sprite_;
  }
}

MapEditorSession::Outcome MapEditorSession::AddSprite(std::string sprite, MapEditStore& store) {
  if (sprite.empty()) {
    return {};
  }
  const ro::mapedit::MapEdit before = store.Document(map_name_);
  ro::mapedit::MapEdit after = before;
  after.AddSprite(ro::mapedit::MapSprite{.sprite = std::move(sprite), .position = drop_point_});
  store.Set(map_name_, WithReasons(after));
  ChooseSprite(after.sprites().size() - 1);
  Record(ro::core::Tr("Ajouter un sprite"), before, store.Document(map_name_));
  return {};
}

MapEditorSession::Outcome MapEditorSession::SetSprite(const ro::ui::map_editor::SetSprite& set,
                                                      MapEditStore& store) {
  const ro::mapedit::MapEdit before = store.Document(map_name_);
  if (set.slot >= before.sprites().size()) {
    return {};
  }
  if (!gesture_before_.has_value()) {
    gesture_before_ = before;
  }
  ro::mapedit::MapEdit after = before;
  ro::mapedit::MapSprite sprite = DocumentSprite(set.sprite);
  // UN NOM VIDÉ garde l'ancien : le document refuse un sprite sans nom.
  if (sprite.sprite.empty()) {
    sprite.sprite = before.sprites()[set.slot].sprite;
  }
  after.SetSprite(set.slot, std::move(sprite));
  if (!(after == before)) {
    store.Set(map_name_, WithReasons(after));
  }
  if (set.released) {
    std::array<char, kLabelCapacity> label{};
    std::snprintf(label.data(), label.size(), ro::core::Tr("Régler le sprite %zu"), set.slot);
    Record(label.data(), *gesture_before_, store.Document(map_name_));
    gesture_before_.reset();
  }
  return {};
}

MapEditorSession::Outcome MapEditorSession::MoveSprite(const ro::ui::map_editor::MoveSprite& move,
                                                       MapEditStore& store) {
  const std::vector<ro::mapedit::MapSprite>& sprites = store.Document(map_name_).sprites();
  if (move.slot >= sprites.size()) {
    return {};
  }
  ro::ui::map_editor::SetSprite set{
      .slot = move.slot, .sprite = ShownSprite(sprites[move.slot]), .released = move.released};
  set.sprite.position = move.native;
  return SetSprite(set, store);
}

MapEditorSession::Outcome MapEditorSession::RemoveSprite(std::size_t slot, MapEditStore& store) {
  const ro::mapedit::MapEdit before = store.Document(map_name_);
  if (slot >= before.sprites().size()) {
    return {};
  }
  ro::mapedit::MapEdit after = before;
  after.RemoveSprite(slot);
  store.Set(map_name_, WithReasons(after));
  selected_sprite_.reset();
  std::array<char, kLabelCapacity> label{};
  std::snprintf(label.data(), label.size(), ro::core::Tr("Retirer le sprite %zu"), slot);
  Record(label.data(), before, store.Document(map_name_));
  return {};
}

MapEditorSession::Outcome MapEditorSession::NudgeSprite(const std::array<float, 3>& delta,
                                                        MapEditStore& store) {
  const std::vector<ro::mapedit::MapSprite>& sprites = store.Document(map_name_).sprites();
  if (!selected_sprite_.has_value() || *selected_sprite_ >= sprites.size()) {
    return {};
  }
  std::array<float, 3> position = sprites[*selected_sprite_].position;
  for (std::size_t axis = 0; axis < position.size(); ++axis) {
    position[axis] += delta[axis];
  }
  return MoveSprite(
      ro::ui::map_editor::MoveSprite{
          .slot = *selected_sprite_, .native = position, .released = true},
      store);
}

MapEditorSession::Outcome MapEditorSession::DuplicateSprite(MapEditStore& store) {
  const ro::mapedit::MapEdit before = store.Document(map_name_);
  if (!selected_sprite_.has_value() || *selected_sprite_ >= before.sprites().size()) {
    return {};
  }
  ro::mapedit::MapSprite copy = before.sprites()[*selected_sprite_];
  ro::mapedit::MapEdit after = before;
  after.AddSprite(std::move(copy));
  store.Set(map_name_, WithReasons(after));
  ChooseSprite(after.sprites().size() - 1);
  Record(ro::core::Tr("Ajouter un sprite"), before, store.Document(map_name_));
  return {};
}

}  // namespace moonclient
