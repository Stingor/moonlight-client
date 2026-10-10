#pragma once

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/repair_list.h"

namespace ro::net {

// LA LISTE DE REPAIR WEAPON : `0x0B65`, les pièces abîmées du joueur visé.
//
//     0B65 <longueur>.W { <place>.W <objet>.L <cartes>.4L <raffinement>.B <grade>.B }*
//
// Et son accusé, `ZC_ACK_ITEMREPAIR 0x01FE`, dont le résultat vaut zéro pour une
// pièce réparée.
[[nodiscard]] FrameOutcome ApplyRepairFrame(const Frame& frame, sim::RepairListOffer& offer);

// LE CHOIX : `CZ_REQ_ITEMREPAIR2 0x0B66`, l'entrée choisie renvoyée telle quelle.
// ⚠ SANS ENTRÉE, C'EST LE REFUS, et il part aussi : sa place vaut moins un.
[[nodiscard]] core::Result<void> RequestRepair(Connection& link, const sim::RepairableItem* chosen);

}  // namespace ro::net
