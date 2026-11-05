// CE QUI DÉCIDE À QUI VA UNE TOUCHE ENFONCÉE, vu du routage des événements.

#include <optional>

#include <SDL3/SDL.h>

#include "ro/sim/world.h"
#include "ro/ui/chat_bar.h"

#include "game/window_events.h"
#include "input/key_gate.h"

namespace moonclient {

namespace {

// LE DIALOGUE EST-IL À L'ÉCRAN, ET MONTRE-T-IL UNE LISTE ? La fenêtre suit le
// monde sans rien décider : c'est donc le monde qui répond, et la boucle n'a pas
// à interroger l'affichage.
[[nodiscard]] bool NpcDialogShown(const ro::sim::World* world) {
  return world != nullptr && world->npc_dialog().has_value();
}

[[nodiscard]] bool NpcMenuShown(const ro::sim::World* world) {
  return NpcDialogShown(world) && world->npc_dialog()->prompt == ro::sim::NpcPrompt::kMenu;
}

// LA CONVERSATION ÉCRIT, SON CHAMP EST VIDE, ET CETTE TOUCHE AVEC ALT PORTE UNE
// LIGNE. Une case de macro encore vide laisse Alt Gr écrire son caractère.
[[nodiscard]] bool EmptyChatHoldsMacro(const EventRouting& to, bool chat_bar_takes_typing,
                                       SDL_Keycode key) {
  if (to.chat == nullptr || to.bindings == nullptr || to.macros == nullptr) {
    return false;
  }
  const ro::ui::ChatInputState& input = to.chat->input;
  if ((!input.was_active && !chat_bar_takes_typing) || input.text[0] != '\0') {
    return false;
  }
  const std::optional<ActionRef> action =
      to.bindings->ActionFor(key, /*ctrl=*/false, /*shift=*/false, /*alt=*/true);
  return action.has_value() && action->is_macro() &&
         action->macro_slot() < to.macros->rows.size() &&
         to.macros->rows[action->macro_slot()][0] != '\0';
}

}  // namespace

bool KeyDownGoesToGame(const EventRouting& to, const ro::ui::InputGate& gate,
                       const SDL_KeyboardEvent& key) {
  // 🔴 DEUX AVALAGES, ET PAS UN SEUL. Tant que l'interface prenait le
  // clavier SANS NUANCE, la barre de conversation éteignait le jeu entier :
  // ni F1 à F9, ni Alt+E, ni Inser tant qu'elle avait le focus — et un objet
  // simplement GLISSÉ faisait la même chose, ImGui comptant l'élément tenu
  // comme une prise de clavier.
  //
  // Une SAISIE ne réclame que les touches qui écrivent : `GameOnlyKey` dit
  // celles qu'elle ne peut pas utiliser, et elles reviennent au jeu. Une
  // prise TOTALE — une modale — garde tout, parce qu'il faut lui répondre
  // avant le reste.
  //
  // 🔴 ET LA BARRE DÉPLIÉE PAR LE BATTLE MODE RÉCLAME LES FRAPPES, même
  // sans le clavier : une lettre tapée là s'écrit dedans au lieu de faire
  // marcher le personnage. Voir `ChatBarKeepsKey`.
  //
  // 🔴 ET LE DIALOGUE D'UN PNJ GARDE CE QU'IL LIT, lui aussi sans le
  // clavier : Entrée y répond au script, et déplier la conversation
  // par-dessus la page ferait taper la frappe suivante dans une phrase.
  //
  // 🔴 ET LA CONVERSATION VIDE LAISSE ALT GR LANCER SA MACRO : voir
  // `AltGrMacroKey`.
  const bool chat_bar_takes_typing =
      to.chat != nullptr && ro::ui::ChatBarTakesTyping(to.chat->input.battle_mode,
                                                       to.chat->input.unfolded, gate.text_to_ui);
  const bool empty_chat_holds_macro = EmptyChatHoldsMacro(to, chat_bar_takes_typing, key.key);
  if (!KeyGoesToGame({.interface_has_keyboard = gate.keyboard_to_ui,
                      .typing = gate.typing_to_ui,
                      .chat_bar_takes_typing = chat_bar_takes_typing,
                      .npc_dialog_open = NpcDialogShown(to.world),
                      .npc_menu_open = NpcMenuShown(to.world),
                      .empty_chat_holds_macro = empty_chat_holds_macro},
                     key.key, key.mod)) {
    return false;
  }
  if (empty_chat_holds_macro && AltGrMacroKey(key.key, key.mod)) {
    to.chat->input.altgr_text_pending = true;
    ForgetPendingDeadKey();
  }
  return true;
}

}  // namespace moonclient
