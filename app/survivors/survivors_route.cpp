#include "survivors/survivors_route.h"

#include <utility>

namespace moonclient {

bool SurvivorsReadsKey(std::uint32_t scancode) noexcept {
  return IsMoveKey(scancode) || scancode == kScancodeEscape ||
         (scancode >= kScancodeDigit1 && scancode <= kScancodeDigit4);
}

bool SurvivorsRoute::Take(const SurvivorsEvent& event) {
  switch (event.kind) {
    case SurvivorsEventKind::kKeyDown:
      if (keys_.Take(event.scancode, true)) return true;
      if (event.repeat) return true;
      if (event.scancode == kScancodeEscape) {
        gestures_.escape = true;
      } else if (event.scancode >= kScancodeDigit1 && event.scancode <= kScancodeDigit4) {
        gestures_.pressed_card_key = static_cast<int>(event.scancode - kScancodeDigit1) + 1;
      }
      return true;
    case SurvivorsEventKind::kKeyUp:
      static_cast<void>(keys_.Take(event.scancode, false));
      return true;
    case SurvivorsEventKind::kMouseDown:
    case SurvivorsEventKind::kMouseUp:
      mouse_x_ = event.x;
      mouse_y_ = event.y;
      if (event.button == kLeftMouseButton) {
        mouse_held_ = event.kind == SurvivorsEventKind::kMouseDown;
      }
      return true;
    case SurvivorsEventKind::kMouseMove:
      mouse_x_ = event.x;
      mouse_y_ = event.y;
      return true;
    case SurvivorsEventKind::kWheel:
      gestures_.wheel_notches += event.wheel_notches;
      return true;
    case SurvivorsEventKind::kText:
      return true;
    case SurvivorsEventKind::kFocusLost:
      // Le relâchement des touches et du bouton n'arrivera pas.
      keys_.ReleaseAll();
      mouse_held_ = false;
      gestures_.focus_lost = true;
      return false;
    case SurvivorsEventKind::kOther:
      return false;
  }
  return false;
}

SurvivorsGestures SurvivorsRoute::TakeGestures() noexcept { return std::exchange(gestures_, {}); }

void SurvivorsRoute::Reset() noexcept { *this = SurvivorsRoute(); }

}  // namespace moonclient
