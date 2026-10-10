#pragma once

#include <cstdint>

#include "ro/net/world_update.h"
#include "ro/sim/world_fwd.h"

namespace ro::net {

// LE BOND DE BODY RELOCATION (Snap) : `0x08D2`, l'acteur et sa case d'arrivée.
//
//     08D2 <acteur>.L <x>.W <y>.W
//
// Le serveur l'envoie en zone quand le bond réussit. L'acteur GLISSE vers la
// case d'arrivée à quinze millisecondes par case, dans la pose d'incantation, et
// un joueur laisse une traînée blanche de cent tics.
[[nodiscard]] FrameOutcome ApplySnapFrame(const Frame& frame, sim::World& world,
                                          std::uint32_t now_ms);

}  // namespace ro::net
