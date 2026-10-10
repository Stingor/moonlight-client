#pragma once

#include <cstdint>

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/sim/movement.h"
#include "ro/sim/world_fwd.h"

namespace ro::net {

// « Je tourne sur place. »
//
// 🔴 LE PROTOCOLE D'ORIGINE NE SAIT PAS VISER UNE CASE : le paquet ne porte
// qu'un corps et un regard, tous deux déjà résolus. C'est au client de faire la
// géométrie, et c'est aussi lui qui décide de la CADENCE du demi-tour — le
// serveur pose ce qu'on lui donne, sans rien vérifier (`pc_setdir` n'est qu'une
// affectation). Un client qui poserait la direction visée d'un coup ferait
// pivoter le personnage plus vite que tous les autres, et le serveur le
// diffuserait tel quel. Voir `ro::sim::TurnToward` pour la règle mesurée.
//
// 🔴 ET LE SERVEUR NE RÉPOND RIEN. `clif_changed_dir` diffuse en AREA_WOS —
// tout le monde autour, SAUF l'envoyeur. Le demi-tour est donc posé dans le
// monde ICI, à l'émission ; l'attendre du fil ferait attendre pour toujours.
//
// ⚠ Rend une erreur si le serveur ne lit plus ce paquet à ces décalages, et
// n'écrit alors rien dans le monde : le personnage ne doit pas tourner chez
// nous d'un tour que personne d'autre ne verra.
// ⚠ LA FACE EST DÉJÀ RÉSOLUE quand elle arrive ici : c'est à l'appelant
// d'avoir consulté `ro::sim::TurnToward` en partant de ce que le personnage
// MONTRE — et ce n'est pas toujours ce que sa case annonce, un chemin gardant
// le regard de son dernier pas.
[[nodiscard]] core::Result<void> TurnPlayer(Connection& link, sim::World& world,
                                            sim::Facing facing);

}  // namespace ro::net
