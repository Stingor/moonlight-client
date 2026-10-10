#pragma once

// LES TRANSFERTS GELÉS : pendant la préparation d'une échoppe, aucun objet ne
// passe du sac à la charrette ou au coffre, ni dans l'autre sens.
//
// La préparation lit ce que la charrette et le sac contiennent ; le serveur
// refuserait le transfert de toute façon, mais en silence. Les demandes sont
// donc retenues ici, avant tout envoi, et l'appelant le dit au joueur.

#include "ro/ui/cart_window.h"
#include "ro/ui/inventory_window.h"
#include "ro/ui/storage_window.h"

namespace moonclient {

// Retire des demandes des trois fenêtres tout ce qui ferait passer un objet
// d'un contenant à l'autre — menu, double-clic, quantité confirmée, glisser.
// Le reste — utiliser, équiper, jeter, fermer le coffre — passe. Rend vrai si
// une demande a été retirée.
[[nodiscard]] bool HoldTransfers(ro::ui::InventoryWindowAction& inventory,
                                 ro::ui::CartWindowAction& cart,
                                 ro::ui::StorageWindowAction& storage);

// LE GLISSER REÇU À CETTE IMAGE. ⚠ TROIS FENÊTRES PEUVENT AVOIR REÇU, une seule
// a vraiment reçu : le joueur n'a qu'une souris. C'est la première qui parle,
// dans l'ordre sac, charrette, coffre ; aucune, une demande vide.
[[nodiscard]] ro::ui::ItemTransferAsk ReceivedTransfer(
    const ro::ui::InventoryWindowAction& inventory, const ro::ui::CartWindowAction& cart,
    const ro::ui::StorageWindowAction& storage);

}  // namespace moonclient
