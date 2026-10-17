#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "ro/core/error.h"

namespace ro::proto {

// CZ_REQ_USER_COUNT : « combien de joueurs sont connectés ? ». Rien d'autre que
// l'opcode ; le serveur répond par `ZcUserCount`, au seul demandeur.
struct UserCountRequest {
  static constexpr std::uint16_t kOpcode = 0x00C1;
  static constexpr std::size_t kWireSize = 2;
};

// ⚠ REFUSÉ si le serveur ne déclare plus le lire sous cet opcode et cette
// longueur : la question partirait dans le vide, et `/who` resterait muet.
[[nodiscard]] core::Result<std::vector<std::uint8_t>> EncodeUserCountRequest();

}  // namespace ro::proto
