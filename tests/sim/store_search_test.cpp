// LA RECHERCHE D'ÉCHOPPES : le modèle et les contrôles de la saisie.
//
// 🔴 CE BANC PORTE LES RÈGLES DU CLIENT D'ORIGINE qu'aucune session ne
// montrerait vite :
//
//   · un nom se compare par inclusion quand « objets similaires » est coché,
//     par égalité sinon, et le mot d'une carte compte comme un nom ;
//   · un prix n'est lisible que sous sa seule écriture décimale ;
//   · « rien trouvé » efface les résultats et un essai, et la page qui le suit
//     efface le refus ;
//   · un clic allume l'enseigne avant la réponse, un clic refusé l'éteint.

#include "ro/sim/store_search.h"

#include <array>
#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using ro::sim::FindStoreSearchIds;
using ro::sim::PlanStoreSearch;
using ro::sim::ReadStoreSearchPrice;
using ro::sim::StoreSearch;
using ro::sim::StoreSearchCatalogueEntry;
using ro::sim::StoreSearchEffect;
using ro::sim::StoreSearchFault;
using ro::sim::StoreSearchForm;
using ro::sim::StoreSearchKind;
using ro::sim::StoreSearchNameMatches;
using ro::sim::StoreSearchResult;

namespace {

constexpr std::uint32_t kOwnerAid = 2000005;

const std::array<StoreSearchCatalogueEntry, 5> kCatalogue = {{
    {.item_id = 501, .name = "Red Potion"},
    {.item_id = 502, .name = "Orange Potion"},
    {.item_id = 1201, .name = "Knife"},
    {.item_id = 4001, .name = "Poring Card", .card_word = "Lucky", .card = true},
    {.item_id = 4002, .name = "Fabre Card", .card_word = "of Vitality", .card = true},
}};

std::vector<StoreSearchResult> Page(std::size_t count, std::uint32_t first_store) {
  std::vector<StoreSearchResult> page(count);
  for (std::size_t rank = 0; rank < count; ++rank) {
    page[rank].store_id = first_store + static_cast<std::uint32_t>(rank);
    page[rank].owner_aid = kOwnerAid;
  }
  return page;
}

}  // namespace

TEST_CASE("Recherche d'échoppes : un nom se compare par inclusion ou par égalité",
          "[sim][store_search]") {
  CHECK(StoreSearchNameMatches("Red Potion", "Potion", true));
  CHECK_FALSE(StoreSearchNameMatches("Red Potion", "Potion", false));
  CHECK(StoreSearchNameMatches("Red Potion", "Red Potion", false));
  // La casse compte, dans les deux cas.
  CHECK_FALSE(StoreSearchNameMatches("Red Potion", "potion", true));
  // Une saisie plus longue que le nom ne s'y inclut pas.
  CHECK_FALSE(StoreSearchNameMatches("Knife", "Knife [3]", true));
}

TEST_CASE("Recherche d'échoppes : les identifiants suivent l'ordre de la table",
          "[sim][store_search]") {
  CHECK(FindStoreSearchIds(kCatalogue, "Potion", false, true) ==
        std::vector<std::uint32_t>{501, 502});
  CHECK(FindStoreSearchIds(kCatalogue, "Potion", false, false).empty());
  // LE MOT D'UNE CARTE COMPTE COMME UN NOM, dans les deux champs.
  CHECK(FindStoreSearchIds(kCatalogue, "Lucky", false, false) == std::vector<std::uint32_t>{4001});
  CHECK(FindStoreSearchIds(kCatalogue, "Vitality", true, true) == std::vector<std::uint32_t>{4002});
  // Le champ des cartes ne parcourt que les cartes.
  CHECK(FindStoreSearchIds(kCatalogue, "Knife", true, true).empty());
}

TEST_CASE("Recherche d'échoppes : un prix n'a qu'une écriture", "[sim][store_search]") {
  CHECK(ReadStoreSearchPrice("") == 0);
  CHECK(ReadStoreSearchPrice("1500") == 1500);
  CHECK(ReadStoreSearchPrice("-5") == -5);
  CHECK(ReadStoreSearchPrice("2147483647") == 2147483647);
  CHECK_FALSE(ReadStoreSearchPrice("007").has_value());
  CHECK_FALSE(ReadStoreSearchPrice("+5").has_value());
  CHECK_FALSE(ReadStoreSearchPrice("-0").has_value());
  CHECK_FALSE(ReadStoreSearchPrice("1,500").has_value());
  CHECK_FALSE(ReadStoreSearchPrice("2147483648").has_value());
  CHECK_FALSE(ReadStoreSearchPrice("12a").has_value());
}

