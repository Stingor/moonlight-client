// LE MARCHÉ, VU DE L'ÉTAGE : ce que « Y aller » et « Voir » demandent, et
// l'ouverture sur un item.

#include "game/market_route.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using moonclient::MarketRoute;
using moonclient::RouteMarketAnswer;
using ro::sim::MarketKind;
using ro::sim::MarketOffer;

namespace {

constexpr std::uint32_t kFirstStore = 7;
constexpr std::uint32_t kSecondStore = 9;
constexpr std::uint32_t kOwnerAid = 2000001;

MarketOffer Offer(std::uint32_t store_id, std::string map, int x, int y) {
  MarketOffer offer;
  offer.store_id = store_id;
  offer.owner_aid = kOwnerAid;
  offer.map_name = std::move(map);
  offer.cell_x = x;
  offer.cell_y = y;
  return offer;
}

const std::vector<MarketOffer> kOffers = {
    Offer(kFirstStore, "prontera", 150, 120),
    Offer(kSecondStore, "geffen", 110, 80),
};
// La page dessinée montre la seconde offre, puis la première.
const std::vector<std::size_t> kPage = {1, 0};

}  // namespace

TEST_CASE("Marché : « Y aller » guide jusqu'à l'échoppe de la ligne", "[app][market]") {
  ro::ui::MarketAction answer;
  answer.go_to_row = 0;
  const MarketRoute route = RouteMarketAnswer(answer, kOffers, kPage, MarketKind::kVending);
  REQUIRE(route.go_to.has_value());
  CHECK(route.go_to->map_name == "geffen");
  CHECK(route.go_to->cell_x == 110);
  CHECK(route.go_to->cell_y == 80);
  CHECK_FALSE(route.store_query.has_value());
  CHECK_FALSE(route.search);
}

TEST_CASE("Marché : « Voir » demande le contenu de l'échoppe, en lecture seule", "[app][market]") {
  ro::ui::MarketAction answer;
  answer.view_store_row = 1;
  const MarketRoute route = RouteMarketAnswer(answer, kOffers, kPage, MarketKind::kBuying);
  REQUIRE(route.store_query.has_value());
  CHECK(route.store_query->store_id == kFirstStore);
  CHECK(route.store_query->owner_aid == kOwnerAid);
  CHECK(route.store_query->kind == MarketKind::kBuying);
  // Le contenu d'une échoppe ne porte aucun filtre.
  CHECK(route.store_query->item_ids.empty());
  REQUIRE(route.store_offer != nullptr);
  CHECK(route.store_offer->store_id == kFirstStore);
  CHECK_FALSE(route.go_to.has_value());
}

TEST_CASE("Marché : une ligne hors de la page, ou une fenêtre fermée, ne désigne rien",
          "[app][market]") {
  ro::ui::MarketAction answer;
  answer.go_to_row = 2;
  answer.view_store_row = -1;
  const MarketRoute outside = RouteMarketAnswer(answer, kOffers, kPage, MarketKind::kVending);
  CHECK_FALSE(outside.go_to.has_value());
  CHECK_FALSE(outside.store_query.has_value());
  answer.go_to_row = 0;
  answer.search = true;
  answer.close = true;
  const MarketRoute closed = RouteMarketAnswer(answer, kOffers, kPage, MarketKind::kVending);
  CHECK_FALSE(closed.go_to.has_value());
  CHECK_FALSE(closed.search);
}

TEST_CASE("Marché : chercher l'emporte sur actualiser", "[app][market]") {
  ro::ui::MarketAction answer;
  answer.refresh = true;
  CHECK(RouteMarketAnswer(answer, kOffers, kPage, MarketKind::kVending).refresh);
  answer.search = true;
  const MarketRoute both = RouteMarketAnswer(answer, kOffers, kPage, MarketKind::kVending);
  CHECK(both.search);
  CHECK_FALSE(both.refresh);
}

TEST_CASE("Marché : l'ouverture sur un item remplit la saisie de son identifiant",
          "[app][market]") {
  CHECK(moonclient::MarketOpeningQuery(std::uint32_t{4001}) == "4001");
  CHECK(moonclient::MarketOpeningQuery(std::nullopt).empty());
}

