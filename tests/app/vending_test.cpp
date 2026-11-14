// L'ÉCHOPPE DE VENTE VUE DE L'ÉCRAN : les mots d'un code du serveur, la
// dernière échoppe reposée sur le cart, et l'enseigne qu'un clic vise.
//
// 🔴 LA DERNIÈRE ÉCHOPPE NE POSE RIEN QUE LE CART NE CONTIENNE, et elle
// reconnaît une pile à son objet ENTIER : le prix d'une épée +7 ne se pose pas
// sur une épée +0.

#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/vfs/item_words.h"
#include "ro/vfs/trade_words.h"

#include "game/vending_memory.h"
#include "game/vending_said.h"
#include "world/signboard_pick.h"

using moonclient::RememberedVendingLine;
using moonclient::RememberedVendingShop;
using moonclient::ResumeVendingLines;

namespace {

ro::sim::InventoryItem Stack(std::uint16_t slot, std::uint32_t item_id, int amount,
                             std::uint8_t refine = 0) {
  ro::sim::InventoryItem item;
  item.client_slot = slot;
  item.item_id = item_id;
  item.amount = static_cast<std::int16_t>(amount);
  item.refine_level = refine;
  item.identified = true;
  return item;
}

ro::vfs::ItemWords WordsWith(ro::vfs::TradeWord word, std::string text) {
  ro::vfs::ItemWords words;
  words.trade.texts[static_cast<std::size_t>(word)] = std::move(text);
  return words;
}

}  // namespace

TEST_CASE("Échoppe : la dernière échoppe se repose sur le cart d'aujourd'hui", "[app][vending]") {
  RememberedVendingShop shop;
  shop.character = "Marchand";
  shop.title = "Potions";
  shop.lines = {RememberedVendingLine{.item_id = 501, .amount = 100, .price_zeny = 40},
                RememberedVendingLine{
                    .item_id = 1201, .refine_level = 7, .amount = 1, .price_zeny = 5'000'000},
                RememberedVendingLine{.item_id = 502, .amount = 5, .price_zeny = 90},
                RememberedVendingLine{.item_id = 501, .amount = 10, .price_zeny = 45}};
  const std::array<ro::sim::InventoryItem, 4> cart = {Stack(2, 501, 30), Stack(3, 1201, 1, 0),
                                                      Stack(4, 501, 8), Stack(5, 999, 1)};

  const auto resumed = ResumeVendingLines(shop, cart, 12);
  // La potion prend la première pile, bornée à ce qu'elle contient ; la
  // seconde ligne de potions prend l'autre pile. L'épée +0 n'est pas l'épée
  // +7, et la potion bleue n'est plus dans le cart.
  REQUIRE(resumed.size() == 2);
  CHECK(resumed[0].cart_slot == 2);
  CHECK(resumed[0].amount == 30);
  CHECK(resumed[0].price_zeny == 40);
  CHECK(resumed[1].cart_slot == 4);
  CHECK(resumed[1].amount == 8);
  CHECK(resumed[1].price_zeny == 45);

  // Pas plus de lignes que la préparation n'en permet.
  CHECK(ResumeVendingLines(shop, cart, 1).size() == 1);
}

TEST_CASE("Échoppe : la mémoire garde une échoppe par personnage", "[app][vending]") {
  moonclient::VendingMemory memory;
  memory.Remember({.character = "A", .title = "un"});
  memory.Remember({.character = "B", .title = "deux"});
  memory.Remember({.character = "A", .title = "trois"});
  REQUIRE(memory.shops.size() == 2);
  REQUIRE(memory.LastOf("A") != nullptr);
  CHECK(memory.LastOf("A")->title == "trois");
  CHECK(memory.LastOf("C") == nullptr);
}

