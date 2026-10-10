#pragma once

#include <cstdint>

namespace ro::ui {

// UNE DEMANDE D'ITEMS DU STAFF : cet item, en cette quantité.
//
// C'est une INTENTION, et rien de plus : la fenêtre ne sait pas qu'elle
// s'écrit « @item », et ne vérifie aucun droit. `app` revérifie le droit du
// staff au moment du geste, compose la commande et l'envoie — voir
// `chat/staff_item_command.h`.
struct StaffItemGrant {
  std::uint32_t item_id = 0;
  int amount = 0;

  friend bool operator==(const StaffItemGrant&, const StaffItemGrant&) = default;
};

// LE PLAFOND D'UNE DEMANDE : une pile pleine du serveur.
//
// 🔴 AU-DELÀ, LE SERVEUR REFUSE TOUT, il ne donne pas le plafond : une pile qui
// déborderait n'est pas créée du tout. Mieux vaut en donner une pleine que rien.
//
// ⚠ UN ITEM QUI NE S'EMPILE PAS — arme, armure, œuf — arrive un exemplaire par
// case, autant de fois que demandé : la quantité y est bornée par les cases
// libres de l'inventory, et chaque exemplaire de trop est refusé à son tour,
// avec son propre avis d'inventory plein.
inline constexpr int kStaffItemMostAmount = 30000;

}  // namespace ro::ui
