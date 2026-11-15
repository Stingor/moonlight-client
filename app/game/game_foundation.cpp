// LE SOCLE DE LA PARTIE ET LA PARTIE, construits : ce qu'elles tiennent de la
// coquille. Voir `game/game_foundation.h`.

#include <utility>

#include "game/game.h"
#include "options/quick_cast_options.h"

namespace moonclient {

GameFoundation::GameFoundation(ClientShell& shell, ro::net::Connection link, ro::sim::World world,
                               const ro::net::ZoneTicket& ticket)
    : shell_(shell),
      client_root_(shell.client_root()),
      vfs_(&shell.vfs()),
      gl_(&shell.gl()),
      settings_(&shell.settings()),
      windows_(shell.windows()),
      interface_(&shell.interface()),
      pieces_(shell.pieces()),
      audio_(shell.audio()),
      link_(std::move(link)),
      world_(std::move(world)),
      ticket_(ticket),
      targeting_(shell.settings().options().targeting().settings()),
      quick_cast_(shell.settings().options().quick_cast().look()),
      screenshot_(shell.client_root()) {}

Game::Game(ClientShell& shell, ro::net::Connection link, ro::sim::World world,
           const ro::net::ZoneTicket& ticket)
    : GameFoundation(shell, std::move(link), std::move(world), ticket),
      music_(shell.music()),
      card_album_(shell.client_root()) {}

}  // namespace moonclient