TEST_CASE("Échoppe : un refus d'achat se dit avec les mots du jeu", "[app][vending]") {
  ro::vfs::ItemWords words =
      WordsWith(ro::vfs::TradeWord::kMerchantOutOfStock, "%s : plus que %d en stock.");
  const auto out_of_stock =
      moonclient::SaidOfPurchaseRefusal(words, ro::sim::kVendingBuyOutOfStock, "Red Potion", 3);
  CHECK(out_of_stock.text == "Red Potion : plus que 3 en stock.");
  CHECK(out_of_stock.color == moonclient::kVendingLightRedColor);

  // L'offre introuvable : le client se tait pour un code qui la nomme.
  CHECK(
      moonclient::SaidOfPurchaseRefusal(words, ro::sim::kVendingBuyOutOfStock, "", 3).text.empty());

  words.trade.texts[static_cast<std::size_t>(ro::vfs::TradeWord::kBuyFromMerchantFailed)] =
      "%s : %s";
  words.trade.texts[static_cast<std::size_t>(ro::vfs::TradeWord::kLacksZeny)] = "pas assez.";
  CHECK(moonclient::SaidOfPurchaseRefusal(words, ro::sim::kVendingBuyNoZeny, "Knife", 1).text ==
        "Knife : pas assez.");

  words.trade.texts[static_cast<std::size_t>(ro::vfs::TradeWord::kNoSaleInformation)] = "rien.";
  const auto no_sale =
      moonclient::SaidOfPurchaseRefusal(words, ro::sim::kVendingBuyNoSaleInfo, "", 0);
  CHECK(no_sale.text == "rien.");
  CHECK(no_sale.color == moonclient::kVendingFailedColor);
}

TEST_CASE("Échoppe : l'échec de l'ouverture se dit dans une boîte, la case dans une ligne",
          "[app][vending]") {
  ro::vfs::ItemWords words = WordsWith(ro::vfs::TradeWord::kShopMakingFailed, "échec");
  words.trade.texts[static_cast<std::size_t>(ro::vfs::TradeWord::kShopBadPosition)] = "case";
  CHECK(moonclient::SaidOfOpeningResult(words, ro::sim::kVendingOpenFailed).said ==
        moonclient::VendingOpeningSaid::kNotice);
  const auto position =
      moonclient::SaidOfOpeningResult(words, ro::sim::kVendingOpenForbiddenPosition);
  CHECK(position.said == moonclient::VendingOpeningSaid::kLine);
  CHECK(position.text == "case");
  CHECK(moonclient::SaidOfOpeningResult(words, ro::sim::kVendingOpenSucceeded).said ==
        moonclient::VendingOpeningSaid::kNothing);
}

TEST_CASE("Échoppe : une vente s'écrit en cyan pâle", "[app][vending]") {
  const ro::vfs::ItemWords words = WordsWith(ro::vfs::TradeWord::kItemSold, "%s x %d vendu");
  const auto sold = moonclient::SaidOfSale(words, "Red Potion", 4);
  CHECK(sold.text == "Red Potion x 4 vendu");
  CHECK(sold.color == moonclient::kVendingSoldColor);
}

