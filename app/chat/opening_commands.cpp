// Les commandes qui ouvrent une fenêtre. Voir l'en-tête.

#include "chat/opening_commands.h"

#include <optional>
#include <string>
#include <utility>

#include "game/database_compose.h"
#include "game/database_stage.h"
#include "game/market_route.h"
#include "game/market_stage.h"

namespace moonclient {

bool FollowOpeningCommand(std::string_view line, MarketStage* market, DatabaseStage* database) {
  if (IsMarketCommand(line)) {
    if (market != nullptr) {
      market->AskOpen();
    }
    return true;
  }
  if (std::optional<std::string> query = ReadDatabaseCommand(line); query.has_value()) {
    if (database != nullptr) {
      if (query->empty()) {
        database->AskOpen();
      } else {
        database->AskOpenOnQuery(std::move(*query));
      }
    }
    return true;
  }
  return false;
}

}  // namespace moonclient
