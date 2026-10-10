// Le troc. Voir l'en-tête pour la fermeture due, l'achat qui ferme, et la façon
// dont les devises se comptent.

#include "ro/sim/barter.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "ro/sim/inventory.h"
#include "ro/sim/player_status.h"

namespace ro::sim {
namespace {

// CE QUI NE S'EMPILE PAS, par les types que le fil porte : armure, arme,
// armure de familier (`item_data::isStackable`, `src/map/itemdb.cpp`). L'œuf et
// l'équipement d'ombre, que le serveur tient aussi pour non empilables,
// arrivent repliés sur l'armure ou l'arme (`itemtype()`).
constexpr std::array<std::uint16_t, 3> kUnstackableTypes = {4, 5, 8};

[[nodiscard]] bool TypeStacks(std::uint16_t type) noexcept {
  return std::ranges::find(kUnstackableTypes, type) == kUnstackableTypes.end();
}

[[nodiscard]] BarterCartCheck Refuse(BarterCartRefusal refusal, std::size_t cart_line,
                                     std::uint32_t item_id) {
  return BarterCartCheck{.refusal = refusal, .cart_line = cart_line, .item_id = item_id};
}

[[nodiscard]] std::size_t MostLines(BarterGeneration generation) noexcept {
  return generation == BarterGeneration::kSimple ? kBarterMostSimpleLines
                                                 : kBarterMostExpandedLines;
}

// Ce qui refuse sans le sac ni la fiche : le silence du serveur.
[[nodiscard]] BarterCartCheck CheckShape(const Barter& barter,
                                         std::span<const BarterCartLine> cart) {
  if (!barter.open()) {
    return Refuse(BarterCartRefusal::kNotOpen, 0, 0);
  }
  if (cart.empty()) {
    return Refuse(BarterCartRefusal::kEmptyCart, 0, 0);
  }
  if (cart.size() > MostLines(barter.generation())) {
    return Refuse(BarterCartRefusal::kTooManyLines, cart.size() - 1, 0);
  }
  for (std::size_t line = 0; line < cart.size(); ++line) {
    const BarterItem* item = barter.FindItem(cart[line].shop_index);
    if (item == nullptr) {
      return Refuse(BarterCartRefusal::kUnknownItem, line, 0);
    }
    if (cart[line].amount == 0) {
      return Refuse(BarterCartRefusal::kZeroAmount, line, item->item_id);
    }
    // `clif_parse_barter_buy` rejette deux lignes qui livrent le même objet.
    for (std::size_t earlier = 0; earlier < line; ++earlier) {
      const BarterItem* other = barter.FindItem(cart[earlier].shop_index);
      if (other != nullptr && other->item_id == item->item_id) {
        return Refuse(BarterCartRefusal::kSameItemTwice, line, item->item_id);
      }
    }
  }
  return {};
}

// ── LES DEVISES ─────────────────────────────────────────────────────────────

// Ce qu'une ligne du panier exige d'une devise, cumulé sur toutes les lignes
// qui l'exigent au même raffinement.
struct CurrencyNeed {
  std::uint32_t item_id = 0;
  std::uint16_t refine_level = kBarterAnyRefine;
  std::uint64_t amount = 0;
  // La première ligne qui l'exige : c'est elle qu'on désigne.
  std::size_t first_line = 0;
};

// Quels exemplaires on laisse compter.
struct CurrencyPool {
  bool with_worn = false;
  bool with_favorites = false;
};

[[nodiscard]] bool Counts(const InventoryItem& item, std::uint32_t item_id, CurrencyPool pool) {
  if (item.item_id != item_id || item.amount <= 0) {
    return false;
  }
  if (item.equipped() && !pool.with_worn) {
    return false;
  }
  return !item.favorite || pool.with_favorites;
}

[[nodiscard]] bool RefineFits(std::uint16_t required, std::uint8_t held) noexcept {
  return required == kBarterAnyRefine || held == required;
}

// Une devise qui s'empile : chaque besoin se prend dans la PREMIÈRE pile qui
// convient, et seulement dans celle-là (`npc_barter_purchase`).
[[nodiscard]] bool StackedSuffices(std::span<const CurrencyNeed> needs, const Inventory& inventory,
                                   CurrencyPool pool) {
  const std::span<const InventoryItem> held = inventory.items();
  std::vector<std::uint64_t> taken(held.size(), 0);
  for (const CurrencyNeed& need : needs) {
    const auto stack = std::ranges::find_if(held, [&](const InventoryItem& item) {
      return Counts(item, need.item_id, pool) && RefineFits(need.refine_level, item.refine_level);
    });
    if (stack == held.end()) {
      return false;
    }
    const auto at = static_cast<std::size_t>(stack - held.begin());
    taken[at] += need.amount;
    if (taken[at] > static_cast<std::uint64_t>(stack->amount)) {
      return false;
    }
  }
  return true;
}

// Une devise qui ne s'empile pas : un exemplaire par unité, au raffinement
// exigé ou plus haut. Les ensembles « raffiné au moins à r » s'emboîtent : le
// compte tient si, pour chaque seuil exigé, les besoins de ce seuil et des
// seuils plus hauts ne dépassent pas les exemplaires qui l'atteignent.
[[nodiscard]] bool UnstackedSuffices(std::span<const CurrencyNeed> needs,
                                     const Inventory& inventory, CurrencyPool pool) {
  for (const CurrencyNeed& threshold : needs) {
    std::uint64_t wanted = 0;
    for (const CurrencyNeed& need : needs) {
      if (need.refine_level >= threshold.refine_level) {
        wanted += need.amount;
      }
    }
    std::uint64_t available = 0;
    for (const InventoryItem& item : inventory.items()) {
      if (Counts(item, threshold.item_id, pool) && item.refine_level >= threshold.refine_level) {
        available += static_cast<std::uint64_t>(item.amount);
      }
    }
    if (wanted > available) {
      return false;
    }
  }
  return true;
}

[[nodiscard]] bool CurrencySuffices(std::span<const CurrencyNeed> needs, bool stacks,
                                    const Inventory& inventory, CurrencyPool pool) {
  return stacks ? StackedSuffices(needs, inventory, pool)
                : UnstackedSuffices(needs, inventory, pool);
}

[[nodiscard]] std::vector<CurrencyNeed> GatherNeeds(const Barter& barter,
                                                    std::span<const BarterCartLine> cart) {
  std::vector<CurrencyNeed> needs;
  for (std::size_t line = 0; line < cart.size(); ++line) {
    const BarterItem* item = barter.FindItem(cart[line].shop_index);
    for (const BarterCurrency& currency : item->currencies) {
      const std::uint64_t amount = static_cast<std::uint64_t>(currency.amount) *
                                   static_cast<std::uint64_t>(cart[line].amount);
      const auto known = std::ranges::find_if(needs, [&](const CurrencyNeed& need) {
        return need.item_id == currency.item_id && need.refine_level == currency.refine_level;
      });
      if (known != needs.end()) {
        known->amount += amount;
      } else {
        needs.push_back(CurrencyNeed{.item_id = currency.item_id,
                                     .refine_level = currency.refine_level,
                                     .amount = amount,
                                     .first_line = line});
      }
    }
  }
  return needs;
}

[[nodiscard]] BarterCartCheck CheckCurrencies(const Barter& barter,
                                              std::span<const BarterCartLine> cart,
                                              const Inventory& inventory) {
  const std::vector<CurrencyNeed> needs = GatherNeeds(barter, cart);
  std::vector<std::uint32_t> judged;
  for (const CurrencyNeed& first : needs) {
    if (std::ranges::find(judged, first.item_id) != judged.end()) {
      continue;
    }
    judged.push_back(first.item_id);
    std::vector<CurrencyNeed> same_item;
    std::ranges::copy_if(needs, std::back_inserter(same_item),
                         [&](const CurrencyNeed& need) { return need.item_id == first.item_id; });
    // CE QUI S'EMPILE SE LIT DANS LE SAC, pas dans la devise : le troc simple ne
    // dit pas sa famille, et c'est de toute façon l'objet tenu qui fait foi.
    const auto any_copy = std::ranges::find_if(inventory.items(), [&](const InventoryItem& item) {
      return item.item_id == first.item_id;
    });
    if (any_copy == inventory.items().end()) {
      return Refuse(BarterCartRefusal::kNotEnoughCurrency, first.first_line, first.item_id);
    }
    const bool stacks = TypeStacks(any_copy->type);
    if (CurrencySuffices(same_item, stacks, inventory, CurrencyPool{})) {
      continue;
    }
    if (CurrencySuffices(same_item, stacks, inventory, CurrencyPool{.with_worn = true})) {
      return Refuse(BarterCartRefusal::kCurrencyWorn, first.first_line, first.item_id);
    }
    if (CurrencySuffices(same_item, stacks, inventory, CurrencyPool{.with_favorites = true})) {
      return Refuse(BarterCartRefusal::kCurrencyFavorite, first.first_line, first.item_id);
    }
    return Refuse(BarterCartRefusal::kNotEnoughCurrency, first.first_line, first.item_id);
  }
  return {};
}

// ── LE PANIER ENTIER ────────────────────────────────────────────────────────

[[nodiscard]] BarterCartCheck CheckLines(const Barter& barter,
                                         std::span<const BarterCartLine> cart) {
  for (std::size_t line = 0; line < cart.size(); ++line) {
    const BarterItem& item = *barter.FindItem(cart[line].shop_index);
    if (!TypeStacks(item.type) && cart[line].amount > 1) {
      return Refuse(BarterCartRefusal::kGearOneAtATime, line, item.item_id);
    }
    if (cart[line].amount > kBarterMostPerLine) {
      return Refuse(BarterCartRefusal::kOverLineLimit, line, item.item_id);
    }
    if (!item.unlimited() && cart[line].amount > item.stock) {
      return Refuse(BarterCartRefusal::kOutOfStock, line, item.item_id);
    }
  }
  return {};
}

[[nodiscard]] BarterCartCheck CheckWeight(const Barter& barter,
                                          std::span<const BarterCartLine> cart,
                                          const PlayerStatus& status) {
  if (status.weight < 0 || status.max_weight < 0) {
    return {};
  }
  std::int64_t load = status.weight;
  for (std::size_t line = 0; line < cart.size(); ++line) {
    const BarterItem& item = *barter.FindItem(cart[line].shop_index);
    load += static_cast<std::int64_t>(item.weight) * static_cast<std::int64_t>(cart[line].amount);
    if (load > status.max_weight) {
      return Refuse(BarterCartRefusal::kOverWeight, line, item.item_id);
    }
  }
  return {};
}

[[nodiscard]] BarterCartCheck CheckZeny(const Barter& barter, std::span<const BarterCartLine> cart,
                                        const PlayerStatus& status) {
  if (status.zeny < 0) {
    return {};
  }
  std::int64_t spent = 0;
  for (std::size_t line = 0; line < cart.size(); ++line) {
    const BarterItem& item = *barter.FindItem(cart[line].shop_index);
    spent += static_cast<std::int64_t>(item.zeny) * static_cast<std::int64_t>(cart[line].amount);
    if (spent > status.zeny) {
      return Refuse(BarterCartRefusal::kNotEnoughZeny, line, item.item_id);
    }
  }
  return {};
}

[[nodiscard]] BarterCartCheck CheckWholeCart(const Barter& barter,
                                             std::span<const BarterCartLine> cart,
                                             const Inventory& inventory,
                                             const PlayerStatus& status) {
  // ⚠ DANS L'ORDRE, ET UN À UN : les contrôles suivants supposent que la forme
  // a tenu, chaque ligne désignant un article connu.
  const BarterCartCheck shape = CheckShape(barter, cart);
  if (!shape.passed()) {
    return shape;
  }
  const BarterCartCheck lines = CheckLines(barter, cart);
  if (!lines.passed()) {
    return lines;
  }
  const BarterCartCheck weight = CheckWeight(barter, cart, status);
  if (!weight.passed()) {
    return weight;
  }
  const BarterCartCheck currencies = CheckCurrencies(barter, cart, inventory);
  if (!currencies.passed()) {
    return currencies;
  }
  return CheckZeny(barter, cart, status);
}

}  // namespace

// ── LE MODÈLE ───────────────────────────────────────────────────────────────

void Barter::TellOpening(BarterGeneration generation, std::vector<BarterItem> items) {
  if (open_ && generation_ != generation) {
    (generation_ == BarterGeneration::kSimple ? simple_farewell_due_ : expanded_farewell_due_) =
        true;
  }
  // La liste prouve que le serveur a désarmé puis réarmé l'état de cette
  // génération : une fermeture encore due la refermerait.
  (generation == BarterGeneration::kSimple ? simple_farewell_due_ : expanded_farewell_due_) = false;
  open_ = true;
  generation_ = generation;
  items_ = std::move(items);
  ++opening_seq_;
  cart_.clear();
  ++cart_seq_;
  awaiting_result_ = false;
  sent_cart_.clear();
}

const BarterItem* Barter::FindItem(std::uint32_t shop_index) const noexcept {
  const auto found = std::ranges::find_if(
      items_, [&](const BarterItem& item) { return item.shop_index == shop_index; });
  return found == items_.end() ? nullptr : &*found;
}

std::uint32_t Barter::InCart(std::uint32_t shop_index) const noexcept {
  const auto line = std::ranges::find_if(
      cart_, [&](const BarterCartLine& entry) { return entry.shop_index == shop_index; });
  return line == cart_.end() ? 0 : line->amount;
}

std::uint32_t Barter::RemainingStock(const BarterItem& item) const noexcept {
  if (item.unlimited()) {
    return kBarterUnlimitedStock;
  }
  const std::uint32_t carted = InCart(item.shop_index);
  return carted >= item.stock ? 0 : item.stock - carted;
}

bool Barter::PutInCart(std::uint32_t shop_index, std::uint32_t amount) {
  if (!open_ || amount == 0 || FindItem(shop_index) == nullptr) {
    return false;
  }
  const auto line = std::ranges::find_if(
      cart_, [&](const BarterCartLine& entry) { return entry.shop_index == shop_index; });
  if (line == cart_.end()) {
    cart_.push_back(BarterCartLine{.shop_index = shop_index, .amount = amount});
  } else {
    const std::uint64_t total = std::uint64_t{line->amount} + amount;
    line->amount = static_cast<std::uint32_t>(
        std::min<std::uint64_t>(total, std::numeric_limits<std::uint32_t>::max()));
  }
  ++cart_seq_;
  return true;
}

bool Barter::SetCartAmount(std::uint32_t shop_index, std::uint32_t amount) {
  const auto line = std::ranges::find_if(
      cart_, [&](const BarterCartLine& entry) { return entry.shop_index == shop_index; });
  if (line == cart_.end()) {
    return false;
  }
  if (amount == 0) {
    cart_.erase(line);
  } else {
    line->amount = amount;
  }
  ++cart_seq_;
  return true;
}

void Barter::ClearCart() noexcept {
  if (!cart_.empty()) {
    cart_.clear();
    ++cart_seq_;
  }
}

void Barter::MarkPurchaseSent() {
  if (!open_) {
    return;
  }
  sent_cart_ = cart_;
  awaiting_result_ = true;
  Close();
}

void Barter::TellResult(std::uint8_t code) {
  result_ = code;
  ++result_seq_;
  awaiting_result_ = false;
}

void Barter::Close() {
  if (!open_) {
    return;
  }
  (generation_ == BarterGeneration::kSimple ? simple_farewell_due_ : expanded_farewell_due_) = true;
  open_ = false;
  ClearCart();
}

void Barter::DropEverythingOpen() {
  open_ = false;
  ClearCart();
  awaiting_result_ = false;
  simple_farewell_due_ = false;
  expanded_farewell_due_ = false;
}

void Barter::CloseForMapChange() { DropEverythingOpen(); }

void Barter::CloseSilently() { DropEverythingOpen(); }

std::optional<BarterGeneration> Barter::TakeFarewell() noexcept {
  if (simple_farewell_due_) {
    simple_farewell_due_ = false;
    return BarterGeneration::kSimple;
  }
  if (expanded_farewell_due_) {
    expanded_farewell_due_ = false;
    return BarterGeneration::kExpanded;
  }
  return std::nullopt;
}

// ── LA VALIDATION ───────────────────────────────────────────────────────────

BarterCartCheck CheckBarterCartShape(const Barter& barter) {
  return CheckShape(barter, barter.cart());
}

BarterCartCheck CheckBarterCart(const Barter& barter, const Inventory& inventory,
                                const PlayerStatus& status) {
  return CheckWholeCart(barter, barter.cart(), inventory, status);
}

BarterCartCheck CheckBarterAddition(const Barter& barter, std::uint32_t shop_index,
                                    std::uint32_t amount, const Inventory& inventory,
                                    const PlayerStatus& status) {
  std::vector<BarterCartLine> cart(barter.cart().begin(), barter.cart().end());
  const auto line = std::ranges::find_if(
      cart, [&](const BarterCartLine& entry) { return entry.shop_index == shop_index; });
  const std::size_t line_index = static_cast<std::size_t>(line - cart.begin());
  const BarterItem* item = barter.FindItem(shop_index);
  if (item == nullptr) {
    return Refuse(BarterCartRefusal::kUnknownItem, line_index, 0);
  }
  if (amount == 0) {
    return Refuse(BarterCartRefusal::kZeroAmount, line_index, item->item_id);
  }
  if (!TypeStacks(item->type) && line != cart.end()) {
    return Refuse(BarterCartRefusal::kGearAlreadyInCart, line_index, item->item_id);
  }
  if (line == cart.end()) {
    cart.push_back(BarterCartLine{.shop_index = shop_index, .amount = amount});
  } else {
    line->amount = static_cast<std::uint32_t>(std::min<std::uint64_t>(
        std::uint64_t{line->amount} + amount, std::numeric_limits<std::uint32_t>::max()));
  }
  return CheckWholeCart(barter, cart, inventory, status);
}

BarterCurrencyHolding HoldBarterCurrency(const Inventory& inventory, std::uint32_t item_id,
                                         std::uint16_t refine_level) {
  BarterCurrencyHolding holding;
  const std::span<const InventoryItem> held = inventory.items();
  const auto any_copy = std::ranges::find_if(
      held, [&](const InventoryItem& item) { return item.item_id == item_id; });
  if (any_copy == held.end()) {
    return holding;
  }
  if (TypeStacks(any_copy->type)) {
    const auto stack = std::ranges::find_if(held, [&](const InventoryItem& item) {
      return Counts(item, item_id, CurrencyPool{}) && RefineFits(refine_level, item.refine_level);
    });
    if (stack != held.end()) {
      holding.usable = static_cast<std::uint64_t>(stack->amount);
      holding.may_take_higher_refine = stack->refine_level > refine_level;
    }
    return holding;
  }
  for (const InventoryItem& item : held) {
    if (Counts(item, item_id, CurrencyPool{}) && item.refine_level >= refine_level) {
      holding.usable += static_cast<std::uint64_t>(item.amount);
      holding.may_take_higher_refine |= item.refine_level > refine_level;
    }
  }
  return holding;
}

}  // namespace ro::sim