TEST_CASE("Marché : `/marche`, sans la casse, et rien d'autre", "[app][market]") {
  CHECK(moonclient::IsMarketCommand("/marche"));
  CHECK(moonclient::IsMarketCommand("/MARCHE"));
  CHECK_FALSE(moonclient::IsMarketCommand("/marche potion"));
  CHECK_FALSE(moonclient::IsMarketCommand("/march"));
}

TEST_CASE("Marché : une enseigne d'échoppe se compare dans l'onglet de sa sorte", "[app][market]") {
  CHECK(moonclient::MarketKindOfSignboard(ro::sim::SignboardKind::kVendingShop) ==
        MarketKind::kVending);
  CHECK(moonclient::MarketKindOfSignboard(ro::sim::SignboardKind::kBuyingStore) ==
        MarketKind::kBuying);
  CHECK_FALSE(moonclient::MarketKindOfSignboard(ro::sim::SignboardKind::kChatRoom).has_value());
  CHECK(moonclient::MarketTabOf(MarketKind::kVending) == ro::ui::kMarketSellTab);
  CHECK(moonclient::MarketTabOf(MarketKind::kBuying) == ro::ui::kMarketBuyTab);
  CHECK(moonclient::MarketTabOf(MarketKind::kNpc) == ro::ui::kMarketNpcTab);
}

TEST_CASE("Marché : le prix d'une offre dit sa monnaie", "[app][market]") {
  MarketOffer offer;
  offer.price_zeny = 1'500;
  CHECK(moonclient::MarketPriceText(offer, {}) == "1,500 z");
  offer.currency = ro::sim::MarketCurrency::kCash;
  CHECK(moonclient::MarketPriceText(offer, {}) == "1,500 cash");
  offer.currency = ro::sim::MarketCurrency::kItem;
  offer.price_zeny = 3;
  CHECK(moonclient::MarketPriceText(offer, "Jellopy") == "3 x Jellopy");
  offer.currency = ro::sim::MarketCurrency::kPoints;
  offer.price_zeny = 50;
  offer.currency_name = "#KAFRAPOINTS";
  CHECK(moonclient::MarketPriceText(offer, {}) == "50 #KAFRAPOINTS");
  offer.currency = ro::sim::MarketCurrency::kBarter;
  CHECK(moonclient::MarketPriceText(offer, {}) == "troc + 50 z");
  offer.price_zeny = 0;
  CHECK(moonclient::MarketPriceText(offer, {}) == "troc");
}

TEST_CASE("Marché : chez un PNJ, un stock nul est illimité", "[app][market]") {
  MarketOffer offer;
  CHECK(moonclient::MarketAmountText(offer, MarketKind::kNpc) == "—");
  CHECK(moonclient::MarketAmountText(offer, MarketKind::kVending) == "0");
  offer.item.amount = 12;
  CHECK(moonclient::MarketAmountText(offer, MarketKind::kNpc) == "12");
}

TEST_CASE("Marché : la colonne des quantités se tait chez des PNJ au stock illimité",
          "[app][market]") {
  std::vector<MarketOffer> offers(2);
  CHECK_FALSE(moonclient::MarketAmountColumnShown(offers, MarketKind::kNpc));
  CHECK(moonclient::MarketAmountColumnShown(offers, MarketKind::kVending));
  CHECK(moonclient::MarketAmountColumnShown(offers, MarketKind::kBuying));
  offers[1].item.amount = 5;
  CHECK(moonclient::MarketAmountColumnShown(offers, MarketKind::kNpc));
}

TEST_CASE("Marché : « Voir » sur un PNJ demande sa boutique", "[app][market]") {
  constexpr std::uint32_t kNpcBlock = 110000123;
  std::vector<MarketOffer> offers = {Offer(kNpcBlock, "prontera", 73, 134)};
  offers[0].owner_aid = 0;
  const std::vector<std::size_t> page = {0};
  ro::ui::MarketAction answer;
  answer.view_store_row = 0;
  answer.go_to_row = 0;
  const MarketRoute route = RouteMarketAnswer(answer, offers, page, MarketKind::kNpc);
  REQUIRE(route.store_query.has_value());
  CHECK(route.store_query->kind == MarketKind::kNpc);
  CHECK(route.store_query->store_id == kNpcBlock);
  CHECK(route.store_query->owner_aid == 0);
  REQUIRE(route.go_to.has_value());
  CHECK(route.go_to->map_name == "prontera");
  CHECK(route.go_to->cell_x == 73);
}
