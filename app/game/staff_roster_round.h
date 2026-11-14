#pragma once

#include <cstdint>
#include <optional>

#include "ro/net/connection.h"
#include "ro/net/moonclient_handshake.h"
#include "ro/sim/world.h"
#include "ro/ui/entity_menu.h"
#include "ro/ui/staff_tools.h"

#include "game/game_pieces.h"
#include "world/entity_menu.h"
#include "world/staff_roster_menu.h"

namespace moonclient {

// Le menu d'un joueur de la liste : ce que la partie retient de sa cible, et
// ses lignes.
struct StaffRosterMenu {
  EntityMenuFacts target;
  ro::ui::EntityMenuView view;
};

// L'IMAGE DE LA FENÊTRE « JOUEURS EN LIGNE ». Sa vue descend dans l'état de
// l'établi, qui la dessine ; la pièce avance et émet sa demande ; et le clic
// droit sur une ligne, relevé à l'image précédente, rend le menu de son joueur.
//
// À appeler liaison tenue ou non : sans elle le droit du staff tombe, et la
// pièce oublie tout.
[[nodiscard]] std::optional<StaffRosterMenu> AdvanceStaffRoster(
    GamePieces& pieces, ro::ui::StaffToolsState& tools, ro::net::Connection& link,
    const ro::sim::World& world, const ro::net::ClientIdentity& own_client, std::uint32_t now_ms);

}  // namespace moonclient
