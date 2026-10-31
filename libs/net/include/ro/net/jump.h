#pragma once

#include <cstdint>

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/world_fwd.h"

namespace ro::net {

// LE SAUT COSMÉTIQUE : voir `ro/sim/jump.h`.
//
//     0F1A <longueur>.W                  le joueur saute
//     0F1B <longueur>.W <compte>.L       un joueur en vue a sauté
//
// 🔴 LE SERVEUR NE RÉPOND RIEN À L'ENVOYEUR : il rediffuse à ceux qui
// l'entourent, et à eux seuls. Le saut se pose donc dans le monde ICI, à
// l'émission ; l'attendre du fil le ferait attendre pour toujours.
//
// ⚠ MUET SANS LA POIGNÉE DE MAIN : comme tous les paquets personnalisés de ce
// serveur, il ne lit et ne rediffuse le saut que pour les clients qui se sont
// annoncés. Voir l'ADR 0040.

// Fait sauter le joueur, et le dit au serveur. Rien ne part tant que le saut
// précédent n'est pas retombé : le serveur jetterait celui-là.
[[nodiscard]] core::Result<void> JumpPlayer(Connection& link, sim::World& world,
                                            std::uint32_t now_ms);

// Un autre joueur a sauté. Notre propre compte, s'il revenait, est ignoré : le
// saut est déjà en l'air chez nous.
[[nodiscard]] FrameOutcome ApplyJumpFrame(const Frame& frame, sim::World& world,
                                          std::uint32_t now_ms);

}  // namespace ro::net
