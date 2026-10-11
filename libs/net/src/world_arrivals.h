#pragma once

#include <cstdint>

#include "ro/sim/actor.h"
#include "ro/sim/world_fwd.h"

// CE QUE L'ARRIVÉE ET LE DÉPART D'UN ACTEUR MONTRENT, au-delà du fait lui-même.
//
// En-tête PRIVÉ, comme `world_relay.h` : l'aiguillage des trames du monde lit
// le paquet, et c'est ici que se décide ce qu'une apparition annoncée par le
// serveur (`clif_spawn`) et un départ laissent voir.
namespace ro::net {

// Fait entrer l'acteur qu'une apparition annonce. Neuf, une créature monte de
// la transparence (`Actor::appeared_ms`) et un joueur visible reçoit les
// anneaux bleus du portail ; réannoncé, il ne montre rien.
void SpawnAppearing(sim::World& world, sim::Actor actor, bool player, std::uint32_t now_ms);

// Applique une disparition (`clr_type`, dans src/map/clif.hpp). Rend faux pour
// la mort d'un acteur inconnu, qui n'est pas créé pour autant.
//
//   * LA MORT met l'acteur à terre — sauf un PNJ, qui reste debout : voir
//     `ro::sim::DeathAftermathOf`.
//   * LE RAFRAÎCHISSEMENT (type 4) retire l'acteur sur-le-champ, sans fondu ni
//     effet : le serveur le fait aussitôt réapparaître.
//   * Les autres départs le retirent aussi, après avoir noté ce qu'ils
//     laissent : l'effet d'une déconnexion ou d'une téléportation qu'on voit,
//     joué là où il était ; ou, sorti de vue, sa copie qui s'efface.
[[nodiscard]] bool ApplyVanish(sim::World& world, std::uint32_t actor_id, std::uint8_t vanish_type,
                               std::uint32_t now_ms);

}  // namespace ro::net
