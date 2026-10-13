#pragma once

#include <optional>

#include "ro/net/connection.h"
#include "ro/sim/equip_attributes.h"

namespace ro::net {

// CE QU'APPORTE UNE PIÈCE OU UN ENSEMBLE : `ZC_MOONLIGHT_EQUIP_ATTRIBUTES 0x0F3D`.
//
//     0f3d <len>.W <équipé>.B <nature>.B <id>.L { <niveau>.B <type>.W <valeur>.L×5 }*
//
// Le serveur l'envoie après l'accusé réussi d'une pose ou d'un retrait, à qui
// annonce `kUiCapEquipAttributes` : à la pose, la pièce puis chaque ensemble
// formé ; au retrait, chaque ensemble rompu puis la pièce. Rien à la connexion.
// Le familier s'y dit aussi : à l'éclosion, puis quand il quitte le joueur.
//
// ⚠ UNE NATURE INCONNUE REND LA TRAME ILLISIBLE, comme une trame tronquée : rien
// n'en est retenu. Le serveur peut en ajouter une, et un client qui l'écrirait
// comme une pièce dirait un mensonge.
[[nodiscard]] std::optional<sim::EquipAttributes> ReadEquipAttributes(const Frame& frame);

}  // namespace ro::net
