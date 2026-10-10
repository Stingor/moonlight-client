#pragma once

#include <cstdint>

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/spell_list.h"

namespace ro::net {

// LA LISTE D'AUTOSPELL : `0x0AFB`, les sorts que le joueur peut armer.
//
//     0AFB <longueur>.W { <compétence>.L }*
[[nodiscard]] FrameOutcome ApplySpellListFrame(const Frame& frame, sim::SpellListOffer& offer);

// LE CHOIX : `CZ_SELECTAUTOSPELL 0x01CE`. ⚠ ZÉRO EST LE REFUS, et le client
// l'envoie quand on ferme la liste sans choisir : sans lui, le serveur attend.
[[nodiscard]] core::Result<void> RequestSpellChoice(Connection& link, std::uint16_t skill_id);

}  // namespace ro::net
