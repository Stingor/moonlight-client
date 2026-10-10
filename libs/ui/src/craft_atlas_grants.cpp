// CE QU'EXIGE UNE RECETTE, CE QUI MANQUE, ET CE QUE LE STAFF PEUT EN DEMANDER.
// Aucun dessin : la fiche de l'Atlas et ses boutons du staff s'en servent.

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <vector>

#include "ro/ui/craft_atlas_window.h"
#include "ro/ui/staff_item_grant.h"

namespace ro::ui {

int CraftIngredientNeed(const CraftAtlasRow& ingredient, int crafts) noexcept {
  // Un matériau non consommé est satisfait dès qu'on en a UN : exiger sa
  // quantité, qui vaut zéro, le peindrait en rouge sur une recette faisable.
  if (ingredient.kept) {
    return 1;
  }
  const std::int64_t need = static_cast<std::int64_t>(ingredient.quantity) * std::max(crafts, 1);
  return static_cast<int>(std::clamp<std::int64_t>(need, 0, std::numeric_limits<int>::max()));
}

int CraftIngredientShortfall(const CraftAtlasRow& ingredient, int crafts) noexcept {
  return std::max(CraftIngredientNeed(ingredient, crafts) - std::max(ingredient.owned, 0), 0);
}

CraftAtlasGrantPlan CraftAtlasGrants(std::span<const CraftAtlasRow> ingredients, int crafts,
                                     CraftAtlasGrantKind kind, std::optional<int> free_bag_slots) {
  const int bounded = std::clamp(crafts, 1, kCraftAtlasMostCrafts);
  // Vide : la capacité du sac n'est pas dite, et rien ne se décompte.
  std::optional<int> slots_left;
  if (free_bag_slots.has_value()) {
    slots_left = std::max(*free_bag_slots, 0);
  }
  CraftAtlasGrantPlan plan;
  for (const CraftAtlasRow& ingredient : ingredients) {
    const int asked = kind == CraftAtlasGrantKind::kAll
                          ? CraftIngredientNeed(ingredient, bounded)
                          : CraftIngredientShortfall(ingredient, bounded);
    if (ingredient.item_id == 0 || asked <= 0) {
      continue;
    }
    const int amount = std::min(asked, kStaffItemMostAmount);
    const StaffItemRoom room = StaffItemRoomFor(ingredient.stackable, slots_left, amount);
    plan.room = std::max(plan.room, room);
    const int granted = std::min(amount, StaffItemMostFor(ingredient.stackable, slots_left));
    if (granted <= 0) {
      continue;
    }
    if (slots_left.has_value()) {
      if (ingredient.stackable == false) {
        *slots_left -= granted;
      } else if (ingredient.stackable == true && ingredient.owned <= 0 && *slots_left > 0) {
        // Une pile nouvelle : la première case libre.
        --*slots_left;
      }
    }
    plan.grants.push_back(StaffItemGrant{.item_id = ingredient.item_id, .amount = granted});
  }
  return plan;
}

}  // namespace ro::ui