TEST_CASE("Échoppe : l'enseigne peinte la dernière prend le clic", "[app][vending]") {
  using moonclient::SignboardHit;
  const std::array<SignboardHit, 3> boards = {
      SignboardHit{.owner_id = 1,
                   .kind = ro::sim::SignboardKind::kVendingShop,
                   .rect = {.min = {0.0F, 0.0F}, .max = {100.0F, 20.0F}}},
      SignboardHit{.owner_id = 2,
                   .kind = ro::sim::SignboardKind::kBuyingStore,
                   .rect = {.min = {50.0F, 10.0F}, .max = {150.0F, 30.0F}}},
      SignboardHit{.owner_id = 3,
                   .kind = ro::sim::SignboardKind::kChatRoom,
                   .rect = {.min = {0.0F, 0.0F}, .max = {200.0F, 200.0F}}}};
  // LE SALON EST DESSUS, et il cache les échoppes au clic comme à l'œil : il
  // n'en ouvre aucune, et ne laisse pas passer le clic à celles d'en dessous.
  CHECK_FALSE(moonclient::StoreSignboardAt(boards, {60.0F, 15.0F}).has_value());
  CHECK_FALSE(moonclient::StoreSignboardAt(boards, {180.0F, 180.0F}).has_value());
  const auto top = moonclient::TopSignboardAt(boards, {60.0F, 15.0F});
  REQUIRE(top.has_value());
  CHECK(top->owner_id == 3);

  // Sans le salon, la plus haute des deux échoppes l'emporte.
  const std::span<const SignboardHit> shops(boards.data(), 2);
  const auto overlap = moonclient::StoreSignboardAt(shops, {60.0F, 15.0F});
  REQUIRE(overlap.has_value());
  CHECK(overlap->owner_id == 2);
  const auto first = moonclient::StoreSignboardAt(shops, {10.0F, 5.0F});
  REQUIRE(first.has_value());
  CHECK(first->owner_id == 1);
  // LE BORD GAUCHE ET LE BORD HAUT EN SONT ; LE BORD DROIT ET LE BORD BAS NON :
  // le pixel 100 est le premier hors de la première enseigne, le 20 aussi.
  const auto corner = moonclient::StoreSignboardAt(shops, {0.0F, 0.0F});
  REQUIRE(corner.has_value());
  CHECK(corner->owner_id == 1);
  CHECK_FALSE(moonclient::StoreSignboardAt(shops, {100.0F, 5.0F}).has_value());
  const auto right_inside = moonclient::StoreSignboardAt(shops, {99.5F, 5.0F});
  REQUIRE(right_inside.has_value());
  CHECK(right_inside->owner_id == 1);
  CHECK_FALSE(moonclient::StoreSignboardAt(shops, {10.0F, 20.0F}).has_value());
  const auto bottom_inside = moonclient::StoreSignboardAt(shops, {10.0F, 19.5F});
  REQUIRE(bottom_inside.has_value());
  CHECK(bottom_inside->owner_id == 1);
  CHECK_FALSE(moonclient::TopSignboardAt(shops, {180.0F, 180.0F}).has_value());
}

TEST_CASE("Échoppe : une demande ne part que si rien ne s'y oppose", "[app][vending]") {
  using moonclient::StoreVisitGate;
  using moonclient::StoreVisitMayLeave;
  constexpr std::uint32_t kMerchant = 7;
  constexpr std::uint32_t kMe = 9;
  constexpr std::uint32_t kFirstMs = 5000;
  const StoreVisitGate first{.owner_id = kMerchant, .player_id = kMe, .now_ms = kFirstMs};
  CHECK(StoreVisitMayLeave(first));

  StoreVisitGate own = first;
  own.owner_id = kMe;
  CHECK_FALSE(StoreVisitMayLeave(own));
  StoreVisitGate dead = first;
  dead.player_dead = true;
  CHECK_FALSE(StoreVisitMayLeave(dead));
  StoreVisitGate visiting = first;
  visiting.visiting = true;
  CHECK_FALSE(StoreVisitMayLeave(visiting));

  // L'ÉCART SE DÉPASSE : à exactement une seconde, la seconde demande d'un
  // double-clic ne part pas ; une milliseconde plus tard, si.
  StoreVisitGate again = first;
  again.last_sent_ms = kFirstMs;
  again.now_ms = kFirstMs + moonclient::kStoreVisitSpacingMs;
  CHECK_FALSE(StoreVisitMayLeave(again));
  again.now_ms = kFirstMs + moonclient::kStoreVisitSpacingMs + 1;
  CHECK(StoreVisitMayLeave(again));

  // UNE DEMANDE PARTIE À L'INSTANT ZÉRO COMPTE AUSSI.
  StoreVisitGate at_zero = first;
  at_zero.last_sent_ms = 0;
  at_zero.now_ms = moonclient::kStoreVisitSpacingMs;
  CHECK_FALSE(StoreVisitMayLeave(at_zero));
}

TEST_CASE("Échoppe : l'heure d'une vente s'écrit", "[app][vending]") {
  CHECK(moonclient::VendingSoldAtText(0).size() == std::string("01/01 00:00:00").size());
}
