#pragma once

#include <cstdint>
#include <string_view>

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/friend_list.h"

namespace ro::net {

// ── LES AMIS : CE QUE LE SERVEUR EN DIT ─────────────────────────────────────
//
// Même contrat que `ApplyPartyFrame` : l'appelant soumet CHAQUE trame, et ce
// que ce traducteur ne reconnaît pas rend `kUnhandled`.
[[nodiscard]] FrameOutcome ApplyFriendFrame(const Frame& frame, sim::FriendList& friends);

// ── CE QU'ON LUI DEMANDE ────────────────────────────────────────────────────
//
// ⚠ LES NOMS ARRIVENT EN UTF-8 et partent en 1252.

// DEMANDER PAR LE NOM : le serveur cherche le personnage, qui doit être en
// ligne, et lui transmet la demande.
[[nodiscard]] core::Result<void> RequestFriend(Connection& link, std::string_view name);
[[nodiscard]] core::Result<void> AnswerFriendRequest(Connection& link, std::uint32_t account_id,
                                                     std::uint32_t char_id, bool accept);
[[nodiscard]] core::Result<void> RequestFriendRemoval(Connection& link, std::uint32_t account_id,
                                                      std::uint32_t char_id);

}  // namespace ro::net
