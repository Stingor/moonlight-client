#pragma once

#include <cstdint>

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/adoption.h"

namespace ro::net {

// ── L'ADOPTION : CE QUE LE SERVEUR EN DIT ───────────────────────────────────
//
// Même contrat que `ApplyFriendFrame` : l'appelant soumet CHAQUE trame, et ce
// que ce traducteur ne reconnaît pas rend `kUnhandled`. Un refus au numéro
// inconnu est ignoré.
[[nodiscard]] FrameOutcome ApplyAdoptionFrame(const Frame& frame, sim::AdoptionDesk& desk);

// ── CE QU'ON LUI DEMANDE ────────────────────────────────────────────────────

// « J'adopte ce joueur », avec mon conjoint.
[[nodiscard]] core::Result<void> RequestAdoption(Connection& link, std::uint32_t account_id);

// La réponse à la demande reçue : elle renvoie les deux comptes que la demande
// portait.
[[nodiscard]] core::Result<void> AnswerAdoption(Connection& link,
                                                const sim::AdoptionInvitation& invitation,
                                                bool accept);

}  // namespace ro::net
