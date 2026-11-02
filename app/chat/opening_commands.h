#pragma once

// LES COMMANDES DE CE CLIENT QUI OUVRENT UNE FENÊTRE : `/marche`, `/db` et
// `/blocage`. Elles ne partent jamais au serveur, et la ligne se prend même
// quand la fenêtre manque.

#include <algorithm>
#include <array>
#include <string_view>

#include "ro/core/case_fold.h"

namespace ro::ui {
struct BlockListWindowState;
}

namespace moonclient {

class DatabaseStage;
class MarketStage;

// LA FENÊTRE DES JOUEURS BLOQUÉS, sous son nom et deux alias, à la casse près.
inline constexpr std::array<std::string_view, 3> kBlockListCommands = {"/blocage", "/blocages",
                                                                       "/blocklist"};
[[nodiscard]] inline bool IsBlockListCommand(std::string_view line) {
  return std::ranges::any_of(kBlockListCommands, [line](std::string_view command) {
    return ro::core::EqualsFolded(line, command);
  });
}

// Rend vrai quand la ligne était l'une d'elles. `/db <texte>` ouvre la base de
// données sur cette recherche.
[[nodiscard]] bool FollowOpeningCommand(std::string_view line, MarketStage* market,
                                        DatabaseStage* database,
                                        ro::ui::BlockListWindowState* block_list);

}  // namespace moonclient
