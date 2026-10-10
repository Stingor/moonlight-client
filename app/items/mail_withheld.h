#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#include "ro/ui/inventory_window.h"

namespace moonclient {

// LE SAC MONTRÉ PENDANT L'ÉCRITURE D'UN COURRIER.
//
// Les items joints RESTENT dans le sac du serveur jusqu'à l'envoi réussi : le
// sac du monde ne bouge donc pas. Le sac MONTRÉ, lui, retranche de chaque case
// ce qui est joint ou en route, et la case dont tout est retenu disparaît de la
// grille — elle n'offre plus rien à faire. Un brouillon à rouvrir après un
// envoi ne retient plus rien : il a oublié ses pièces jointes.
//
// `withheld` rend, pour une case du sac, la quantité retenue ; zéro pour une
// case libre de toute pièce jointe.
void WithholdMailedCells(std::vector<ro::ui::InventoryCellView>& cells,
                         const std::function<std::int32_t(std::uint16_t)>& withheld);

}  // namespace moonclient
