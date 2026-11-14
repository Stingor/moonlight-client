// LES RÉCAPITULATIFS DE L'OUVERTURE D'UNE ÉCHOPPE : le titre, puis chaque objet
// dans l'ordre de la liste, la commission que retient le serveur (vente seule),
// puis le zeny et le trait. Une ouverture refusée n'écrit rien.

#include "game/trade_recap.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/buying_store.h"
#include "ro/sim/vending.h"
#include "ro/vfs/item_words.h"
#include "ro/vfs/trade_words.h"

using moonclient::TradeRecapItem;
using moonclient::VendingChatLine;
using ro::vfs::TradeWord;

namespace {

// Les gabarits tels que l'anglais du jeu les écrit, retraits compris.
ro::vfs::ItemWords RecapWords() {
  ro::vfs::ItemWords words;
  auto put = [&words](TradeWord word, std::string text) {
    words.trade.texts[static_cast<std::size_t>(word)] = std::move(text);
  };
  put(TradeWord::kRecapSaleTitle, "======================  SALE LIST  ======================");
  put(TradeWord::kRecapBuyTitle, "======================  BUY LIST  ======================");
  put(TradeWord::kRecapSaleLine, "    %s  :  %s  Zeny");
  put(TradeWord::kRecapBuyLine, "%s : %s  Zeny => %s ea.");
  put(TradeWord::kRecapCommissionNone, "Commission : none");
  put(TradeWord::kRecapCommissionAlways, "Commission : %s %% on every purchase");
  put(TradeWord::kRecapCommissionFrom, "Commission : %s %% from %s Zeny per purchase");
  put(TradeWord::kRecapCurrentZeny, "  Current Zeny : %s Z");
  return words;
}

// La taxe d'un rAthena d'usine : cinq pour cent dès cent millions par achat.
constexpr moonclient::VendingTax kFactoryTax{.permyriad = 500, .from_zeny = 100'000'000};

constexpr std::string_view kClosingRule =
    "============================ ============================";

std::vector<std::string> TextsOf(const std::vector<VendingChatLine>& lines) {
  std::vector<std::string> texts;
  for (const VendingChatLine& line : lines) {
    texts.push_back(line.text);
  }
  return texts;
}

}  // namespace

