#include "character/shortcut_wheel.h"

#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_mouse.h>

namespace moonclient {

ro::ui::ShortcutWheelView ShortcutWheel::Compose(const ro::ui::ShortcutBarsLayout& layout) {
  bar_ = ro::ui::WheelBarOf(layout);
  if (!bar_.has_value() && open_) {
    Close(/*use=*/false);
  }
  return ro::ui::ShortcutWheelView{.open = open_,
                                   .bar = bar_.value_or(0),
                                   .center = ImVec2(center_.x, center_.y),
                                   .pointer = ImVec2(pointer_.x, pointer_.y)};
}

void ShortcutWheel::Open(glm::vec2 at, Opener opener, std::uint32_t key) {
  open_ = true;
  opener_ = opener;
  opener_key_ = key;
  center_ = at;
  pointer_ = at;
}

void ShortcutWheel::Close(bool use) {
  open_ = false;
  if (!use || !bar_.has_value()) {
    return;
  }
  const std::optional<std::size_t> rank = ro::ui::WheelSectorAt(
      pointer_.x - center_.x, pointer_.y - center_.y, ro::ui::kWheelDeadZoneRadiusPx);
  if (rank.has_value()) {
    chosen_ = ro::ui::ShortcutSlotIndex(*bar_, *rank);
  }
}

bool ShortcutWheel::TakeEvent(const SDL_Event& event, const Hotkeys& bindings, WheelGate gate,
                              SDL_Keymod held, glm::vec2* mouse) {
  if (!open_) {
    if (event.type != SDL_EVENT_MOUSE_BUTTON_DOWN || !bar_.has_value() || gate.capturing_hotkey ||
        gate.mouse_over_interface ||
        PressGestureOf(event.button.button) != MouseGesture::kMiddleHold) {
      return false;
    }
    const std::optional<ActionRef> action =
        bindings.ActionForGesture(MouseGesture::kMiddleHold, (held & SDL_KMOD_CTRL) != 0,
                                  (held & SDL_KMOD_SHIFT) != 0, (held & SDL_KMOD_ALT) != 0);
    if (action != ActionRef(GameAction::kShortcutWheel)) {
      return false;
    }
    Open(glm::vec2{event.button.x, event.button.y}, Opener::kMiddleButton, 0);
    return true;
  }
  switch (event.type) {
    case SDL_EVENT_MOUSE_MOTION:
      pointer_ = glm::vec2{event.motion.x, event.motion.y};
      if (mouse != nullptr) {
        *mouse = pointer_;
      }
      return true;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_WHEEL:
      return true;
    case SDL_EVENT_MOUSE_BUTTON_UP:
      // ⚠ LES AUTRES RELÂCHERS PASSENT : celui du bouton gauche arrête la marche
      // tenue, et l'avaler la laisserait se redemander sans fin.
      if (opener_ != Opener::kMiddleButton || event.button.button != SDL_BUTTON_MIDDLE) {
        return false;
      }
      pointer_ = glm::vec2{event.button.x, event.button.y};
      Close(/*use=*/true);
      return true;
    case SDL_EVENT_KEY_UP:
      if (opener_ != Opener::kKey || event.key.key != opener_key_) {
        return false;
      }
      Close(/*use=*/true);
      return true;
    case SDL_EVENT_KEY_DOWN:
      if (event.key.key == SDLK_ESCAPE) {
        Close(/*use=*/false);
        return true;
      }
      // L'auto-répétition de la touche tenue n'est que le geste qui dure.
      return opener_ == Opener::kKey && event.key.key == opener_key_;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
      // Le relâcher ne viendra jamais : on annule, sans prendre l'événement.
      Close(/*use=*/false);
      return false;
    default:
      return false;
  }
}

bool ShortcutWheel::TakeKeyDown(const SDL_KeyboardEvent& key, const Hotkeys& bindings,
                                WheelGate gate, glm::vec2 mouse) {
  if (open_ || key.repeat || !bar_.has_value() || gate.capturing_hotkey) {
    return false;
  }
  const std::optional<ActionRef> action =
      bindings.ActionFor(key.key, (key.mod & SDL_KMOD_CTRL) != 0, (key.mod & SDL_KMOD_SHIFT) != 0,
                         (key.mod & SDL_KMOD_ALT) != 0);
  if (action != ActionRef(GameAction::kShortcutWheel)) {
    return false;
  }
  Open(mouse, Opener::kKey, key.key);
  return true;
}

ro::ui::ShortcutBarsAction ShortcutWheel::WithChosen(const ro::ui::ShortcutBarsAction& asked) {
  ro::ui::ShortcutBarsAction merged = asked;
  if (chosen_.has_value() && merged.cast < 0) {
    merged.cast = static_cast<int>(*chosen_);
  }
  chosen_.reset();
  return merged;
}

}  // namespace moonclient
