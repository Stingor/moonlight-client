// L'échoppe d'achat vue de l'écran. Voir l'en-tête.

#include "game/buying_store_rules.h"

#include <algorithm>
#include <array>
#include <cstdio>

#include "ro/core/i18n.h"

namespace moonclient {
namespace {

using ro::sim::BuyingStoreDraftFault;
using ro::sim::BuyingStoreSaleFault;
using ro::vfs::TradeWord;

// Le mois d'un `std::tm` compte depuis zéro.
constexpr int kFirstMonth = 1;

// Les refus qui tiennent à la PILE elle-même, et non à la quantité ou aux
// bourses : ceux-là grisent une ligne de la liste.
constexpr std::array<BuyingStoreSaleFault, 3> kPileFaults = {
    BuyingStoreSaleFault::kCarded, BuyingStoreSaleFault::kBound, BuyingStoreSaleFault::kRented};

}  // namespace

std::optional<TradeWord> WordOfOpenRefusal(std::uint16_t code) noexcept {
  switch (code) {
    case ro::sim::kBuyingStoreOpenFailed:
      return TradeWord::kBuyingStoreOpenFailed;
    case ro::sim::kBuyingStoreOpenOverweight:
      return TradeWord::kBuyingStoreOverweight;
    case kBuyingStoreOpenNoStore:
      return TradeWord::kNoSaleInformation;
    case kBuyingStoreOpenBadPosition:
      return TradeWord::kShopBadPosition;
    default:
      return std::nullopt;
  }
}

std::optional<TradeWord> WordOfEnd(std::uint16_t code) noexcept {
  switch (code) {
    case ro::sim::kBuyingStoreEndLimitReached:
      return TradeWord::kBuyingStoreLimitReached;
    case ro::sim::kBuyingStoreEndAllBought:
      return TradeWord::kBuyingStoreAllBought;
    default:
      return std::nullopt;
  }
}

std::optional<TradeWord> WordOfSaleRefusal(std::uint16_t code) noexcept {
  switch (code) {
    case ro::sim::kBuyingStoreSaleFailed:
      return TradeWord::kDealFailed;
    case ro::sim::kBuyingStoreSaleOverCount:
      return TradeWord::kBuyingStoreOverCount;
    case ro::sim::kBuyingStoreSaleBuyerLacksZeny:
      return TradeWord::kBuyingStoreBuyerBroke;
    default:
      return std::nullopt;
  }
}

std::string PrintedWord(const ro::vfs::ItemWords& words, TradeWord word, std::int64_t number) {
  std::string printed = ro::vfs::TradeTextWith(words, word, number);
  if (printed.empty()) {
    // UN TEXTE SANS CONVERSION se montre tel quel ; un gabarit d'une autre forme
    // ne se montre pas du tout — ses conversions s'écriraient à l'écran.
    const std::string_view raw = ro::vfs::TradeText(words, word);
    if (raw.find('%') == std::string_view::npos) {
      printed = std::string(raw);
    }
  }
  return printed;
}

std::string SoldLineText(const ro::vfs::ItemWords& words, std::string_view item_name,
                         const ro::sim::BuyingStoreSale& sale) {
  return ro::vfs::TradeTextWith(words, TradeWord::kBuyingStoreSold, item_name, sale.amount,
                                sale.zeny);
}

std::string ClockText(const std::tm& local) {
  std::array<char, 32> text{};
  std::snprintf(text.data(), text.size(), "%02d/%02d - %02d:%02d:%02d", local.tm_mday,
                local.tm_mon + kFirstMonth, local.tm_hour, local.tm_min, local.tm_sec);
  return text.data();
}

std::string LocalClockText(std::uint32_t unix_s) {
  const auto seconds = static_cast<std::time_t>(unix_s);
  std::tm local{};
#ifdef _WIN32
  if (localtime_s(&local, &seconds) != 0) {
    return {};
  }
#else
  if (localtime_r(&seconds, &local) == nullptr) {
    return {};
  }
#endif
  return ClockText(local);
}

std::vector<BuyingStoreDraftCandidate> BuyingStoreDraftCandidates(
    std::span<const ro::sim::InventoryItem> bag, const ro::vfs::ItemProperties* properties) {
  std::vector<const ro::sim::InventoryItem*> order;
  order.reserve(bag.size());
  for (const ro::sim::InventoryItem& item : bag) {
    order.push_back(&item);
  }
  std::ranges::sort(order, {}, &ro::sim::InventoryItem::client_slot);

  std::vector<BuyingStoreDraftCandidate> candidates;
  for (const ro::sim::InventoryItem* item : order) {
    if (item->equipped() || item->amount <= 0) {
      continue;
    }
    if (properties != nullptr && properties->BuyingStoreAllowed(item->item_id) == false) {
      continue;
    }
    if (std::ranges::find(candidates, item->item_id, &BuyingStoreDraftCandidate::item_id) !=
        candidates.end()) {
      continue;
    }
    candidates.push_back(BuyingStoreDraftCandidate{
        .item_id = item->item_id, .client_slot = item->client_slot, .owned = item->amount});
  }
  return candidates;
}

std::vector<ro::sim::BuyingStoreItemFacts> BuyingStoreFactsOf(
    std::span<const ro::sim::BuyingStoreWish> lines, const ro::vfs::ItemProperties* properties) {
  std::vector<ro::sim::BuyingStoreItemFacts> facts;
  if (properties == nullptr || !properties->known()) {
    return facts;
  }
  facts.reserve(lines.size());
  for (const ro::sim::BuyingStoreWish& line : lines) {
    facts.push_back({.allowed = properties->BuyingStoreAllowed(line.item_id),
                     .unit_weight = properties->WeightTenthsOf(line.item_id)});
  }
  return facts;
}

std::vector<BuyingStoreSaleCandidate> BuyingStoreSaleCandidates(
    const ro::sim::VisitedBuyingStore& store, std::span<const ro::sim::InventoryItem> bag) {
  std::vector<BuyingStoreSaleCandidate> candidates;
  for (const ro::sim::InventoryItem& item : bag) {
    if (item.equipped() || item.amount <= 0) {
      continue;
    }
    const auto wanted =
        std::ranges::find(store.lines, item.item_id, &ro::sim::BuyingStoreLine::item_id);
    if (wanted == store.lines.end()) {
      continue;
    }
    BuyingStoreSaleCandidate candidate{.client_slot = item.client_slot,
                                       .item_id = item.item_id,
                                       .owned = item.amount,
                                       .unit_price = wanted->price};
    if (wanted->wanted == 0) {
      candidate.barred = BuyingStoreSaleFault::kNotWanted;
    } else {
      // LES RÈGLES DE LA PILE SONT CELLES DE LA VENTE : une ligne d'un
      // exemplaire, jugée comme le serait la vraie.
      const std::array<ro::sim::BuyingStoreSaleLine, 1> probe = {ro::sim::BuyingStoreSaleLine{
          .client_slot = item.client_slot, .item_id = item.item_id, .amount = 1}};
      for (const ro::sim::BuyingStoreSaleProblem& problem :
           ro::sim::CheckBuyingStoreSale(store, probe, bag, 0)) {
        if (std::ranges::find(kPileFaults, problem.fault) != kPileFaults.end()) {
          candidate.barred = problem.fault;
          break;
        }
      }
    }
    candidates.push_back(candidate);
  }
  std::ranges::sort(candidates, {}, &BuyingStoreSaleCandidate::client_slot);
  return candidates;
}

const char* DraftFaultText(BuyingStoreDraftFault fault) {
  switch (fault) {
    case BuyingStoreDraftFault::kNoLine:
      return ro::core::Tr("Pose au moins un item.");
    case BuyingStoreDraftFault::kTooManyLines:
      return ro::core::Tr("Plus de lignes que le skill n'en permet.");
    case BuyingStoreDraftFault::kEmptyTitle:
      return ro::core::Tr("Donne un nom au buying store.");
    case BuyingStoreDraftFault::kDuplicateItem:
      return ro::core::Tr("Le même item est sur deux lignes.");
    case BuyingStoreDraftFault::kZeroAmount:
      return ro::core::Tr("Une ligne est à quantité 0.");
    case BuyingStoreDraftFault::kZeroPrice:
      return ro::core::Tr("Un item est à prix 0 : le serveur le refuse.");
    case BuyingStoreDraftFault::kPriceAboveMost:
      return ro::core::Tr("Prix maximal : 99,990,000z par item.");
    case BuyingStoreDraftFault::kNotInBag:
      return ro::core::Tr("Il faut en posséder au moins un dans l'inventory.");
    case BuyingStoreDraftFault::kOwnedAboveMost:
      return ro::core::Tr("Ce qu'on a, plus ce qu'on veut, dépasserait 9,999 exemplaires.");
    case BuyingStoreDraftFault::kLimitNotPositive:
      return ro::core::Tr("Renseigne la limite de zeny à dépenser.");
    case BuyingStoreDraftFault::kLimitAboveZeny:
      return ro::core::Tr("La limite dépasse tes zeny.");
    case BuyingStoreDraftFault::kNotForBuyingStore:
      return ro::core::Tr("Cet item ne s'achète pas en buying store.");
    case BuyingStoreDraftFault::kOverweight:
      return ro::core::Tr(
          "Trop lourd : tout acheter mettrait le personnage en surcharge, et le serveur refuse.");
    case BuyingStoreDraftFault::kLimitBelowTotalCost:
      return ro::core::Tr(
          "La limite ne paie pas tout : le buying store fermera quand elle sera épuisée.");
  }
  return "";
}

const char* SaleFaultText(BuyingStoreSaleFault fault) {
  switch (fault) {
    case BuyingStoreSaleFault::kNoLine:
      return ro::core::Tr("Rien à vendre.");
    case BuyingStoreSaleFault::kTooManyLines:
      return ro::core::Tr("Trop de lignes pour une seule vente.");
    case BuyingStoreSaleFault::kDuplicateSlot:
      return ro::core::Tr("La même pile est sur deux lignes.");
    case BuyingStoreSaleFault::kZeroAmount:
      return ro::core::Tr("Une ligne est à quantité 0.");
    case BuyingStoreSaleFault::kNotInBag:
      return ro::core::Tr("Un item n'est plus dans l'inventory, ou pas assez.");
    case BuyingStoreSaleFault::kNotWanted:
      return ro::core::Tr("L'acheteur n'en veut plus.");
    case BuyingStoreSaleFault::kAboveWanted:
      return ro::core::Tr("Plus que ce que l'acheteur veut encore.");
    case BuyingStoreSaleFault::kCarded:
      return ro::core::Tr("Item serti : le serveur refuse de le vendre.");
    case BuyingStoreSaleFault::kBound:
      return ro::core::Tr("Item lié au personnage.");
    case BuyingStoreSaleFault::kRented:
      return ro::core::Tr("Item loué.");
    case BuyingStoreSaleFault::kEquipped:
      return ro::core::Tr("Item équipé.");
    case BuyingStoreSaleFault::kAboveBuyerLimit:
      return ro::core::Tr("L'acheteur n'a pas de quoi tout payer.");
    case BuyingStoreSaleFault::kAboveMyPurse:
      return ro::core::Tr("Cette vente dépasserait le zeny maximum.");
  }
  return "";
}

}  // namespace moonclient
