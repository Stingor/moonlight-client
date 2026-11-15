// LE BLOCAGE D'UN PNJ, posé ou levé depuis son menu : le réglage, et la phrase
// qui dit au joueur ce que son clic gauche fera désormais.

#include <cstdint>
#include <string>

#include "ro/core/i18n.h"
#include "ro/core/printed.h"
#include "ro/sim/chat_line.h"

#include "game/game.h"
#include "options/context_menu_options.h"
#include "world/blocked_npcs.h"

namespace moonclient {

using ro::core::Tr;

void Game::ToggleNpcBlock(std::uint32_t id, const std::string& name, std::uint32_t now_ms) {
  const bool blocked = settings_->ToggleBlockedNpc(id, name);
  const std::string who = BlockedNpcLabel(id, name);
  std::string line;
  if (blocked) {
    // Marqué comme déjà expliqué : la phrase qui suit dit tout, et le premier
    // clic de vérification ne doit pas la redire deux secondes plus tard.
    static_cast<void>(settings_->options().context_menu().blocked().FirstExplanation(id));
    line = ro::core::Printed(Tr("%s : le clic gauche ne fait plus rien sur lui. Clic droit pour le "
                                "débloquer ou lui parler."),
                             who.c_str());
  } else {
    line = ro::core::Printed(Tr("%s : le clic gauche est de nouveau actif."), who.c_str());
  }
  world_.RecordChat(ro::sim::ChatKind::kServer, 0, line, now_ms, kClientSaysColor);
}

}  // namespace moonclient
