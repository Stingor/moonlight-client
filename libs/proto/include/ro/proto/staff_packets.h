#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "ro/core/error.h"

namespace ro::proto {

// ── LES ORDRES DU STAFF QUE LE SERVEUR LIT CHAMP PAR CHAMP ──────────────────
//
// Même règle que `manual_packets.h`, dont ils sont la famille : aucune
// structure ne les décrit côté serveur, et chaque disposition est RECOUPÉE
// avec les décalages que sa table déclare avant d'être émise. Ils vivent à
// part parce que l'autre en-tête touche son plafond de lignes.

// « Déconnecte ce joueur. »
//
// 🔴 LE SERVEUR NE FAIT PAS CONFIANCE AU PAQUET : il rejoue `@kick` sous les
// droits du groupe de l'émetteur. Un compte sans la permission n'obtient qu'un
// refus dans la conversation.
struct KickRequest {
  static constexpr std::uint16_t kOpcode = 0x00CC;
  static constexpr std::size_t kWireSize = 6;

  std::uint32_t account_id = 0;
};

[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const KickRequest& request);

}  // namespace ro::proto
