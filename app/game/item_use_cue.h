#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

#include "ro/net/world_update.h"
#include "ro/sim/world.h"

namespace moonclient {

// L'USAGE D'UN OBJET, VU ET ENTENDU — le sien comme celui d'un voisin.
//
// Lit l'accusé d'usage (ZC_USE_ITEM_ACK2, 0x01C8) que le serveur envoie à toute
// la zone. Un usage réussi sème sur l'utilisateur les effets que
// `ro::sim::FindItemUseEffect` donne à l'objet, et rend le son à jouer, à
// l'oreille. Le manuel d'expérience écrit en plus sa ligne, pour soi seul.
//
// Rien ne se joue quand l'usage est refusé, ni quand l'utilisateur n'est pas en
// vue : ni effet, ni son, ni ligne.
//
// ⚠ À APPELER APRÈS LE MONDE, et sans regarder son verdict : le sac a déjà lu
// la trame s'il le devait, et cette lecture ne la réclame pas — l'accusé d'un
// voisin ne touche jamais au sac par ici.
[[nodiscard]] std::optional<std::string_view> CueItemUse(const ro::net::Frame& frame,
                                                         ro::sim::World& world,
                                                         std::uint32_t now_ms);

}  // namespace moonclient
