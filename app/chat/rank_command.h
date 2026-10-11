#pragma once

#include <optional>
#include <string_view>

#include "ro/net/rank_notices.h"

namespace moonclient {

// `/blacksmith`, `/alchemist` ET `/taekwon` : le classement demandé, ou rien
// quand la ligne n'est pas l'une des trois. La ligne entière compte, sans la
// casse latine.
[[nodiscard]] std::optional<ro::net::RankKind> ReadRankCommand(std::string_view line);

// `/ranking` : ouvre le tableau des classements. ⚠ UNE COMMANDE DE CE CLIENT,
// que le client d'origine n'a pas : elle ne part jamais au serveur. La ligne
// entière compte, sans la casse latine.
inline constexpr std::string_view kRankingBoardCommand = "/ranking";
[[nodiscard]] bool IsRankingBoardCommand(std::string_view line);

}  // namespace moonclient
