#include "ro/sim/npc_shop_cart.h"

#include <algorithm>
#include <unordered_map>

#include "ro/sim/player_status.h"

namespace ro::sim {
namespace {

void Note(NpcShopVerdict& verdict, NpcShopIssue issue) {
  if (!verdict.Has(issue)) {
    verdict.issues.push_back(issue);
  }
}

// Ce que le sac porte déjà de cet objet, pièces équipées exclues : elles ne
// comptent pas dans la pile que l'achat viendrait grossir.
std::int64_t CarriedCount(std::span<const InventoryItem> bag, std::uint32_t item_id) {
  std::int64_t carried = 0;
  for (const InventoryItem& item : bag) {
    if (item.item_id == item_id && item.worn_at == 0) {
      carried += item.amount;
    }
  }
  return carried;
}

const InventoryItem* ItemAtSlot(std::span<const InventoryItem> bag, std::uint16_t client_slot) {
  const auto found = std::ranges::find_if(
      bag, [client_slot](const InventoryItem& item) { return item.client_slot == client_slot; });
  return found == bag.end() ? nullptr : &*found;
}

}  // namespace

bool ItemTypeStacks(std::uint8_t item_type) noexcept {
  switch (item_type) {
    case kItemTypeWeapon:
    case kItemTypeArmor:
    case kItemTypePetEgg:
    case kItemTypePetArmor:
    case kItemTypeShadowGear:
      return false;
    default:
      return true;
  }
}

bool NpcShopVerdict::Has(NpcShopIssue issue) const noexcept {
  return std::ranges::find(issues, issue) != issues.end();
}

NpcShopVerdict CheckNpcShopBuy(const NpcShop& shop, std::span<const NpcShopBuyLine> cart,
                               std::int64_t money, std::span<const InventoryItem> bag,
                               std::int64_t weight, std::int64_t max_weight) {
  NpcShopVerdict verdict;
  if (cart.empty()) {
    Note(verdict, NpcShopIssue::kEmptyCart);
    return verdict;
  }
  // ⚠ LE POIDS NE SE JUGE QUE SI CHAQUE LIGNE LE DONNE, et que le personnage a
  // dit le sien : une seule ligne muette rendrait le total faux par défaut.
  verdict.weight_known = weight != kUnknownStat && max_weight != kUnknownStat;
  // Deux lignes du même objet se cumulent pour le stock et pour la pile.
  std::unordered_map<std::uint32_t, std::int64_t> wanted;
  for (const NpcShopBuyLine& line : cart) {
    if (line.amount <= 0) {
      Note(verdict, NpcShopIssue::kBadAmount);
      continue;
    }
    const auto offer = std::ranges::find_if(
        shop.buy_offers(), [&line](const NpcShopOffer& on) { return on.item_id == line.item_id; });
    if (offer == shop.buy_offers().end()) {
      Note(verdict, NpcShopIssue::kNotOffered);
      continue;
    }
    verdict.total += static_cast<std::int64_t>(offer->price) * line.amount;
    // Une liste qui ne porte pas de poids en dit 0 : la table répond alors.
    const std::optional<std::uint32_t> unit_weight =
        offer->unit_weight != 0 ? std::optional<std::uint32_t>(offer->unit_weight)
                                : line.table_unit_weight;
    if (!unit_weight.has_value()) {
      verdict.weight_known = false;
    }
    verdict.added_weight += static_cast<std::int64_t>(unit_weight.value_or(0)) * line.amount;
    const std::int64_t total_wanted = wanted[line.item_id] += line.amount;
    if (!ItemTypeStacks(offer->item_type)) {
      if (total_wanted > 1) {
        Note(verdict, NpcShopIssue::kOnePerPurchase);
      }
    } else if (CarriedCount(bag, line.item_id) + total_wanted > kNpcShopMostPerStack) {
      Note(verdict, NpcShopIssue::kOverStack);
    }
    if (offer->stock != kNpcShopUnlimitedStock && total_wanted > offer->stock) {
      Note(verdict, NpcShopIssue::kOverStock);
    }
  }
  // ⚠ UNE BOURSE QUE LE SERVEUR N'A PAS ENCORE DITE NE RETIENT RIEN : « je ne
  // sais pas » n'est pas « zéro ».
  if (money != kUnknownStat && verdict.total > money) {
    Note(verdict, NpcShopIssue::kLacksMoney);
  }
  if (verdict.weight_known && weight + verdict.added_weight > max_weight) {
    Note(verdict, NpcShopIssue::kOverWeight);
  }
  return verdict;
}

NpcShopVerdict CheckNpcShopSell(const NpcShop& shop, std::span<const NpcShopSellLine> cart,
                                std::span<const InventoryItem> bag, std::int64_t zeny) {
  NpcShopVerdict verdict;
  if (cart.empty()) {
    Note(verdict, NpcShopIssue::kEmptyCart);
    return verdict;
  }
  // 🔴 UNE SEULE LIGNE FAUSSE FAIT ÉCHOUER TOUTE LA VENTE (`npc_selllist`) : on
  // les juge donc toutes, et une seule raison suffit à retenir le bouton.
  std::unordered_map<std::uint16_t, std::int64_t> wanted;
  for (const NpcShopSellLine& line : cart) {
    if (line.amount <= 0) {
      Note(verdict, NpcShopIssue::kBadAmount);
      continue;
    }
    const auto offer = std::ranges::find_if(
        shop.sell_offers(),
        [&line](const NpcShopSellOffer& on) { return on.client_slot == line.client_slot; });
    const InventoryItem* item = ItemAtSlot(bag, line.client_slot);
    if (offer == shop.sell_offers().end() || item == nullptr) {
      Note(verdict, NpcShopIssue::kNotOffered);
      continue;
    }
    verdict.total += static_cast<std::int64_t>(offer->price) * line.amount;
    if ((wanted[line.client_slot] += line.amount) > item->amount) {
      Note(verdict, NpcShopIssue::kOverOwned);
    }
  }
  if (zeny != kUnknownStat && zeny + verdict.total > kMostZeny) {
    Note(verdict, NpcShopIssue::kZenyOverflow);
  }
  return verdict;
}

bool NpcShopListsForSale(const InventoryItem& item, bool favorites_locked) noexcept {
  return item.amount > 0 && !(favorites_locked && item.favorite);
}

}  // namespace ro::sim
