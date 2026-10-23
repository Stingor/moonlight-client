#pragma once

#include <cstdint>

#include "ro/core/error.h"
#include "ro/net/connection.h"

namespace ro::net {

// UN TROPHÉE DE MINI-JEU GAGNÉ : `CZ_MOONLIGHT_MINIGAME_TROPHY 0x0F4B`.
//
//     0f4b <len>.W <trophée>.B
//
// `trophy` est le rang du trophée dans le catalogue de `ro::sim::MinigameTrophy`.
// Le serveur achève le succès qui lui correspond ; un succès déjà achevé, il
// l'ignore sans un mot.
//
// 🔴 RIEN NE PROUVE LA PARTIE : ces succès ne rapportent rien au serveur.
[[nodiscard]] core::Result<void> SendMinigameTrophy(Connection& link, std::uint8_t trophy);

}  // namespace ro::net
