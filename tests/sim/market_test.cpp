// LE MARCHÉ, sans fil : la saisie jugée — un nom ou un identifiant —, les
// filtres qui partent, le tri et les pages.

#include "ro/sim/market.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using ro::sim::MarketColumn;
using ro::sim::MarketFault;
using ro::sim::MarketForm;
using ro::sim::MarketKind;
using ro::sim::MarketOffer;
using ro::sim::MarketSort;
using ro::sim::PlanMarketSearch;
using ro::sim::StoreSearchCatalogueEntry;

namespace {

constexpr std::uint32_t kRedPotion = 501;
constexpr std::uint32_t kYellowPotion = 503;
constexpr std::uint32_t kSword = 1101;
constexpr std::uint32_t kPoringCard = 4001;

// Trié par identifiant, comme la recherche l'exige.
const std::array<StoreSearchCatalogueEntry, 4> kCatalogue = {{
    {.item_id = kRedPotion, .name = "Red Potion"},
    {.item_id = kYellowPotion, .name = "Yellow Potion"},
    {.item_id = kSword, .name = "Épée"},
    {.item_id = kPoringCard, .name = "Poring Card", .card = true},
}};

MarketOffer Offer(std::uint32_t price, int amount, std::string shop, std::string map) {
  MarketOffer offer;
  offer.price_zeny = price;
  offer.item.amount = static_cast<std::int16_t>(amount);
  offer.shop_name = std::move(shop);
  offer.map_name = std::move(map);
  return offer;
}

}  // namespace

TEST_CASE("Marché : un nom partiel, sans casse ni accents, désigne ses items", "[sim][market]") {
  const auto plan = PlanMarketSearch(MarketForm{.text = "  POTION "}, kCatalogue);
  CHECK(plan.fault == MarketFault::kNone);
  CHECK(plan.query.item_ids == std::vector<std::uint32_t>{kRedPotion, kYellowPotion});
  CHECK(PlanMarketSearch(MarketForm{.text = "epee"}, kCatalogue).query.item_ids ==
        std::vector<std::uint32_t>{kSword});
}

TEST_CASE("Marché : une saisie toute en chiffres cherche l'identifiant", "[sim][market]") {
  // ⚠ « 501 » n'est le nom d'aucun item, mais c'est l'identifiant de l'un.
  const auto plan = PlanMarketSearch(MarketForm{.text = "4001"}, kCatalogue);
  CHECK(plan.fault == MarketFault::kNone);
  CHECK(plan.query.item_ids == std::vector<std::uint32_t>{kPoringCard});
  // Un identifiant que la table ne connaît pas part quand même : c'est le
  // serveur qui sait ce qui se vend.
  CHECK(PlanMarketSearch(MarketForm{.text = "99999"}, kCatalogue).query.item_ids ==
        std::vector<std::uint32_t>{99999});
}

TEST_CASE("Marché : une saisie vide cherche tout, un nom inconnu ne part pas", "[sim][market]") {
  const auto all = PlanMarketSearch(MarketForm{}, kCatalogue);
  CHECK(all.fault == MarketFault::kNone);
  CHECK(all.query.item_ids.empty());
  CHECK(PlanMarketSearch(MarketForm{.text = "Zzz"}, kCatalogue).fault == MarketFault::kUnknownItem);
}

TEST_CASE("Marché : un nom trop vague ne part pas", "[sim][market]") {
  std::vector<std::string> names;
  std::vector<StoreSearchCatalogueEntry> catalogue;
  names.reserve(ro::sim::kMarketMostIds + 1);
  for (std::uint32_t rank = 0; rank <= ro::sim::kMarketMostIds; ++rank) {
    names.push_back("Apple " + std::to_string(rank));
  }
  for (std::uint32_t rank = 0; rank <= ro::sim::kMarketMostIds; ++rank) {
    catalogue.push_back({.item_id = rank + 1, .name = names[rank]});
  }
  const auto plan = PlanMarketSearch(MarketForm{.text = "apple"}, catalogue);
  CHECK(plan.fault == MarketFault::kTooManyItems);
  CHECK(plan.query.item_ids.empty());
}

