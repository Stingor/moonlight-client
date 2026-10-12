#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "options/game_options.h"
#include "options/option_targets.h"
#include "options/talk_filter_options.h"

namespace moonclient {

// `/notalkmsg` (`/nm`) ET `/notalkmsg2` (`/nm2`) : la bascule que la ligne
// nomme, ou rien. La ligne entière compte, sans la casse latine, comme toute la
// table des commandes du jeu. Ce qu'elles taisent : voir `ro::sim::TalkMute`.
[[nodiscard]] std::optional<TalkFilterRow> ReadTalkFilterCommand(std::string_view line);

// BASCULE PAR LE CHEMIN DE LA FENÊTRE — la case suit, le fichier s'écrit — et
// rend l'écho que le jeu écrit dans la conversation, traduit.
[[nodiscard]] std::string ToggleTalkFilter(TalkFilterRow row, GameOptions& options,
                                           const OptionTargets& to, std::uint32_t now_ms);

}  // namespace moonclient
