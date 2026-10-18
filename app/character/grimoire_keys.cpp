#include "character/grimoire_keys.h"

#include <SDL3/SDL_keyboard.h>

#include "ro/core/i18n.h"
#include "ro/core/printed.h"

#include "input/key_gate.h"

namespace moonclient {
namespace {

[[nodiscard]] bool Active(const ro::ui::CharacterWindowState& sheet) {
  return sheet.open && sheet.tab == ro::ui::CharacterTab::kGrimoire &&
         sheet.grimoire.keyboard_layout;
}

// La case que cette touche déclenche, ou rien.
[[nodiscard]] std::optional<std::size_t> SlotOf(const Hotkeys& bindings, const Binding& binding) {
  const std::optional<ActionRef> action =
      bindings.ActionFor(binding.key, binding.ctrl, binding.shift, binding.alt);
  if (!action.has_value() || !action->is_shortcut()) {
    return std::nullopt;
  }
  return action->shortcut_index();
}

}  // namespace

bool GrimoireKeys::TakeKeyDown(const SDL_KeyboardEvent& key,
                               const ro::ui::CharacterWindowState& sheet, const Hotkeys& bindings,
                               std::uint32_t now_ms) {
  // ⚠ LES RÉPÉTITIONS DE LA TOUCHE PRISE SONT PRISES AUSSI, même posée : elle
  // relancerait sinon le sort qu'on vient de mettre dans sa case.
  if (key.repeat) {
    return held_.has_value() && held_->binding.key == key.key;
  }
  if (!Active(sheet) || sheet.grimoire.keyboard_skill == 0 || IsModifierKey(key.key)) {
    return false;
  }
  const Binding binding{.key = key.key,
                        .ctrl = (key.mod & SDL_KMOD_CTRL) != 0,
                        .shift = (key.mod & SDL_KMOD_SHIFT) != 0,
                        .alt = (key.mod & SDL_KMOD_ALT) != 0};
  const std::optional<ActionRef> action =
      bindings.ActionFor(binding.key, binding.ctrl, binding.shift, binding.alt);
  if (action.has_value() && !action->is_shortcut()) {
    return false;
  }
  held_ = Held{.binding = binding,
               .scancode = key.scancode,
               .skill_id = sheet.grimoire.keyboard_skill,
               .level = sheet.grimoire.keyboard_level,
               .since_ms = now_ms};
  return true;
}

void GrimoireKeys::Advance(ro::ui::CharacterWindowState& sheet, ShortcutBarView& bar,
                           const Hotkeys& bindings, std::uint32_t now_ms) {
  if (!held_.has_value()) {
    return;
  }
  // LE RELÂCHEMENT SE LIT À L'ÉTAT DU CLAVIER, comme Quick cast : le jeu ne
  // reçoit pas les touches relâchées.
  const bool* const pressed = SDL_GetKeyboardState(nullptr);
  if (pressed == nullptr || !pressed[held_->scancode]) {
    held_.reset();
    return;
  }
  if (held_->settled) {
    return;
  }
  // ⚠ LA SOURIS PARTIE AILLEURS ABANDONNE : la compétence posée doit être
  // celle qu'on regarde au moment où elle se pose.
  if (!Active(sheet) || sheet.grimoire.keyboard_skill != held_->skill_id) {
    held_->settled = true;
    return;
  }
  if (now_ms - held_->since_ms < kGrimoireKeyHoldMs) {
    return;
  }
  held_->settled = true;
  if (const std::optional<std::size_t> slot = SlotOf(bindings, held_->binding); slot.has_value()) {
    bar.PlaceSkill(*slot, held_->skill_id, held_->level);
    return;
  }
  sheet.grimoire.keyboard_notice =
      ro::core::Printed(ro::core::Tr("« %s » ne déclenche aucune case de barre"),
                        LabelOfBinding(held_->binding).c_str());
  sheet.grimoire.keyboard_notice_fresh = true;
}

std::uint16_t GrimoireKeys::Highlighted(const ro::ui::CharacterWindowState& sheet) {
  return Active(sheet) ? sheet.grimoire.keyboard_skill : std::uint16_t{0};
}

}  // namespace moonclient