TEST_CASE("Récapitulatif : le taux se lit en pour cent, sans zéro de queue", "[app][trade_recap]") {
  CHECK(moonclient::VendingTaxPercent(0) == "0");
  CHECK(moonclient::VendingTaxPercent(500) == "5");
  CHECK(moonclient::VendingTaxPercent(50) == "0.5");
  CHECK(moonclient::VendingTaxPercent(125) == "1.25");
  CHECK(moonclient::VendingTaxPercent(5) == "0.05");
  CHECK(moonclient::VendingTaxPercent(10'000) == "100");
}

TEST_CASE("Récapitulatif : la commission dit la règle du serveur", "[app][trade_recap]") {
  const ro::vfs::ItemWords words = RecapWords();
  SECTION("taxe nulle : aucune, quel que soit le seuil") {
    CHECK(moonclient::VendingCommissionText(words, {.permyriad = 0, .from_zeny = 100'000'000}) ==
          "Commission : none");
  }
  SECTION("taxe et seuil : le taux à partir du seuil d'un achat") {
    CHECK(moonclient::VendingCommissionText(words, kFactoryTax) ==
          "Commission : 5 % from 100,000,000 Zeny per purchase");
  }
  SECTION("seuil nul : tout achat est taxé") {
    CHECK(moonclient::VendingCommissionText(words, {.permyriad = 50, .from_zeny = 0}) ==
          "Commission : 0.5 % on every purchase");
  }
}
TEST_CASE("Récapitulatif : la liste de vente, un objet et sa commission dans l'ordre",
          "[app][trade_recap]") {
  const ro::vfs::ItemWords words = RecapWords();
  const std::array<TradeRecapItem, 2> items = {
      TradeRecapItem{.name = "+7 Sword", .unit_price_zeny = 12'500'000, .amount = 1},
      TradeRecapItem{.name = "Red Potion", .unit_price_zeny = 45, .amount = 100}};

  const std::vector<VendingChatLine> lines =
      moonclient::SaleListRecap(words, items, 1'234'567, kFactoryTax);

  CHECK(TextsOf(lines) == std::vector<std::string>{
                              "======================  SALE LIST  ======================",
                              "    +7 Sword  :  12,500,000  Zeny",
                              "    Red Potion  :  45  Zeny",
                              "Commission : 5 % from 100,000,000 Zeny per purchase",
                              "  Current Zeny : 1,234,567 Z",
                              std::string(kClosingRule),
                          });
  REQUIRE(lines.size() == 6);
  CHECK(lines[0].color == moonclient::kRecapFrameColor);
  CHECK(lines[1].color == moonclient::kRecapItemColor);
  CHECK(lines[3].color == moonclient::kRecapItemColor);
  CHECK(lines[4].color == moonclient::kRecapZenyColor);
  CHECK(lines[5].color == moonclient::kRecapFrameColor);
}

TEST_CASE("Récapitulatif : sans taxe, la liste de vente le dit", "[app][trade_recap]") {
  const ro::vfs::ItemWords words = RecapWords();
  const std::array<TradeRecapItem, 1> items = {
      TradeRecapItem{.name = "+7 Sword", .unit_price_zeny = 500'000'000, .amount = 1}};

  const std::vector<VendingChatLine> lines =
      moonclient::SaleListRecap(words, items, 0, {.permyriad = 0, .from_zeny = 100'000'000});

  CHECK(TextsOf(lines) == std::vector<std::string>{
                              "======================  SALE LIST  ======================",
                              "    +7 Sword  :  500,000,000  Zeny",
                              "Commission : none",
                              "  Current Zeny : 0 Z",
                              std::string(kClosingRule),
                          });
}

TEST_CASE("Récapitulatif : la liste d'achat porte la quantité voulue, sans commission",
          "[app][trade_recap]") {
  const ro::vfs::ItemWords words = RecapWords();
  const std::array<TradeRecapItem, 2> items = {
      TradeRecapItem{.name = "Jellopy", .unit_price_zeny = 1'000, .amount = 2'500},
      TradeRecapItem{.name = "Elunium", .unit_price_zeny = 15'000'000, .amount = 3}};

  const std::vector<VendingChatLine> lines = moonclient::BuyListRecap(words, items, 0);

  CHECK(TextsOf(lines) == std::vector<std::string>{
                              "======================  BUY LIST  ======================",
                              "Jellopy : 1,000  Zeny => 2,500 ea.",
                              "Elunium : 15,000,000  Zeny => 3 ea.",
                              "  Current Zeny : 0 Z",
                              std::string(kClosingRule),
                          });
}

TEST_CASE("Récapitulatif : un gabarit illisible se tait, sans décaler les autres",
          "[app][trade_recap]") {
  ro::vfs::ItemWords words = RecapWords();
  words.trade.texts[static_cast<std::size_t>(TradeWord::kRecapCommissionFrom)] = "%d";
  const std::array<TradeRecapItem, 1> items = {
      TradeRecapItem{.name = "Jellopy", .unit_price_zeny = 10, .amount = 1}};

  const std::vector<VendingChatLine> lines =
      moonclient::SaleListRecap(words, items, 5, kFactoryTax);

  CHECK(TextsOf(lines) == std::vector<std::string>{
                              "======================  SALE LIST  ======================",
                              "    Jellopy  :  10  Zeny",
                              "  Current Zeny : 5 Z",
                              std::string(kClosingRule),
                          });
}

TEST_CASE("Récapitulatif : l'échoppe de vente ne se dit qu'à l'ouverture réussie",
          "[app][trade_recap]") {
  SECTION("réussie : une fois") {
    ro::sim::Vending vending;
    vending.TellPreparation(3);
    vending.NoteOpeningSent("Potions");
    ro::sim::VendingLine line;
    line.item.item_id = 501;
    line.item.amount = 10;
    line.price_zeny = 40;
    vending.TellShopOpened(1, {line});
    const std::uint32_t before = vending.opening_result_seq();
    vending.TellOpeningResult(ro::sim::kVendingOpenSucceeded);
    CHECK(moonclient::SaleListRecapDue(vending, before));
    CHECK_FALSE(moonclient::SaleListRecapDue(vending, vending.opening_result_seq()));
  }
  SECTION("refusée : rien") {
    ro::sim::Vending vending;
    vending.TellPreparation(3);
    vending.NoteOpeningSent("Potions");
    const std::uint32_t before = vending.opening_result_seq();
    vending.TellOpeningResult(ro::sim::kVendingOpenFailed);
    CHECK_FALSE(moonclient::SaleListRecapDue(vending, before));
  }
  SECTION("ouverte sans demande de ce client : rien") {
    ro::sim::Vending vending;
    vending.TellShopOpened(1, {});
    const std::uint32_t before = vending.opening_result_seq();
    vending.TellOpeningResult(ro::sim::kVendingOpenSucceeded);
    CHECK_FALSE(moonclient::SaleListRecapDue(vending, before));
  }
}

TEST_CASE("Récapitulatif : l'échoppe d'achat ne se dit qu'à l'ouverture", "[app][trade_recap]") {
  SECTION("ouverte : une fois") {
    ro::sim::BuyingStore store;
    store.TellPreparation(2);
    REQUIRE(store.TellRequested("Jellopy", 100'000));
    const std::uint32_t before = store.opening_seq();
    const std::array<ro::sim::BuyingStoreLine, 1> lines = {
        ro::sim::BuyingStoreLine{.item_id = 909, .wanted = 50, .price = 10}};
    store.TellOpened(1, 100'000, lines);
    CHECK(moonclient::BuyListRecapDue(store, before));
    CHECK_FALSE(moonclient::BuyListRecapDue(store, store.opening_seq()));
  }
  SECTION("refusée : rien") {
    ro::sim::BuyingStore store;
    store.TellPreparation(2);
    REQUIRE(store.TellRequested("Jellopy", 100'000));
    const std::uint32_t before = store.opening_seq();
    store.TellOpenRefused(ro::sim::kBuyingStoreOpenFailed, 0);
    CHECK_FALSE(moonclient::BuyListRecapDue(store, before));
  }
}
