// L'ÉCHOPPE D'ACHAT VUE DE L'ÉCRAN : ce qu'on propose au joueur, les mots que
// disent les codes du serveur, et la ligne d'une vente confirmée.
//
// 🔴 UN CODE QUE LE CLIENT D'ORIGINE NE DIT PAS NE DOIT PAS SE TAIRE : il rend
// un mot vide, et l'étage écrit alors le code lui-même.

#include "game/buying_store_rules.h"

#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/vfs/item_words.h"

using moonclient::BuyingStoreDraftCandidates;
using moonclient::BuyingStoreSaleCandidates;
using ro::sim::BuyingStoreSaleFault;
using ro::sim::InventoryItem;
using ro::vfs::TradeWord;

namespace {

constexpr std::uint32_t kJellopy = 909;
constexpr std::uint32_t kKnife = 1201;
constexpr std::uint32_t kCard = 4001;

InventoryItem Pile(std::uint16_t slot, std::uint32_t item_id, std::int16_t amount) {
  InventoryItem item;
  item.client_slot = slot;
  item.item_id = item_id;
  item.amount = amount;
  item.identified = true;
  return item;
}

void Teach(ro::vfs::ItemWords& words, TradeWord word, const char* text) {
  words.trade.texts[static_cast<std::size_t>(word)] = text;
}

}  // namespace

TEST_CASE("Échoppe d'achat : chaque code connu a son mot, les autres aucun", "[buying_store]") {
  CHECK(moonclient::WordOfOpenRefusal(1) == TradeWord::kBuyingStoreOpenFailed);
  CHECK(moonclient::WordOfOpenRefusal(2) == TradeWord::kBuyingStoreOverweight);
  CHECK(moonclient::WordOfOpenRefusal(9) == TradeWord::kNoSaleInformation);
  CHECK(moonclient::WordOfOpenRefusal(10) == TradeWord::kShopBadPosition);
  CHECK_FALSE(moonclient::WordOfOpenRefusal(8).has_value());

  CHECK(moonclient::WordOfEnd(3) == TradeWord::kBuyingStoreLimitReached);
  CHECK(moonclient::WordOfEnd(4) == TradeWord::kBuyingStoreAllBought);
  CHECK_FALSE(moonclient::WordOfEnd(5).has_value());

  CHECK(moonclient::WordOfSaleRefusal(5) == TradeWord::kDealFailed);
  CHECK(moonclient::WordOfSaleRefusal(6) == TradeWord::kBuyingStoreOverCount);
  CHECK(moonclient::WordOfSaleRefusal(7) == TradeWord::kBuyingStoreBuyerBroke);
  CHECK_FALSE(moonclient::WordOfSaleRefusal(4).has_value());
}

TEST_CASE("Échoppe d'achat : la ligne d'une vente, et un gabarit qui ne tient pas",
          "[buying_store]") {
  ro::vfs::ItemWords words;
  Teach(words, TradeWord::kBuyingStoreSold, "%s x %d : %d zeny");
  const ro::sim::BuyingStoreSale sale{
      .client_slot = 4, .item_id = kJellopy, .amount = 3, .unit_price = 70, .zeny = 210};
  CHECK(moonclient::SoldLineText(words, "Jellopy", sale) == "Jellopy x 3 : 210 zeny");

  // UN GABARIT D'UNE AUTRE FORME NE S'ÉCRIT PAS : il lirait n'importe quoi.
  Teach(words, TradeWord::kBuyingStoreSold, "%d %s");
  CHECK(moonclient::SoldLineText(words, "Jellopy", sale).empty());

  // Un texte sans conversion se montre tel quel, un entier à placer aussi.
  Teach(words, TradeWord::kBuyingStoreOpenFailed, "Failed to open buying store.");
  CHECK(moonclient::PrintedWord(words, TradeWord::kBuyingStoreOpenFailed, 0) ==
        "Failed to open buying store.");
  Teach(words, TradeWord::kBuyingStoreOverweight, "Over by %d.");
  CHECK(moonclient::PrintedWord(words, TradeWord::kBuyingStoreOverweight, 12) == "Over by 12.");
  Teach(words, TradeWord::kBuyingStoreOverweight, "Over %s.");
  CHECK(moonclient::PrintedWord(words, TradeWord::kBuyingStoreOverweight, 12).empty());
}

TEST_CASE("Échoppe d'achat : l'heure d'un achat", "[buying_store]") {
  std::tm local{};
  local.tm_mday = 1;
  local.tm_mon = 9;
  local.tm_hour = 7;
  local.tm_min = 5;
  local.tm_sec = 3;
  CHECK(moonclient::ClockText(local) == "01/10 - 07:05:03");
  CHECK(moonclient::LocalClockText(0).size() == std::strlen("01/10 - 07:05:03"));
}

