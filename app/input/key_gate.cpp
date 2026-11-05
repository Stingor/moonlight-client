#include "input/key_gate.h"

#if defined(_WIN32)
#include <windows.h>
#endif

namespace moonclient {

bool IsModifierKey(SDL_Keycode key) {
  switch (key) {
    case SDLK_LCTRL:
    case SDLK_RCTRL:
    case SDLK_LSHIFT:
    case SDLK_RSHIFT:
    case SDLK_LALT:
    case SDLK_RALT:
    case SDLK_LGUI:
    case SDLK_RGUI:
      return true;
    default:
      return false;
  }
}

bool GameOnlyKey(SDL_Keycode key, SDL_Keymod held) {
  if (IsModifierKey(key)) {
    return false;
  }
  if (FunctionKey(key)) {
    return true;
  }
  if (key == SDLK_INSERT) {
    return true;
  }
  // ⚠ ALT GAUCHE, SANS CTRL NI MODE NI ALT DROIT : tout le reste est AltGr, qui
  // écrit des caractères — voir l'en-tête.
  constexpr SDL_Keymod kAltGrForms = SDL_KMOD_RALT | SDL_KMOD_CTRL | SDL_KMOD_MODE;
  return (held & SDL_KMOD_LALT) != 0 && (held & kAltGrForms) == 0;
}

bool AltGrMacroKey(SDL_Keycode key, SDL_Keymod held) {
  constexpr SDL_Keymod kChordModifiers =
      SDL_KMOD_ALT | SDL_KMOD_CTRL | SDL_KMOD_SHIFT | SDL_KMOD_GUI | SDL_KMOD_MODE;
  return (held & kChordModifiers) == SDL_KMOD_RALT && key != SDLK_0 && !IsModifierKey(key);
}

void ForgetPendingDeadKey() {
#if defined(_WIN32)
  // Traduire une touche consomme l'accent en attente : Espace le rend seul, et
  // ce rendu ne part nulle part.
  constexpr int kOutputChars = 8;
  constexpr int kKeyStates = 256;
  const BYTE no_key_held[kKeyStates] = {};
  WCHAR discarded[kOutputChars] = {};
  static_cast<void>(ToUnicode(VK_SPACE, MapVirtualKeyW(VK_SPACE, MAPVK_VK_TO_VSC), no_key_held,
                              discarded, kOutputChars, 0));
#endif
}

bool FunctionKey(SDL_Keycode key) { return key >= SDLK_F1 && key <= SDLK_F12; }

bool PresetKeyLive(bool text_input, SDL_Keycode key) { return !text_input || FunctionKey(key); }

bool ChatBarKeepsKey(SDL_Keycode key, SDL_Keymod held) {
  if (key == SDLK_RETURN || key == SDLK_KP_ENTER || key == SDLK_ESCAPE) {
    return false;  // les trois touches par lesquelles la barre s'ouvre et se ferme
  }
  return !GameOnlyKey(key, held);
}

bool NpcDialogKeepsKey(bool menu_open, SDL_Keycode key, SDL_Keymod held) {
  if ((held & (SDL_KMOD_CTRL | SDL_KMOD_ALT | SDL_KMOD_SHIFT)) != 0) {
    return false;
  }
  if (key == SDLK_RETURN || key == SDLK_KP_ENTER || key == SDLK_SPACE) {
    return true;
  }
  return menu_open && (key == SDLK_UP || key == SDLK_DOWN);
}

bool KeyGoesToGame(const KeyClaims& claims, SDL_Keycode key, SDL_Keymod held) {
  // 🔴 LE DIALOGUE EN PREMIER : il réclame sans avoir le clavier, comme la
  // barre dépliée, mais il passe AVANT elle — une Entrée qui répond au PNJ ne
  // doit pas d'abord déplier la conversation.
  if (claims.npc_dialog_open && NpcDialogKeepsKey(claims.npc_menu_open, key, held)) {
    return false;
  }
  // ⚠ UNE MODALE RESTE PLUS FORTE : la macro n'interrompt qu'une saisie.
  if (claims.empty_chat_holds_macro && AltGrMacroKey(key, held) &&
      (claims.typing || !claims.interface_has_keyboard)) {
    return true;
  }
  if (claims.chat_bar_takes_typing && ChatBarKeepsKey(key, held)) {
    return false;
  }
  if (!claims.interface_has_keyboard) {
    return true;
  }
  return claims.typing && GameOnlyKey(key, held);
}

}  // namespace moonclient
