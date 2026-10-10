#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>

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
// case, autant de fois que demandé : chaque exemplaire de trop est refusé à son
// tour, avec son propre avis d'inventory plein. Voir `StaffItemMostFor`.
inline constexpr int kStaffItemMostAmount = 30000;

// CE QUE LE SAC PERMET d'une demande, RANGÉ DU MIEUX AU PIRE : plusieurs
// demandes retiennent le pire par `std::max`.
enum class StaffItemRoom : std::uint8_t {
  // La demande passe entière : l'item s'empile, on ne sait pas, ou les cases
  // libres suffisent.
  kFits,
  // L'item ne s'empile pas : la demande est ramenée aux cases libres du sac.
  kBounded,
  // L'item ne s'empile pas et le sac est plein : rien ne peut venir.
  kFull,
};

// LA PLUS GRANDE QUANTITÉ QU'UNE DEMANDE PEUT OBTENIR de cet item.
//
// `stackable` ou `free_bag_slots` vide : on ne sait pas, et l'on ne borne rien
// — borner sur un doute priverait le staff d'un item qui s'empile peut-être,
// ou d'un sac dont le serveur n'a pas encore dit la capacité.
[[nodiscard]] constexpr int StaffItemMostFor(std::optional<bool> stackable,
                                             std::optional<int> free_bag_slots) noexcept {
  if (stackable.has_value() && !*stackable && free_bag_slots.has_value()) {
    return std::clamp(*free_bag_slots, 0, kStaffItemMostAmount);
  }
  return kStaffItemMostAmount;
}

// Ce que le sac permet d'une demande de `amount` exemplaires.
[[nodiscard]] constexpr StaffItemRoom StaffItemRoomFor(std::optional<bool> stackable,
                                                       std::optional<int> free_bag_slots,
                                                       int amount) noexcept {
  const int most = StaffItemMostFor(stackable, free_bag_slots);
  if (most >= kStaffItemMostAmount || amount <= most) {
    return StaffItemRoom::kFits;
  }
  return most == 0 ? StaffItemRoom::kFull : StaffItemRoom::kBounded;
}

}  // namespace ro::ui
