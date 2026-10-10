#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "ro/core/error.h"

namespace ro::proto {

// ── LES GESTES D'ATELIER QU'AUCUNE STRUCTURE NE DÉCRIT ──────────────────────
//
// Même règle que `manual_packets.h` : le serveur lit ces paquets à la main, et
// chaque disposition est RECOUPÉE avec les décalages que sa table déclare avant
// d'être émise.

// « Affine l'arme de cette place du sac. »
//
// 🔴 MOINS UN N'EST PAS UNE PLACE : c'est l'ANNULATION. Le serveur n'efface le
// skill en attente qu'en recevant ce paquet, et un personnage qui ne l'envoie
// pas ne lance plus rien tant que ce skill reste armé.
//
// ⚠ LA PLACE EST CELLE DE LA LISTE REÇUE, déjà décalée par le serveur : elle
// repart telle quelle.
struct WeaponRefineRequest {
  static constexpr std::uint16_t kOpcode = 0x0222;
  static constexpr std::size_t kWireSize = 6;
  static constexpr std::int32_t kCancel = -1;

  std::int32_t index = kCancel;
};

[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const WeaponRefineRequest& request);

}  // namespace ro::proto
