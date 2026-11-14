// LA BOUTIQUE D'UN PNJ : ses ouvertures, son réarmement, sa fermeture — et le
// panier jugé avant que le serveur ne le juge.
//
// 🔴 CE BANC PORTE CE QUE LE SERVEUR FAIT SANS LE DIRE :
//
//   · il oublie la boutique à zeny après CHAQUE transaction ;
//   · une boutique ouverte par un script n'est jamais nommée, et ne sert donc
//     qu'une fois ;
//   · un objet non empilable demandé à plusieurs est ramené à un ;
//   · une vente qui ferait déborder la bourse est créditée tronquée.

#include "ro/sim/npc_shop.h"

#include <array>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/inventory.h"
#include "ro/sim/npc_shop_cart.h"
#include "ro/sim/player_status.h"

using ro::sim::CheckNpcShopBuy;
using ro::sim::CheckNpcShopSell;
using ro::sim::InventoryItem;
using ro::sim::kUnknownStat;
using ro::sim::NpcShop;
using ro::sim::NpcShopBuyLine;
using ro::sim::NpcShopDeals;
using ro::sim::NpcShopFarewell;
using ro::sim::NpcShopIssue;
using ro::sim::NpcShopKind;
using ro::sim::NpcShopOffer;
using ro::sim::NpcShopSellLine;
using ro::sim::NpcShopSellOffer;
using ro::sim::NpcShopSide;

namespace {

constexpr std::uint32_t kShopNpc = 110000123;
constexpr std::uint32_t kRedPotion = 501;
constexpr std::uint32_t kKnife = 1201;
constexpr std::uint8_t kHealingType = 0;

std::vector<NpcShopOffer> PotionAndKnife() {
  return {
      NpcShopOffer{.item_id = kRedPotion, .base_price = 50, .price = 38, .item_type = kHealingType},
      NpcShopOffer{
          .item_id = kKnife, .base_price = 50, .price = 38, .item_type = ro::sim::kItemTypeWeapon}};
}

InventoryItem Carried(std::uint16_t slot, std::uint32_t item_id, std::int16_t amount) {
  InventoryItem item;
  item.client_slot = slot;
  item.item_id = item_id;
  item.amount = amount;
  return item;
}

}  // namespace

TEST_CASE("Boutique PNJ : le choix ouvre sans armer, la liste arme", "[sim][npc_shop]") {
  NpcShop shop;
  shop.TellDealChoice(kShopNpc);
  CHECK(shop.open());
  CHECK(shop.kind() == NpcShopKind::kZeny);
  CHECK(shop.deals() == NpcShopDeals::kBuyAndSell);
  CHECK(shop.npc_id() == kShopNpc);
  CHECK_FALSE(shop.armed());

  const std::vector<NpcShopOffer> offers = PotionAndKnife();
  shop.TellBuyList(offers);
  CHECK(shop.armed());
  CHECK(shop.buy_seq() == 1);
  // La liste d'une boutique NOMMÉE ne rouvre pas : même ouverture.
  CHECK(shop.deals() == NpcShopDeals::kBuyAndSell);
  CHECK(shop.opening_seq() == 1);
}

TEST_CASE("Boutique PNJ : chaque transaction désarme la boutique à zeny", "[sim][npc_shop]") {
  NpcShop shop;
  shop.TellDealChoice(kShopNpc);
  shop.TellBuyList(PotionAndKnife());
  shop.NoteDealSent();
  CHECK_FALSE(shop.armed());
  CHECK(shop.awaiting_result());
  shop.TellBuyResult(0);
  CHECK_FALSE(shop.awaiting_result());
  CHECK(shop.outcome().seq == 1);
  CHECK(shop.outcome().side == NpcShopSide::kBuy);
  // Nommée, elle reste ouverte : on la réarmera.
  CHECK(shop.open());
}

TEST_CASE("Boutique PNJ : ouverte par un script, elle ne sert qu'une fois", "[sim][npc_shop]") {
  NpcShop shop;
  const std::vector<NpcShopSellOffer> sold = {
      NpcShopSellOffer{.client_slot = 2, .base_price = 10, .price = 12}};
  shop.TellSellList(sold);
  CHECK(shop.open());
  CHECK(shop.deals() == NpcShopDeals::kSellOnly);
  CHECK(shop.npc_id() == 0);
  CHECK(shop.single_use());
  shop.NoteDealSent();
  shop.TellSellResult(0);
  CHECK_FALSE(shop.open());
  CHECK(shop.outcome().side == NpcShopSide::kSell);
  CHECK(shop.TakeFarewell() == NpcShopFarewell::kTradeQuit);
}

TEST_CASE("Boutique PNJ : un résultat qui arrive après la fermeture se lit encore",
          "[sim][npc_shop]") {
  NpcShop shop;
  shop.TellDealChoice(kShopNpc);
  shop.TellBuyList(PotionAndKnife());
  shop.NoteDealSent();
  shop.Close();
  CHECK_FALSE(shop.open());
  shop.TellBuyResult(1);
  CHECK(shop.outcome().result == 1);
  CHECK(shop.outcome().seq == 1);
}

