#include "ro/sim/buying_store_checks.h"

#include <algorithm>
#include <map>

namespace ro::sim {
namespace {

// LA PILE QUE LE SERVEUR COMPLÈTE : la première case, dans l'ordre du sac, qui
// porte l'objet. C'est elle seule qu'il compte, pas le total des piles.
const InventoryItem* FirstStackOf(std::span<const InventoryItem> bag, std::uint32_t item_id) {
  const InventoryItem* first = nullptr;
  for (const InventoryItem& item : bag) {
    if (item.item_id == item_id && (first == nullptr || item.client_slot < first->client_slot)) {
      first = &item;
    }
  }
  return first;
}

const InventoryItem* ItemAt(std::span<const InventoryItem> bag, std::uint16_t client_slot) {
  const auto found = std::ranges::find(bag, client_slot, &InventoryItem::client_slot);
  return found == bag.end() ? nullptr : &*found;
}

const BuyingStoreLine* WantedLineOf(const VisitedBuyingStore& store, std::uint32_t item_id) {
  const auto found = std::ranges::find(store.lines, item_id, &BuyingStoreLine::item_id);
  return found == store.lines.end() ? nullptr : &*found;
}

bool HasAnyCard(const InventoryItem& item) {
  return std::ranges::any_of(item.cards, [](std::uint32_t card) { return card != 0; });
}

}  // namespace

bool BlocksOpening(BuyingStoreDraftFault fault) noexcept {
  return fault != BuyingStoreDraftFault::kLimitBelowTotalCost;
}

std::int64_t BuyingStoreDraftCost(std::span<const BuyingStoreWish> lines) noexcept {
  std::int64_t cost = 0;
  for (const BuyingStoreWish& line : lines) {
    cost += static_cast<std::int64_t>(line.amount) * line.price;
  }
  return cost;
}

std::vector<BuyingStoreDraftProblem> CheckBuyingStoreDraft(
    std::string_view title_on_wire, std::int64_t zeny_limit, std::span<const BuyingStoreWish> lines,
    std::size_t slots, std::span<const InventoryItem> bag, std::int64_t zeny) {
  std::vector<BuyingStoreDraftProblem> problems;
  const auto flag = [&problems](BuyingStoreDraftFault fault, std::size_t line = kNoFaultLine) {
    problems.push_back(BuyingStoreDraftProblem{.fault = fault, .line = line});
  };

  if (lines.empty()) {
    flag(BuyingStoreDraftFault::kNoLine);
  }
  if (lines.size() > slots) {
    flag(BuyingStoreDraftFault::kTooManyLines);
  }
  if (title_on_wire.empty()) {
    flag(BuyingStoreDraftFault::kEmptyTitle);
  }

  for (std::size_t rank = 0; rank < lines.size(); ++rank) {
    const BuyingStoreWish& line = lines[rank];
    const auto earlier = lines.first(rank);
    if (std::ranges::find(earlier, line.item_id, &BuyingStoreWish::item_id) != earlier.end()) {
      flag(BuyingStoreDraftFault::kDuplicateItem, rank);
    }
    if (line.amount == 0) {
      flag(BuyingStoreDraftFault::kZeroAmount, rank);
    }
    if (line.price == 0) {
      flag(BuyingStoreDraftFault::kZeroPrice, rank);
    } else if (line.price > kBuyingStoreMostPrice) {
      flag(BuyingStoreDraftFault::kPriceAboveMost, rank);
    }
    const InventoryItem* owned = FirstStackOf(bag, line.item_id);
    if (owned == nullptr) {
      flag(BuyingStoreDraftFault::kNotInBag, rank);
    } else if (static_cast<std::int64_t>(std::max<std::int16_t>(owned->amount, 0)) + line.amount >
               kBuyingStoreMostOwned) {
      flag(BuyingStoreDraftFault::kOwnedAboveMost, rank);
    }
  }

  bool limit_valid = true;
  if (zeny_limit <= 0) {
    flag(BuyingStoreDraftFault::kLimitNotPositive);
    limit_valid = false;
  } else if (zeny_limit > zeny || zeny_limit > kBuyingStoreMostZeny) {
    flag(BuyingStoreDraftFault::kLimitAboveZeny);
    limit_valid = false;
  }
  if (limit_valid && !lines.empty() && zeny_limit < BuyingStoreDraftCost(lines)) {
    flag(BuyingStoreDraftFault::kLimitBelowTotalCost);
  }
  return problems;
}

std::int64_t BuyingStoreSaleTotal(const VisitedBuyingStore& store,
                                  std::span<const BuyingStoreSaleLine> lines) noexcept {
  std::int64_t total = 0;
  for (const BuyingStoreSaleLine& line : lines) {
    if (const BuyingStoreLine* wanted = WantedLineOf(store, line.item_id); wanted != nullptr) {
      total += static_cast<std::int64_t>(line.amount) * wanted->price;
    }
  }
  return total;
}

std::vector<BuyingStoreSaleProblem> CheckBuyingStoreSale(const VisitedBuyingStore& store,
                                                         std::span<const BuyingStoreSaleLine> lines,
                                                         std::span<const InventoryItem> bag,
                                                         std::int64_t zeny) {
  std::vector<BuyingStoreSaleProblem> problems;
  const auto flag = [&problems](BuyingStoreSaleFault fault, std::size_t line = kNoFaultLine) {
    problems.push_back(BuyingStoreSaleProblem{.fault = fault, .line = line});
  };

  if (lines.empty()) {
    flag(BuyingStoreSaleFault::kNoLine);
  }
  if (lines.size() > kBuyingStoreMostSaleLines) {
    flag(BuyingStoreSaleFault::kTooManyLines);
  }

  // 🔴 LE CUMUL PAR OBJET, que le serveur ne fait pas. Il compare chaque ligne à
  // ce qui est voulu SANS retrancher les lignes précédentes du même objet, puis
  // les soustrait toutes d'une quantité non signée : deux piles qui passent
  // chacune feraient tomber la ligne de l'acheteur sous zéro. On refuse avant.
  std::map<std::uint32_t, std::uint32_t> sold_by_item;
  for (std::size_t rank = 0; rank < lines.size(); ++rank) {
    const BuyingStoreSaleLine& line = lines[rank];
    const auto earlier = lines.first(rank);
    if (std::ranges::find(earlier, line.client_slot, &BuyingStoreSaleLine::client_slot) !=
        earlier.end()) {
      flag(BuyingStoreSaleFault::kDuplicateSlot, rank);
    }
    if (line.amount == 0) {
      flag(BuyingStoreSaleFault::kZeroAmount, rank);
    }

    const InventoryItem* item = ItemAt(bag, line.client_slot);
    if (item == nullptr || item->item_id != line.item_id || item->amount < line.amount) {
      flag(BuyingStoreSaleFault::kNotInBag, rank);
    } else {
      if (HasAnyCard(*item)) {
        flag(BuyingStoreSaleFault::kCarded, rank);
      }
      if (item->bind_on_equip == kItemBoundToOwner) {
        flag(BuyingStoreSaleFault::kBound, rank);
      }
      if (item->rented()) {
        flag(BuyingStoreSaleFault::kRented, rank);
      }
      if (item->equipped()) {
        flag(BuyingStoreSaleFault::kEquipped, rank);
      }
    }

    const BuyingStoreLine* wanted = WantedLineOf(store, line.item_id);
    if (wanted == nullptr || wanted->wanted == 0) {
      flag(BuyingStoreSaleFault::kNotWanted, rank);
      continue;
    }
    std::uint32_t& sold = sold_by_item[line.item_id];
    sold += line.amount;
    if (sold > wanted->wanted) {
      flag(BuyingStoreSaleFault::kAboveWanted, rank);
    }
  }

  const std::int64_t total = BuyingStoreSaleTotal(store, lines);
  if (total > store.zeny_limit) {
    flag(BuyingStoreSaleFault::kAboveBuyerLimit);
  }
  if (zeny + total > kBuyingStoreMostZeny) {
    flag(BuyingStoreSaleFault::kAboveMyPurse);
  }
  return problems;
}

}  // namespace ro::sim
