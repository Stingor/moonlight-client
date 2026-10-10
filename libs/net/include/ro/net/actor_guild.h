#pragma once

#include "ro/net/world_update.h"
#include "ro/sim/world_fwd.h"

namespace ro::net {

// LA GUILDE D'UN ACTEUR QUI EN CHANGE SOUS NOS YEUX.
//
// 🔴 `ZC_CHANGE_GUILD` 0x0B47 EST UNE RÉANNONCE PARTIELLE. Le serveur l'envoie
// à la zone quand un joueur entre dans une guilde, la quitte, ou quand
// l'emblème de sa guilde change. Sans elle, l'acteur garderait l'ancienne
// guilde — et l'ancien emblème — jusqu'à sa prochaine apparition, c'est-à-dire
// jusqu'à son premier pas s'il reste immobile.
//
// ⚠ ZÉRO COMPRIS : c'est un départ, et l'emblème s'en va avec.
//
// La version d'emblème, elle, va au carnet de guilde, qui lit la même trame :
// voir `ro/net/guild.h`. Un acteur inconnu ne s'invente pas.
[[nodiscard]] FrameOutcome ApplyActorGuildFrame(const Frame& frame, sim::World& world);

}  // namespace ro::net