TEST_CASE("Échoppe d'achat : la préparation propose le sac, un item par identifiant",
          "[buying_store]") {
  std::vector<InventoryItem> bag = {Pile(9, kJellopy, 40), Pile(3, kJellopy, 5), Pile(4, kKnife, 1),
                                    Pile(6, kKnife, 1)};
  // Le couteau porté ne compte pas ; l'autre, si.
  bag[2].worn_at = 2;
  const auto candidates = BuyingStoreDraftCandidates(bag);
  REQUIRE(candidates.size() == 2);
  // LA PREMIÈRE PILE, dans l'ordre du sac : c'est celle que le serveur compte.
  CHECK(candidates[0].item_id == kJellopy);
  CHECK(candidates[0].client_slot == 3);
  CHECK(candidates[0].owned == 5);
  CHECK(candidates[1].item_id == kKnife);
  CHECK(candidates[1].client_slot == 6);
}

TEST_CASE("Échoppe d'achat : la table des propriétés d'item écarte ce que le serveur refuse",
          "[buying_store]") {
  const std::vector<InventoryItem> bag = {Pile(3, kJellopy, 5), Pile(4, kKnife, 1),
                                          Pile(5, kCard, 1)};
  // La table n'accepte en échoppe que le jellopy et la carte.
  const auto properties =
      ro::vfs::ItemProperties::Known({kKnife}, {kJellopy, kCard}, {{kJellopy, 10}, {kKnife, 400}});

  const auto candidates = BuyingStoreDraftCandidates(bag, &properties);
  REQUIRE(candidates.size() == 2);
  CHECK(candidates[0].item_id == kJellopy);
  CHECK(candidates[1].item_id == kCard);

  // Table absente, ou inconnue : tout le sac, comme sans elle.
  CHECK(BuyingStoreDraftCandidates(bag, nullptr).size() == 3);
  const ro::vfs::ItemProperties unknown;
  CHECK(BuyingStoreDraftCandidates(bag, &unknown).size() == 3);

  // Les faits de chaque ligne, dans l'ordre des lignes ; un item sans poids
  // dans la table pèse 0.
  const std::vector<ro::sim::BuyingStoreWish> lines = {{.item_id = kKnife, .amount = 1, .price = 1},
                                                       {.item_id = kCard, .amount = 2, .price = 1}};
  const auto facts = moonclient::BuyingStoreFactsOf(lines, &properties);
  REQUIRE(facts.size() == 2);
  CHECK(facts[0].allowed == false);
  CHECK(facts[0].unit_weight == 400U);
  CHECK(facts[1].allowed == true);
  CHECK(facts[1].unit_weight == 0U);
  CHECK(moonclient::BuyingStoreFactsOf(lines, nullptr).empty());
  CHECK(moonclient::BuyingStoreFactsOf(lines, &unknown).empty());
}

TEST_CASE("Échoppe d'achat : une pile sertie reste proposée, grisée avec sa raison",
          "[buying_store]") {
  ro::sim::VisitedBuyingStore store;
  store.buyer_account_id = 150000;
  store.zeny_limit = 1'000;
  store.lines = {{.item_id = kJellopy, .wanted = 10, .price = 5},
                 {.item_id = kKnife, .wanted = 1, .price = 50},
                 {.item_id = 502, .wanted = 0, .price = 9}};
  std::vector<InventoryItem> bag = {Pile(2, kJellopy, 6), Pile(5, kKnife, 1), Pile(7, kKnife, 1),
                                    Pile(8, 502, 3), Pile(9, 999, 3)};
  bag[1].cards[0] = kCard;
  bag[2].worn_at = 2;

  const auto candidates = BuyingStoreSaleCandidates(store, bag);
  REQUIRE(candidates.size() == 3);
  CHECK(candidates[0].client_slot == 2);
  CHECK_FALSE(candidates[0].barred.has_value());
  CHECK(candidates[0].unit_price == 5);
  CHECK(candidates[1].client_slot == 5);
  CHECK(candidates[1].barred == BuyingStoreSaleFault::kCarded);
  CHECK(candidates[2].client_slot == 8);
  CHECK(candidates[2].barred == BuyingStoreSaleFault::kNotWanted);
}

TEST_CASE("Échoppe d'achat : chaque défaut a ses mots", "[buying_store]") {
  for (int fault = 0;
       fault <= static_cast<int>(ro::sim::BuyingStoreDraftFault::kLimitBelowTotalCost); ++fault) {
    CHECK(std::strlen(
              moonclient::DraftFaultText(static_cast<ro::sim::BuyingStoreDraftFault>(fault))) > 0);
  }
  for (int fault = 0; fault <= static_cast<int>(BuyingStoreSaleFault::kAboveMyPurse); ++fault) {
    CHECK(std::strlen(moonclient::SaleFaultText(static_cast<BuyingStoreSaleFault>(fault))) > 0);
  }
}
