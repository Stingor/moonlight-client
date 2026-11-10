#pragma once

#include <functional>

#include "ro/net/connection.h"
#include "ro/sim/inventory.h"

namespace moonclient {

// LE JUGE CONSULTÉ AVANT L'USAGE D'UN OBJET, quel que soit le geste qui le
// demande — le sac, une case de la barre, sa répétition. Il peut émettre devant
// l'usage, sur `link`, ce que le serveur doit lire d'abord. Rend faux pour
// retenir l'usage.
using ItemUseGate = std::function<bool(const ro::sim::InventoryItem&, ro::net::Connection&)>;

}  // namespace moonclient