TEST_CASE("Recherche d'échoppes : la saisie se juge dans l'ordre du client d'origine",
          "[sim][store_search]") {
  StoreSearchForm form;
  CHECK(PlanStoreSearch(form, kCatalogue).fault == StoreSearchFault::kNoItemName);

  form.item_name = "Elixir";
  CHECK(PlanStoreSearch(form, kCatalogue).fault == StoreSearchFault::kUnknownItem);

  form.item_name = "Knife";
  form.card_name = "Ghost";
  CHECK(PlanStoreSearch(form, kCatalogue).fault == StoreSearchFault::kUnknownCard);

  form.card_name = "Card";
  form.min_price = "200";
  form.max_price = "100";
  CHECK(PlanStoreSearch(form, kCatalogue).fault == StoreSearchFault::kBadPrice);

  SECTION("un maximum vide ne borne rien") {
    form.max_price = "";
    const auto plan = PlanStoreSearch(form, kCatalogue);
    CHECK(plan.fault == StoreSearchFault::kNone);
    CHECK(plan.min_price_zeny == 200);
    // Le serveur échangerait un maximum nul avec le minimum : le plafond part.
    CHECK(plan.max_price_zeny == ro::sim::kVendingMostPriceZeny);
    CHECK(plan.item_ids == std::vector<std::uint32_t>{1201});
    CHECK(plan.card_ids == std::vector<std::uint32_t>{4001, 4002});
  }
  SECTION("un prix négatif part réinterprété sans signe") {
    form.min_price = "-1";
    form.kind = StoreSearchKind::kBuying;
    const auto plan = PlanStoreSearch(form, kCatalogue);
    CHECK(plan.fault == StoreSearchFault::kNone);
    CHECK(plan.kind == StoreSearchKind::kBuying);
    CHECK(plan.min_price_zeny == 0xFFFFFFFFU);
  }
  SECTION("plus de vingt identifiants ne partent pas") {
    std::vector<StoreSearchCatalogueEntry> many;
    for (std::uint32_t item_id = 1; item_id <= 21; ++item_id) {
      many.push_back({.item_id = item_id, .name = "Apple"});
    }
    form.item_name = "Apple";
    form.card_name = "";
    form.min_price = "";
    form.max_price = "";
    CHECK(PlanStoreSearch(form, many).fault == StoreSearchFault::kTooManyIds);
    many.pop_back();
    CHECK(PlanStoreSearch(form, many).fault == StoreSearchFault::kNone);
  }
}

TEST_CASE("Recherche d'échoppes : ouverture, pages, refus et fermeture", "[sim][store_search]") {
  StoreSearch search;
  CHECK_FALSE(search.open());
  search.TellOpened(StoreSearchEffect::kOpenRemotely, 5);
  REQUIRE(search.open());
  CHECK(search.opening_seq() == 1);
  CHECK(search.effect() == StoreSearchEffect::kOpenRemotely);
  CHECK(search.remaining_uses() == 5);

  search.TellResults(true, true, 4, Page(10, 100));
  CHECK(search.results().size() == 10);
  CHECK(search.more_on_server());
  CHECK(search.remaining_uses() == 4);
  const std::uint32_t first_page = search.first_page_seq();

  search.TellResults(false, false, 4, Page(3, 200));
  CHECK(search.results().size() == 13);
  CHECK(search.first_page_seq() == first_page);
  CHECK(ro::sim::StoreSearchPage(search.results(), 2).size() == 3);
  CHECK(ro::sim::StoreSearchPage(search.results(), 2)[0].store_id == 200);
  CHECK(ro::sim::StoreSearchPage(search.results(), 3).empty());
  CHECK(ro::sim::StoreSearchPageHeld(search.results().size(), 1));
  CHECK_FALSE(ro::sim::StoreSearchPageHeld(search.results().size(), 2));

  SECTION("rien trouvé efface les résultats et un essai ; la page qui suit efface le refus") {
    search.TellFailure(ro::sim::kStoreSearchNothingFound);
    CHECK(search.results().empty());
    CHECK(search.remaining_uses() == 3);
    CHECK(search.first_page_seq() != first_page);
    CHECK(search.standing_failure() == ro::sim::kStoreSearchNothingFound);
    search.TellResults(true, false, 3, {});
    CHECK_FALSE(search.standing_failure().has_value());
  }
  SECTION("un clic allume l'enseigne, sa position se marque, un refus éteint tout") {
    search.NoteClickSent(kOwnerAid);
    CHECK(search.spotlit_owner_aid() == kOwnerAid);
    search.TellStorePosition(150, 180);
    REQUIRE(search.marker().has_value());
    CHECK(search.marker()->x == 150);
    search.TellStorePosition(-1, -1);
    CHECK_FALSE(search.marker().has_value());
    search.TellFailure(ro::sim::kStoreSearchStoreUnavailable);
    CHECK(search.spotlit_owner_aid() == 0);
  }
  SECTION("la fermeture efface résultats, enseigne et marqueur") {
    search.NoteClickSent(kOwnerAid);
    search.TellStorePosition(10, 20);
    search.Close();
    CHECK_FALSE(search.open());
    CHECK(search.results().empty());
    CHECK(search.spotlit_owner_aid() == 0);
    CHECK_FALSE(search.marker().has_value());
  }
}

