#pragma once

#include <cstdint>

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/cart_choice.h"

namespace ro::net {

// LA LISTE DES CHARRETTES QUE LE SERVEUR PROPOSE — `ZC 0x097F`, la réponse à
// `MC_CARTDECORATE`. Elle ouvre la fenêtre, à la place de celle qu'aurait
// dressée le client.
[[nodiscard]] FrameOutcome ApplyCartChoiceFrame(const Frame& frame, sim::CartChoice& choice);

// LE TYPE CHOISI DANS LA LISTE.
//
// 🔴 L'IDENTITÉ DE LA LISTE CHOISIT LE PAQUET : nulle, c'est la liste de
// `MC_CHANGECART` et la réponse part par `CZ 0x01AF` ; sinon, c'est celle du
// serveur, et la réponse part par `CZ 0x0980`, identité rappelée.
//
// ⚠ ÉMET, ET RIEN DE PLUS : l'apparence ne change qu'à l'état que le serveur
// repose ensuite sur le marchand.
[[nodiscard]] core::Result<void> RequestCartChoice(Connection& link, std::uint32_t identity,
                                                   std::uint8_t cart_type);

}  // namespace ro::net
