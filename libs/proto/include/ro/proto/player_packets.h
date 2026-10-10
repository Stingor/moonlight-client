#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "ro/core/error.h"

namespace ro::proto {

// ── LES GESTES QU'ON FAIT SUR UN AUTRE JOUEUR ───────────────────────────────
//
// Même règle que `manual_packets.h`, dont ils sont la famille : aucune
// structure ne les décrit côté serveur, et chaque disposition est RECOUPÉE
// avec les décalages que sa table déclare avant d'être émise. Ils vivent à
// part parce que l'autre en-tête touche son plafond de lignes.

// « Montre-moi l'équipement de ce joueur. »
//
// ⚠ LA RÉPONSE NE NOMME PAS LE JOUEUR PAR SON IDENTIFIANT, seulement par son
// nom : qui veut redemander doit retenir à qui il a demandé.
struct ViewEquipRequest {
  static constexpr std::uint16_t kOpcode = 0x02D6;
  static constexpr std::size_t kWireSize = 6;

  std::uint32_t account_id = 0;
};

[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const ViewEquipRequest& request);

}  // namespace ro::proto