TEST_CASE("Recherche d'échoppes : sans minimum, un maximum vide part à zéro",
          "[sim][store_search]") {
  StoreSearchForm form;
  form.item_name = "Knife";
  const auto plan = PlanStoreSearch(form, kCatalogue);
  REQUIRE(plan.fault == StoreSearchFault::kNone);
  CHECK(plan.min_price_zeny == 0);
  CHECK(plan.max_price_zeny == 0);
}

TEST_CASE("Recherche d'échoppes : ce qui arrive hors recherche ouverte est ignoré",
          "[sim][store_search]") {
  StoreSearch search;
  search.TellStorePosition(10, 20);
  search.TellResults(true, true, 3, Page(2, 100));
  search.TellFailure(ro::sim::kStoreSearchNothingFound);
  CHECK_FALSE(search.marker().has_value());
  CHECK(search.results().empty());
  CHECK(search.failure_seq() == 0);

  SECTION("position reçue après la fermeture") {
    search.TellOpened(StoreSearchEffect::kShowPosition, 3);
    search.NoteClickSent(kOwnerAid);
    search.Close();
    search.TellStorePosition(10, 20);
    CHECK_FALSE(search.marker().has_value());
    CHECK(search.spotlit_owner_aid() == 0);
  }
  SECTION("une ouverture neuve efface l'enseigne et le marqueur précédents") {
    search.TellOpened(StoreSearchEffect::kShowPosition, 3);
    search.NoteClickSent(kOwnerAid);
    search.TellStorePosition(10, 20);
    search.TellOpened(StoreSearchEffect::kShowPosition, 2);
    CHECK_FALSE(search.marker().has_value());
    CHECK(search.spotlit_owner_aid() == 0);
  }
}

TEST_CASE("Recherche d'échoppes : la page suivante ne se demande qu'une fois",
          "[sim][store_search]") {
  using ro::sim::StoreSearchMayAskNextPage;
  // Dix reçus, le serveur en a d'autres : depuis la page 1, on demande.
  CHECK(StoreSearchMayAskNextPage(10, 1, true));
  // La demande partie, la fenêtre montre la page 2, vide : rien ne repart.
  CHECK_FALSE(StoreSearchMayAskNextPage(10, 2, true));
  // Une page incomplète est la dernière.
  CHECK_FALSE(StoreSearchMayAskNextPage(13, 2, true));
  CHECK_FALSE(StoreSearchMayAskNextPage(10, 1, false));
  // Depuis une page antérieure, la suivante est déjà reçue.
  CHECK_FALSE(StoreSearchMayAskNextPage(20, 1, true));
  CHECK(StoreSearchMayAskNextPage(20, 2, true));
}

TEST_CASE("Recherche d'échoppes : la fenêtre se ferme une fois, et jamais au changement de carte",
          "[sim][store_search]") {
  using ro::sim::NextStoreSearchWindowStep;
  using ro::sim::StoreSearchWindowStep;
  // Recherche fermée — par nous ou par le serveur : rien ne se dit.
  CHECK(
      NextStoreSearchWindowStep({.search_open = false, .window_open = true, .close_asked = true}) ==
      StoreSearchWindowStep::kHide);
  // Une ouverture neuve remet la fenêtre à neuf, même baissée.
  CHECK(NextStoreSearchWindowStep({.search_open = true, .shows_current_opening = false}) ==
        StoreSearchWindowStep::kReopen);
  // La croix ou Échap : la fermeture se dit.
  CHECK(NextStoreSearchWindowStep(
            {.search_open = true, .shows_current_opening = true, .window_open = false}) ==
        StoreSearchWindowStep::kSendClose);
  CHECK(NextStoreSearchWindowStep({.search_open = true,
                                   .shows_current_opening = true,
                                   .window_open = true,
                                   .close_asked = true}) == StoreSearchWindowStep::kSendClose);
  CHECK(NextStoreSearchWindowStep(
            {.search_open = true, .shows_current_opening = true, .window_open = true}) ==
        StoreSearchWindowStep::kFollow);
  // Une fois la fermeture dite, le modèle est fermé : l'image suivante cache.
  StoreSearch search;
  search.TellOpened(StoreSearchEffect::kShowPosition, 1);
  search.Close();
  CHECK(NextStoreSearchWindowStep(
            {.search_open = search.open(), .shows_current_opening = true, .window_open = false}) ==
        StoreSearchWindowStep::kHide);
}