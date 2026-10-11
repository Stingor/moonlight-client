#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "ro/core/error.h"

namespace ro::proto {

// CZ_STOP_ATTACK : « cesse de frapper ». Le serveur arrête la suite de coups
// qu'une demande d'attaque sans relâche avait lancée. Rien d'autre que l'opcode.
struct StopAttackRequest {
  static constexpr std::uint16_t kOpcode = 0x0118;
  static constexpr std::size_t kWireSize = 2;
};

// ⚠ REFUSÉ si le serveur ne déclare plus le lire sous cet opcode et cette
// longueur : l'arrêt partirait dans le vide, et les coups continueraient.
[[nodiscard]] core::Result<std::vector<std::uint8_t>> EncodeStopAttack();

}  // namespace ro::proto
