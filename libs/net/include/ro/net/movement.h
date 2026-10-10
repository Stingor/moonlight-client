#pragma once

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/sim/packed_position.h"

namespace ro::net {

// Demander au serveur de marcher jusqu'à une case.
//
// LE SERVEUR EST AUTORITAIRE. Cette fonction ne déplace personne : elle pose une
// demande. Le serveur répond en ANNONÇANT un déplacement, que world_update
// applique — ou ne répond rien du tout si la case est inaccessible, occupée, ou
// hors de portée. Une destination refusée se voit donc à l'absence de mouvement,
// jamais à une erreur.
//
// Bouger le personnage tout de suite, pour faire réactif, le désynchroniserait
// du serveur à la première case bloquée, et il faudrait ensuite le ramener de
// force — le défaut que tous les clients répliques montrent contre un mur.
//
// L'erreur rendue ne concerne que l'ÉMISSION : une disposition de paquet qui ne
// s'accorde plus avec ce que le serveur déclare lire.
[[nodiscard]] core::Result<void> RequestMove(Connection& link, sim::CellPosition destination);

// Dire au serveur que la carte est chargée.
//
// ⚠ À redire APRÈS CHAQUE SAUT, et pas seulement à l'entrée en jeu. Le serveur
// RETIRE le personnage de la carte avant de l'annoncer ailleurs
// (unit_remove_map_pc puis clif_changemap, dans pc_setpos) et ne l'y remet
// qu'en recevant cet avis : c'est clif_parse_LoadEndAck qui appelle
// map_addblock. Tant qu'il ne l'a pas, il IGNORE tout ce que le client envoie —
// le répartiteur de paquets écarte tout sauf cet avis-là pour un personnage
// hors carte. Sans lui, on arrive quelque part et l'on ne peut plus bouger.
//
// Le redire pour rien ne coûte rien : le serveur sort aussitôt si le
// personnage est déjà posé.
[[nodiscard]] core::Result<void> NotifyMapLoaded(Connection& link);

}  // namespace ro::net