TEST_CASE("Marché : les filtres partent tels quels", "[sim][market]") {
  const auto plan = PlanMarketSearch(MarketForm{.kind = MarketKind::kVending,
                                                .category = ro::sim::MarketCategory::kCard,
                                                .refine_min = 4,
                                                .refine_max = 7,
                                                .min_price = "100",
                                                .max_price = "2000",
                                                .with_cards = true,
                                                .with_options = true},
                                     kCatalogue);
  REQUIRE(plan.fault == MarketFault::kNone);
  CHECK(plan.query.kind == MarketKind::kVending);
  CHECK(plan.query.category == ro::sim::MarketCategory::kCard);
  CHECK(plan.query.refine_min == 4);
  CHECK(plan.query.refine_max == 7);
  CHECK(plan.query.price_min_zeny == 100);
  CHECK(plan.query.price_max_zeny == 2000);
  CHECK(plan.query.flags == (ro::sim::kMarketWithCards | ro::sim::kMarketWithOptions));
  // Un maximum vide ne borne rien.
  CHECK(PlanMarketSearch(MarketForm{.min_price = "500"}, kCatalogue).query.price_max_zeny == 0);
}

TEST_CASE("Marché : des bornes à l'envers ne partent pas", "[sim][market]") {
  CHECK(PlanMarketSearch(MarketForm{.min_price = "500", .max_price = "100"}, kCatalogue).fault ==
        MarketFault::kBadPrice);
  CHECK(PlanMarketSearch(MarketForm{.min_price = "abc"}, kCatalogue).fault ==
        MarketFault::kBadPrice);
  CHECK(PlanMarketSearch(MarketForm{.refine_min = 8, .refine_max = 3}, kCatalogue).fault ==
        MarketFault::kBadRefine);
}

TEST_CASE("Marché : prix unitaire croissant par défaut, et chaque colonne se trie",
          "[sim][market]") {
  const std::vector<MarketOffer> offers = {
      Offer(300, 1, "Charlie", "payon"),
      Offer(100, 5, "alpha", "prontera"),
      Offer(200, 3, "Bravo", "geffen"),
  };
  const std::vector<std::string> keys = {"b", "c", "a"};
  CHECK(ro::sim::MarketOrder(offers, keys, MarketSort{}) == std::vector<std::size_t>{1, 2, 0});
  CHECK(ro::sim::MarketOrder(offers, keys,
                             MarketSort{.column = MarketColumn::kPrice, .descending = true}) ==
        std::vector<std::size_t>{0, 2, 1});
  CHECK(ro::sim::MarketOrder(offers, keys, MarketSort{.column = MarketColumn::kItem}) ==
        std::vector<std::size_t>{2, 0, 1});
  CHECK(ro::sim::MarketOrder(offers, keys, MarketSort{.column = MarketColumn::kAmount}) ==
        std::vector<std::size_t>{0, 2, 1});
  // L'échoppe se range sans casse.
  CHECK(ro::sim::MarketOrder(offers, keys, MarketSort{.column = MarketColumn::kShop}) ==
        std::vector<std::size_t>{1, 2, 0});
  CHECK(ro::sim::MarketOrder(offers, keys, MarketSort{.column = MarketColumn::kMap}) ==
        std::vector<std::size_t>{2, 0, 1});
}

TEST_CASE("Marché : le prix range une monnaie après l'autre, les zeny d'abord", "[sim][market]") {
  std::vector<MarketOffer> offers = {
      Offer(50, 0, "Kafra", "prontera"),
      Offer(900, 0, "Outils", "prontera"),
      Offer(100, 0, "Marchande", "geffen"),
  };
  offers[0].currency = ro::sim::MarketCurrency::kPoints;
  CHECK(ro::sim::MarketOrder(offers, {}, MarketSort{}) == std::vector<std::size_t>{2, 1, 0});
}

TEST_CASE("Marché : une boutique d'achat ou de PNJ ne garde pas les filtres d'item",
          "[sim][market]") {
  const MarketForm form{.kind = MarketKind::kNpc,
                        .refine_min = 4,
                        .refine_max = 7,
                        .with_cards = true,
                        .with_options = true};
  const auto plan = PlanMarketSearch(form, kCatalogue);
  REQUIRE(plan.fault == MarketFault::kNone);
  CHECK(plan.query.kind == MarketKind::kNpc);
  CHECK(plan.query.refine_min == 0);
  CHECK(plan.query.refine_max == ro::sim::kMarketHighestRefine);
  CHECK(plan.query.flags == 0);
}

TEST_CASE("Marché : comparer une échoppe, c'est la nommer, sans items ni contenu",
          "[sim][market]") {
  constexpr std::uint32_t kOwnerAid = 2000005;
  const ro::sim::MarketQuery query = ro::sim::MarketCompareQuery(MarketKind::kBuying, kOwnerAid);
  CHECK(query.kind == MarketKind::kBuying);
  CHECK(query.flags == ro::sim::kMarketCompare);
  CHECK(query.owner_aid == kOwnerAid);
  // Une recherche, et non le contenu d'une échoppe : sa réponse remplit la liste.
  CHECK(query.store_id == 0);
  CHECK(query.item_ids.empty());
}

