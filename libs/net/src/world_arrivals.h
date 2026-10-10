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

// Ce que laisse un départ, noté AVANT qu'on ne retire l'acteur : l'effet d'une
// déconnexion ou d'une téléportation qu'on voit, joué là où il était ; ou,
// sorti de vue, sa copie qui s'efface.
void NoteVanishEffect(sim::World& world, std::uint32_t actor_id, std::uint8_t vanish_type,
                      std::uint32_t now_ms);

}  // namespace ro::net
