#pragma once

#include <cstdint>

#include "ro/net/world_update.h"
#include "ro/sim/world_fwd.h"

namespace ro::net {

// LE MESSAGE D'UNE TALKIE BOX : `ZC_TALKBOX_CHATCONTENTS 0x0191`.
//
//     0191 <unité>.L <texte>.21B
//
// Le serveur l'envoie en zone quand la boîte se déclenche — quelqu'un marche
// dessus, ou Spring Trap la fait partir —, juste avant qu'elle ne prenne
// l'apparence du piège usé. L'identifiant est celui de l'UNITÉ AU SOL, et le
// client pose le texte au-dessus d'elle comme une parole.
//
// ⚠ LE TEXTE EST CELUI QU'UN JOUEUR A TAPÉ : en 1252 s'il a été écrit par le
// client d'origine, en UTF-8 s'il venait de celui-ci et qu'il le fallait.
[[nodiscard]] FrameOutcome ApplyTalkboxFrame(const Frame& frame, sim::World& world,
                                             std::uint32_t now_ms);

}  // namespace ro::net
