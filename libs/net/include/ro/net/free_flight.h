#pragma once

#include <cstdint>

#include "ro/core/error.h"
#include "ro/net/connection.h"

namespace ro::net {

// LE VOL LIBRE DE L'ADMINISTRATEUR : son corps suit la caméra.
//
//     0F54 <longueur>.W <x>.W <y>.W      pose le corps sur cette case
//
// Le serveur déplace le corps sans chemin ni contrôle de passage, murs compris,
// puis annonce la glissade à la zone, le joueur compris (0x01FF). La zone qu'il
// envoie se recalcule autour de la nouvelle case : c'est ce qui fait voir les
// monstres et les PNJ loin du point de départ.
//
// 🔴 REFUSÉ SANS UN MOT sous le rang d'administrateur, hors de moonclient et
// hors de la carte : le corps reste alors où il est.
[[nodiscard]] core::Result<void> RequestFreeFlightCell(Connection& link, std::int16_t cell_x,
                                                       std::int16_t cell_y);

}  // namespace ro::net
