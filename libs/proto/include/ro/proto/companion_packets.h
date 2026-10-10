#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "ro/core/error.h"

namespace ro::proto {

// ── LES ORDRES D'UN COMPAGNON QUE LE GÉNÉRATEUR N'ENCODE PAS ────────────────
//
// Homoncule et mercenaire partagent les trois ordres ; le serveur reconnaît le
// compagnon à son identifiant d'acteur. Aller à une case (`CZ_REQUEST_MOVENPC`,
// 0x0232) est engendré ; restent ici l'attaque et le retour au maître, que le
// serveur lit à la main.

// ATTAQUER (`CZ_REQUEST_ACTNPC`) : 11 octets, le compagnon, la cible, l'action.
//
// ⚠ LE SERVEUR LE JETTE SANS UN MOT pendant le délai qui suit l'entrée du
// maître sur une carte.
struct CompanionAttackRequest {
  static constexpr std::uint16_t kOpcode = 0x0233;
  static constexpr std::size_t kWireSize = 11;

  std::uint32_t companion_id = 0;
  std::uint32_t target_id = 0;
  // Zéro : un seul coup. Autre chose : en continu.
  std::uint8_t action = 0;
};

[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const CompanionAttackRequest& request);

// REVENIR AU MAÎTRE (`CZ_REQUEST_MOVETOOWNER`) : 6 octets, le compagnon. Le
// serveur le fait marcher vers une case voisine du maître.
struct CompanionReturnRequest {
  static constexpr std::uint16_t kOpcode = 0x0234;
  static constexpr std::size_t kWireSize = 6;

  std::uint32_t companion_id = 0;
};

[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const CompanionReturnRequest& request);

}  // namespace ro::proto
