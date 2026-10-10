#pragma once

#include <cstdint>

#include "ro/sim/world_fwd.h"
#include "ro/vfs/monster_size_effects.h"

namespace moonclient {

// LES EFFETS QUE L'INSTALLATION DONNE D'OFFICE À CERTAINES CRÉATURES : voir
// `ro/vfs/monster_size_effects.h`.
//
// Le client les pose au paquet qui fait paraître l'acteur, et les retire quand
// une option le cache — pour les reposer quand il reparaît. Aucun paquet ne les
// annonce.
//
// 🔴 C'EST LA LISTE D'EFFETS DE L'ACTEUR QUI DIT S'IL EST PARÉ, et non une
// mémoire à côté. Une apparition neuve arrive la liste vide — le serveur peut
// retirer puis réannoncer un monstre dans la même salve, et une mémoire par
// identifiant le croirait encore paré. Un effet qui finit de lui-même, lui,
// reste dans la liste : il ne se rejoue pas, pas plus que chez le client.
//
// Visible et sans aucun de ses effets d'office, la créature les reçoit tous ;
// cachée, elle les perd.
void DressMonsters(ro::sim::World& world, const ro::vfs::MonsterSizeEffects* table,
                   std::uint32_t now_ms);

}  // namespace moonclient