TEST_CASE("Marché : à égalité, le prix départage", "[sim][market]") {
  const std::vector<MarketOffer> offers = {
      Offer(300, 1, "x", "prontera"),
      Offer(100, 1, "y", "prontera"),
  };
  CHECK(ro::sim::MarketOrder(offers, {}, MarketSort{.column = MarketColumn::kMap}) ==
        std::vector<std::size_t>{1, 0});
}

TEST_CASE("Marché : les pages", "[sim][market]") {
  CHECK(ro::sim::MarketPageCount(0) == 1);
  CHECK(ro::sim::MarketPageCount(ro::sim::kMarketRowsPerPage) == 1);
  CHECK(ro::sim::MarketPageCount(ro::sim::kMarketRowsPerPage + 1) == 2);
  std::vector<std::size_t> order(ro::sim::kMarketRowsPerPage + 3);
  for (std::size_t rank = 0; rank < order.size(); ++rank) {
    order[rank] = rank;
  }
  CHECK(ro::sim::MarketPage(order, 1).size() == ro::sim::kMarketRowsPerPage);
  const auto second = ro::sim::MarketPage(order, 2);
  REQUIRE(second.size() == 3);
  CHECK(second[0] == ro::sim::kMarketRowsPerPage);
  // Une page hors bornes se ramène à la dernière, ou à la première.
  CHECK(ro::sim::MarketPage(order, 9).size() == 3);
  CHECK(ro::sim::MarketPage(order, 0).size() == ro::sim::kMarketRowsPerPage);
}

TEST_CASE("Marché : « Vendu chez » est une demande à part, qui ne touche pas la liste",
          "[sim][market]") {
  ro::sim::Market market;
  ro::sim::MarketQuery search;
  search.query_id = market.NextQueryId();
  market.NoteSent(search);
  ro::sim::MarketQuery sellers = ro::sim::MarketNpcSellersQuery(kRedPotion);
  CHECK(sellers.kind == MarketKind::kNpc);
  CHECK(sellers.item_ids == std::vector<std::uint32_t>{kRedPotion});
  CHECK(sellers.aside);
  sellers.query_id = market.NextQueryId();
  market.NoteSent(sellers);
  CHECK(market.search().waiting());
  CHECK(market.aside().waiting());

  ro::sim::MarketReplyPart part;
  part.query_id = sellers.query_id;
  part.kind = MarketKind::kNpc;
  part.offers = {Offer(50, 0, "Tool Dealer", "prontera")};
  market.Tell(part);
  CHECK(market.aside().complete());
  REQUIRE(market.aside().offers().size() == 1);
  CHECK(market.aside().offers()[0].shop_name == "Tool Dealer");
  // La recherche du Marché attend toujours la sienne.
  CHECK(market.search().waiting());
  CHECK(market.search().offers().empty());

  // Son refus se dit à part : le pied du Marché n'en sait rien.
  sellers.query_id = market.NextQueryId();
  market.NoteSent(sellers);
  ro::sim::MarketReplyPart refused;
  refused.query_id = sellers.query_id;
  refused.status = ro::sim::MarketStatus::kTooSoon;
  market.Tell(refused);
  CHECK_FALSE(market.aside().waiting());
  CHECK(market.aside_refusal() == ro::sim::MarketStatus::kTooSoon);
  CHECK(market.aside_refusal_seq() == 1);
  CHECK(market.refusal_seq() == 0);
}

TEST_CASE("Marché : un PNJ ne montre que la partie visible de son nom", "[sim][market]") {
  using ro::sim::MarketShownShopName;
  CHECK(MarketShownShopName(MarketKind::kNpc, "Tool Dealer#gefa_0et") == "Tool Dealer");
  CHECK(MarketShownShopName(MarketKind::kNpc, "Tool Dealer") == "Tool Dealer");
  CHECK(MarketShownShopName(MarketKind::kNpc, "Tool Dealer\r#tu") == "Tool Dealer");
  // Le titre d'un joueur garde son dièse : il en fait partie.
  CHECK(MarketShownShopName(MarketKind::kVending, "Potions #1\rpas cher") == "Potions #1");
  CHECK(MarketShownShopName(MarketKind::kBuying, "#1 achète") == "#1 achète");
}
