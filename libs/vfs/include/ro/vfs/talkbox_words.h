#pragma once

#include <string>
#include <string_view>

#include "ro/vfs/item_words.h"

namespace ro::vfs {

// CE QU'ÉCRIT LA FENÊTRE DE SAISIE DE TALKIE BOX : son titre, et la ligne qui
// demande le message. Des textes de l'interface, posés par `FillTalkboxWords`
// (ADR 0090).
[[nodiscard]] std::string_view TalkboxTitle(const ItemWords& words);
[[nodiscard]] std::string_view TalkboxPrompt(const ItemWords& words);

// Pose ces phrases dans la langue courante.
void FillTalkboxWords(TalkboxWords& words);

}  // namespace ro::vfs
