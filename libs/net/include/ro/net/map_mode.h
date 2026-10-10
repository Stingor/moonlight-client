#pragma once

#include <cstdint>

#include "ro/net/world_update.h"
#include "ro/sim/world_fwd.h"

namespace ro::net {

// CE QUE LA CARTE AUTORISE : `ZC_MAPPROPERTY_R2 0x099B`, et rien d'autre ne le
// dit.
//
// 🔴 CE N'EST PAS UN FAIT D'ACTEUR, ET C'EST POURQUOI CE N'EST PAS DANS
// `world_update.cpp`. Ce paquet ne nomme personne : il décrit le TERRAIN, et
// change ce que tous les acteurs deviennent les uns pour les autres. Il rejoint
// donc la chaîne de familles, comme les contenants et les dossiers de cible.
//
// ⚠ SON JUMEAU `ZC_NOTIFY_MAPPROPERTY 0x0199` NE VIENDRA JAMAIS : le serveur ne
// l'émet qu'en deçà de PACKETVER 20121010, et nous sommes à 20250716. Lui
// écrire un cas serait écrire du code mort — le client d'origine en porte un,
// et il ne s'exécute pas davantage.
//
// Voir `ro/sim/map_mode.h` pour ce que cette déclaration ne dit PAS, et qui
// compte autant que ce qu'elle dit.
[[nodiscard]] FrameOutcome ApplyMapModeFrame(const Frame& frame, sim::World& world);

}  // namespace ro::net
