#include "survivors/survivors_route.h"

#include <utility>

namespace moonclient {

bool IsConfirmKey(std::uint32_t scancode) noexcept {
  return scancode == kScancodeReturn || scancode == kScancodeSpace ||
         scancode == kScancodeKeypadEnter;
}

bool SurvivorsReadsKey(std::uint32_t scancode) noexcept {
  return IsMoveKey(scancode) || scancode == kScancodeEscape || IsConfirmKey(scancode) ||
         scancode == kScancodeTab || (scancode >= kScancodeDigit1 && scancode <= kScancodeDigit4);
}

SurvivorsKeyTurn SurvivorsKeyTurnOf(const SurvivorsEvent& event,
                                    const SurvivorsKeyFacts& facts) noexcept {
  const bool keyboard = event.kind == SurvivorsEventKind::kKeyDown ||
                        event.kind == SurvivorsEventKind::kKeyUp ||
                        event.kind == SurvivorsEventKind::kText;
  if (!keyboard) return SurvivorsKeyTurn::kArena;
  if (facts.typing) return SurvivorsKeyTurn::kTyping;
  const SurvivorsKeyTurn otherwise =
      facts.chat_claims ? SurvivorsKeyTurn::kTyping : SurvivorsKeyTurn::kArena;
  if (event.kind == SurvivorsEventKind::kText) return otherwise;
  if (facts.talk_key) return SurvivorsKeyTurn::kVoice;
  // Entrée et Échap gardent leur sens même quand la barre réclame les
  // frappes : ce sont elles qui l'ouvrent, l'envoient et la replient.
  if (event.kind == SurvivorsEventKind::kKeyDown && !event.repeat) {
    const bool enter = event.scancode == kScancodeReturn || event.scancode == kScancodeKeypadEnter;
    if (enter && !facts.chest_shown) return SurvivorsKeyTurn::kOpenChat;
    if (event.scancode == kScancodeEscape && facts.chat_folds_on_escape) {
      return SurvivorsKeyTurn::kFoldChat;
    }
  }
  return otherwise;
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
      } else if (IsConfirmKey(event.scancode)) {
        gestures_.confirm = true;
      } else if (event.scancode == kScancodeTab && event.ctrl) {
        gestures_.toggle_minimap = true;
      }
      return true;
    case SurvivorsEventKind::kKeyUp:
      static_cast<void>(keys_.Take(event.scancode, false));
      return true;
    case SurvivorsEventKind::kMouseDown:
    case SurvivorsEventKind::kMouseUp:
      mouse_x_ = event.x;
      mouse_y_ = event.y;
      // Un appui sur une fenêtre — la conversation — ne vise pas ; le
      // relâchement, lui, lâche toujours.
      if (event.button == kLeftMouseButton &&
          !(event.kind == SurvivorsEventKind::kMouseDown && event.over_interface)) {
        mouse_held_ = event.kind == SurvivorsEventKind::kMouseDown;
      }
      if (event.button == kRightMouseButton &&
          !(event.kind == SurvivorsEventKind::kMouseDown && event.over_interface)) {
        orbiting_ = event.kind == SurvivorsEventKind::kMouseDown;
        if (orbiting_ && event.second_of_double) gestures_.reset_yaw = true;
      }
      return true;
    case SurvivorsEventKind::kMouseMove:
      mouse_x_ = event.x;
      mouse_y_ = event.y;
      if (orbiting_) gestures_.orbit_px += event.dx_px;
      return true;
    case SurvivorsEventKind::kWheel:
      // Prise dans les deux cas — elle ne parvient pas au jeu —, mais la
      // caméra de l'arène ne la reçoit que hors des fenêtres : l'interface,
      // qui l'a vue avant, en fait défiler celle qui est survolée.
      if (!event.over_interface) gestures_.wheel_notches += event.wheel_notches;
      return true;
    case SurvivorsEventKind::kText:
      return true;
    case SurvivorsEventKind::kFocusLost:
      // Le relâchement des touches et du bouton n'arrivera pas.
      keys_.ReleaseAll();
      mouse_held_ = false;
      orbiting_ = false;
      gestures_.focus_lost = true;
      return false;
    case SurvivorsEventKind::kOther:
      return false;
  }
  return false;
}

SurvivorsGestures SurvivorsRoute::TakeGestures() noexcept { return std::exchange(gestures_, {}); }

void SurvivorsRoute::Reset() noexcept { *this = SurvivorsRoute(); }

void SurvivorsRoute::ReleaseHeld() noexcept {
  keys_.ReleaseAll();
  mouse_held_ = false;
  orbiting_ = false;
}

}  // namespace moonclient
