#include "ro/ui/chat_bar.h"

#include <string>

#include <imgui.h>

#include "ro/ui/chat_input.h"

namespace ro::ui {
namespace {

// Le premier caractère qui ÉCRIT quelque chose, et celui qui efface.
//
// ⚠ SEULS LES IMPRIMABLES OUVRENT LA SAISIE. Entrée a son propre chemin ;
// Retour arrière et Tabulation n'ont rien à effacer ni à parcourir dans un
// champ qui n'existe pas encore.
inline constexpr ImWchar kFirstPrintable = 0x20;
inline constexpr ImWchar kDeleteCharacter = 0x7F;

}  // namespace

ChatBarAnswer PumpChatBarKeys(ChatInputState& state) {
  ChatBarAnswer answer;
  ImGuiIO& io = ImGui::GetIO();
  // Une zone de texte écrit — la nôtre une fois focalisée, ou celle d'une autre
  // fenêtre : les touches lui appartiennent, et elle les reçoit par le chemin
  // ordinaire d'ImGui.
  const bool text_elsewhere = io.WantTextInput;

  if (state.enter_pending) {
    state.enter_pending = false;
    if (!text_elsewhere) {
      switch (ChatBarEnterDoes(state.battle_mode, state.unfolded, state.text[0] != '\0')) {
        case ChatBarEnter::kUnfold:
          state.unfolded = true;
          state.wants_focus = true;
          break;
        case ChatBarEnter::kFocus:
          state.wants_focus = true;
          break;
        case ChatBarEnter::kSend:
          answer.said = std::string(state.text.data());
          RememberTypedLine(state.history, *answer.said);
          state.text[0] = '\0';
          // ⚠ LE CLAVIER RESTE À LA BARRE APRÈS L'ENVOI, comme dans le jeu : on
          // dit souvent deux phrases de suite, et rendre la touche au monde
          // entre les deux ferait marcher le personnage à la première lettre de
          // la seconde.
          state.wants_focus = true;
          break;
        case ChatBarEnter::kFold:
          // LA SORTIE, EN UNE FRAPPE. Rien ne réclame le clavier : c'est
          // justement ce que le joueur vient de rendre au jeu.
          state.unfolded = false;
          break;
      }
    }
  }

  if (state.escape_pending) {
    state.escape_pending = false;
    if (!text_elsewhere && ChatBarFoldsOnEscape(state.battle_mode, state.unfolded)) {
      state.unfolded = false;
      answer.folded = true;
    }
  }

  // ── « TAPER ÉCRIT DANS LA BARRE » ──────────────────────────────────────────
  // 🔴 CAPTURE D'ABORD, RESTITUTION ENSUITE, et cet ordre-là compte : la
  // restitution passe par `AddInputCharacter`, qui écrit dans la MÊME file que
  // celle qu'on lit. L'inverse relirait ce qu'on vient d'y remettre, en boucle.
  const bool takes_typing = ChatBarTakesTyping(state.battle_mode, state.unfolded, text_elsewhere);
  if (takes_typing && io.InputQueueCharacters.Size > 0) {
    for (const ImWchar letter : io.InputQueueCharacters) {
      if (letter >= kFirstPrintable && letter != kDeleteCharacter) {
        state.typed.push_back(letter);
      }
    }
    // ⚠ CONFISQUÉES : personne d'autre n'en veut, et les laisser passer les
    // ferait écrire deux fois le jour où un autre champ s'activerait.
    io.InputQueueCharacters.resize(0);
    if (!state.typed.empty()) {
      state.wants_focus = true;
    }
  }
  // Rendues quand la barre a cessé de réclamer, c'est-à-dire quand le champ a
  // pris le clavier : il lit la file au moment où il est soumis, donc après
  // nous.
  //
  // ⚠ ET JETÉES SI LA BARRE A DISPARU entre-temps — repliée, ou le réglage
  // éteint : mieux vaut perdre une lettre que la voir ressortir bien plus tard,
  // dans un champ que le joueur n'avait pas en tête.
  if (!takes_typing && !state.typed.empty()) {
    // Ce que le joueur a tapé PENDANT le battement passe derrière : on vide la
    // file et on la réécrit dans l'ordre, sinon la deuxième lettre arriverait
    // avant la première.
    for (const ImWchar letter : io.InputQueueCharacters) {
      state.typed.push_back(letter);
    }
    io.InputQueueCharacters.resize(0);
    if (ChatBarShown(state.battle_mode, state.unfolded)) {
      for (const ImWchar letter : state.typed) {
        io.AddInputCharacter(letter);
      }
    }
    state.typed.clear();
  }

  return answer;
}

}  // namespace ro::ui
