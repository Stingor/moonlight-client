#pragma once

// CE QU'UNE FENÊTRE DE CONVERSATION A DEMANDÉ, et laquelle : une entrée de
// `UiAnswers::chat`. À part de `game/ui_answers.h`, qui ne fait que la nommer.

#include <cstdint>

#include "ro/ui/chat_window.h"

namespace moonclient {

struct UiChatWindowAsk {
  std::uint32_t group = 0;
  ro::ui::ChatWindowAction asked;
};

}  // namespace moonclient
