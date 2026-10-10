#pragma once

#include <span>
#include <string>
#include <vector>

#include "ro/ui/staff_item_grant.h"
#include "ro/ui/staff_tools.h"

namespace moonclient {

// ── « @item » : LES ITEMS QUE LE STAFF SE FAIT DONNER ───────────────────────
//
//     @item <item_id> <quantité>
//
// ⚠ L'IDENTIFIANT, JAMAIS LE NOM : le serveur cherche d'abord un nom, et un
// nom à espaces ou à deux-points y serait coupé en plusieurs items.
//
// Le serveur lit une quantité nulle ou négative comme UN : une demande vide ne
// se compose donc pas, plutôt que de donner un item que personne n'a demandé.

// La commande d'une demande : vide quand elle ne demande rien — item nul,
// quantité nulle. La quantité est ramenée à `kStaffItemMostAmount`.
[[nodiscard]] std::string StaffItemCommand(const ro::ui::StaffItemGrant& grant);

// LES COMMANDES À ENVOYER, dans l'ordre des demandes. 🔴 AUCUNE SANS LE DROIT
// du staff, relu au geste : le bouton a pu être dessiné une image avant que le
// lien ne tombe.
[[nodiscard]] std::vector<std::string> StaffItemCommands(
    std::span<const ro::ui::StaffItemGrant> grants, ro::ui::SessionRights rights);

// ── « @itemreset » : LE STAFF VIDE SON SAC ──────────────────────────────────
//
// Le serveur supprime tout l'inventaire SAUF l'équipement porté, les favoris
// et l'œuf du familier sorti. Il ne demande aucune confirmation, et le client
// non plus : c'est le choix du bouton.
//
// La commande, VIDE SANS LE DROIT du staff, relu au geste comme pour `@item`.
[[nodiscard]] std::string StaffItemResetCommand(ro::ui::SessionRights rights);

}  // namespace moonclient
