// L'INSTANCE, dans la boucle de la partie : son étage avance, et ses avis
// s'écrivent dans la conversation.

#include <ctime>
#include <string>
#include <utility>

#include "game/game.h"

namespace moonclient {

void Game::AdvanceMemorialDungeon(std::uint32_t now_ms) {
  // 🔴 LIAISON FERMÉE COMPRISE : la fenêtre se dessine encore, et sa demande
  // de destruction ne part pas.
  const auto now_unix_s = static_cast<std::int64_t>(std::time(nullptr));
  pieces_.memorial.Advance(windows_.memorial, link_, link_.is_open(), now_unix_s);
  for (std::string& notice : pieces_.memorial.TakeNotices()) {
    world_.RecordChat(ro::sim::ChatKind::kServer, 0, std::move(notice), now_ms);
  }
}

}  // namespace moonclient