TEST_CASE("Boutique PNJ : le market décompte son stock au résultat", "[sim][npc_shop]") {
  NpcShop shop;
  const std::vector<NpcShopOffer> offers = {
      NpcShopOffer{.item_id = kRedPotion, .price = 40, .stock = 10, .unit_weight = 70},
      NpcShopOffer{.item_id = kKnife, .price = 90, .stock = ro::sim::kNpcShopUnlimitedStock}};
  shop.TellMarket(offers);
  CHECK(shop.kind() == NpcShopKind::kMarket);
  shop.NoteDealSent();
  CHECK(shop.armed());
  const std::array<NpcShop::MarketBought, 2> bought = {
      NpcShop::MarketBought{.item_id = kRedPotion, .amount = 4},
      NpcShop::MarketBought{.item_id = kKnife, .amount = 1}};
  shop.TellMarketResult(0, bought);
  CHECK(shop.buy_offers()[0].stock == 6);
  CHECK(shop.buy_offers()[1].stock == ro::sim::kNpcShopUnlimitedStock);
  // Un échec ne touche à rien.
  shop.TellMarketResult(0xFFFF, bought);
  CHECK(shop.buy_offers()[0].stock == 6);
  shop.Close();
  CHECK(shop.TakeFarewell() == NpcShopFarewell::kMarketClose);
  CHECK(shop.TakeFarewell() == NpcShopFarewell::kNone);
}

TEST_CASE("Boutique PNJ : changer de serveur ferme sans congé", "[sim][npc_shop]") {
  NpcShop shop;
  shop.TellPointShop(PotionAndKnife(), 300, 0);
  CHECK(shop.points() == 300);
  shop.CloseSilently();
  CHECK_FALSE(shop.open());
  CHECK(shop.TakeFarewell() == NpcShopFarewell::kNone);
}

TEST_CASE("Panier d'achat : zeny, piles et objets non empilables", "[sim][npc_shop]") {
  NpcShop shop;
  shop.TellDealChoice(kShopNpc);
  shop.TellBuyList(PotionAndKnife());
  const std::vector<InventoryItem> bag = {Carried(2, kRedPotion, 29990)};

  const std::array<NpcShopBuyLine, 1> fine = {NpcShopBuyLine{.item_id = kRedPotion, .amount = 5}};
  auto verdict = CheckNpcShopBuy(shop, fine, 1000, bag, kUnknownStat, kUnknownStat);
  CHECK(verdict.clear());
  CHECK(verdict.total == 5 * 38);
  CHECK_FALSE(verdict.weight_known);

  const std::array<NpcShopBuyLine, 1> overstack = {
      NpcShopBuyLine{.item_id = kRedPotion, .amount = 20}};
  CHECK(CheckNpcShopBuy(shop, overstack, 100000, bag, kUnknownStat, kUnknownStat)
            .Has(NpcShopIssue::kOverStack));

  const std::array<NpcShopBuyLine, 1> two_knives = {NpcShopBuyLine{.item_id = kKnife, .amount = 2}};
  CHECK(CheckNpcShopBuy(shop, two_knives, 100000, bag, kUnknownStat, kUnknownStat)
            .Has(NpcShopIssue::kOnePerPurchase));

  CHECK(CheckNpcShopBuy(shop, fine, 100, bag, kUnknownStat, kUnknownStat)
            .Has(NpcShopIssue::kLacksMoney));
  // Une bourse que le serveur n'a pas dite ne retient rien.
  CHECK(CheckNpcShopBuy(shop, fine, kUnknownStat, bag, kUnknownStat, kUnknownStat).clear());

  const std::array<NpcShopBuyLine, 1> stranger = {NpcShopBuyLine{.item_id = 999, .amount = 1}};
  CHECK(CheckNpcShopBuy(shop, stranger, 1000, bag, kUnknownStat, kUnknownStat)
            .Has(NpcShopIssue::kNotOffered));
  CHECK(CheckNpcShopBuy(shop, {}, 1000, bag, kUnknownStat, kUnknownStat)
            .Has(NpcShopIssue::kEmptyCart));
}

TEST_CASE("Panier d'achat : le market juge le stock et le poids", "[sim][npc_shop]") {
  NpcShop shop;
  const std::vector<NpcShopOffer> offers = {
      NpcShopOffer{.item_id = kRedPotion, .price = 40, .stock = 3, .unit_weight = 70}};
  shop.TellMarket(offers);
  const std::array<NpcShopBuyLine, 1> four = {NpcShopBuyLine{.item_id = kRedPotion, .amount = 4}};
  const auto verdict = CheckNpcShopBuy(shop, four, 1000, {}, 8000, 8100);
  CHECK(verdict.Has(NpcShopIssue::kOverStock));
  CHECK(verdict.weight_known);
  CHECK(verdict.added_weight == 280);
  CHECK(verdict.Has(NpcShopIssue::kOverWeight));
}

