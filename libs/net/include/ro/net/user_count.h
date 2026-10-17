#pragma once

#include <cstdint>
#include <optional>

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/net/world_update.h"

namespace ro::net {

// LE NOMBRE DE JOUEURS CONNECTÉS, que `/who` demande.
//
//   CZ 0x00C1 [op]               la question, sans champ
//   ZC 0x00C2 [op][compte:4]     la réponse, au seul demandeur
//
// ⚠ LE SERVEUR COMPTE TOUTE LA ZONE, pas la carte : c'est `map_getusers`.

// « Combien sommes-nous ? » ⚠ ÉMET, ET RIEN DE PLUS : la réponse arrive comme
// une trame ordinaire.
[[nodiscard]] core::Result<void> RequestUserCount(Connection& link);

// Le compte d'une réponse, ou rien quand la trame n'en est pas une.
[[nodiscard]] std::optional<std::int32_t> ReadUserCount(const Frame& frame);

}  // namespace ro::net