TEST_CASE("Panier d'achat : hors market, la table des propriétés d'item donne le poids",
          "[sim][npc_shop]") {
  NpcShop shop;
  shop.TellDealChoice(kShopNpc);
  shop.TellBuyList(PotionAndKnife());

  // Une ligne sans poids de table laisse le poids non jugé.
  const std::array<NpcShopBuyLine, 2> half_known = {
      NpcShopBuyLine{.item_id = kRedPotion, .amount = 10, .table_unit_weight = 70},
      NpcShopBuyLine{.item_id = kKnife, .amount = 1}};
  auto verdict = CheckNpcShopBuy(shop, half_known, 100000, {}, 7900, 8000);
  CHECK_FALSE(verdict.weight_known);
  CHECK_FALSE(verdict.Has(NpcShopIssue::kOverWeight));

  // Chaque ligne pesée : 10 × 70 + 400, au-delà de ce que le personnage porte.
  const std::array<NpcShopBuyLine, 2> weighed = {
      NpcShopBuyLine{.item_id = kRedPotion, .amount = 10, .table_unit_weight = 70},
      NpcShopBuyLine{.item_id = kKnife, .amount = 1, .table_unit_weight = 400}};
  verdict = CheckNpcShopBuy(shop, weighed, 100000, {}, 7000, 8000);
  CHECK(verdict.weight_known);
  CHECK(verdict.added_weight == 1100);
  CHECK(verdict.Has(NpcShopIssue::kOverWeight));
  // Tout juste à la limite : le serveur ne refuse qu'au-delà.
  CHECK_FALSE(
      CheckNpcShopBuy(shop, weighed, 100000, {}, 6900, 8000).Has(NpcShopIssue::kOverWeight));
  // Un poids nul de la table est un poids connu.
  const std::array<NpcShopBuyLine, 1> feather = {
      NpcShopBuyLine{.item_id = kRedPotion, .amount = 1, .table_unit_weight = 0}};
  CHECK(CheckNpcShopBuy(shop, feather, 100000, {}, 8000, 8000).weight_known);
  // Le poids du personnage pas encore dit : rien n'est jugé.
  CHECK_FALSE(CheckNpcShopBuy(shop, weighed, 100000, {}, kUnknownStat, 8000).weight_known);
}

TEST_CASE("Panier d'achat : le poids que porte la liste du market l'emporte sur la table",
          "[sim][npc_shop]") {
  NpcShop shop;
  const std::vector<NpcShopOffer> offers = {
      NpcShopOffer{.item_id = kRedPotion, .price = 40, .stock = 10, .unit_weight = 70}};
  shop.TellMarket(offers);
  const std::array<NpcShopBuyLine, 1> two = {
      NpcShopBuyLine{.item_id = kRedPotion, .amount = 2, .table_unit_weight = 500}};
  const auto verdict = CheckNpcShopBuy(shop, two, 1000, {}, 0, 8000);
  CHECK(verdict.weight_known);
  CHECK(verdict.added_weight == 140);
}

TEST_CASE("Panier de vente : la possession et la bourse", "[sim][npc_shop]") {
  NpcShop shop;
  const std::vector<NpcShopSellOffer> sold = {
      NpcShopSellOffer{.client_slot = 2, .base_price = 10, .price = 1000000000}};
  shop.TellSellList(sold);
  const std::vector<InventoryItem> bag = {Carried(2, kRedPotion, 3)};

  const std::array<NpcShopSellLine, 1> two = {NpcShopSellLine{.client_slot = 2, .amount = 2}};
  auto verdict = CheckNpcShopSell(shop, two, bag, 0);
  CHECK(verdict.clear());
  CHECK(verdict.total == 2000000000);

  const std::array<NpcShopSellLine, 1> four = {NpcShopSellLine{.client_slot = 2, .amount = 4}};
  CHECK(CheckNpcShopSell(shop, four, bag, 0).Has(NpcShopIssue::kOverOwned));

  // Deux milliards, sur une bourse qui en porte déjà deux cents millions : la
  // recette serait tronquée.
  CHECK(CheckNpcShopSell(shop, two, bag, 200000000).Has(NpcShopIssue::kZenyOverflow));

  const std::array<NpcShopSellLine, 1> elsewhere = {NpcShopSellLine{.client_slot = 9, .amount = 1}};
  CHECK(CheckNpcShopSell(shop, elsewhere, bag, 0).Has(NpcShopIssue::kNotOffered));
}

TEST_CASE("Liste de vente : le verrou retire les favoris, une pile vide ne se montre pas",
          "[sim][npc_shop]") {
  InventoryItem potions = Carried(2, kRedPotion, 3);
  CHECK(ro::sim::NpcShopListsForSale(potions, true));

  potions.favorite = true;
  CHECK(ro::sim::NpcShopListsForSale(potions, false));
  CHECK_FALSE(ro::sim::NpcShopListsForSale(potions, true));

  const InventoryItem spent = Carried(3, kRedPotion, 0);
  CHECK_FALSE(ro::sim::NpcShopListsForSale(spent, false));
}
