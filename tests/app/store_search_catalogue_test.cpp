#include "game/store_search_catalogue.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/store_search.h"
#include "ro/vfs/card_illustrations.h"
#include "ro/vfs/card_item_list.h"
#include "ro/vfs/item_table.h"
#include "ro/vfs/item_words.h"

using moonclient::StoreSearchCardTables;
using moonclient::StoreSearchCatalogueOf;
using ro::sim::PlanStoreSearch;
using ro::sim::StoreSearchCatalogueEntry;
using ro::sim::StoreSearchFault;
using ro::sim::StoreSearchForm;
using ro::sim::StoreSearchPlan;
using ro::vfs::CardIllustrations;
using ro::vfs::CardItemList;
using ro::vfs::ItemEntry;
using ro::vfs::ItemTable;
using ro::vfs::ItemWords;

namespace {

constexpr std::uint32_t kKnifeId = 1201;
constexpr std::uint32_t kPoringCardId = 4001;
constexpr std::uint32_t kFabreCardId = 4002;
// Une carte sans dessin ni mot : seule la liste des cartes la connaît.
constexpr std::uint32_t kBareCardId = 4999;

[[nodiscard]] ItemTable SmallTable() {
  ItemTable table;
  table.Put(kBareCardId, ItemEntry{.name = "Bare Card"});
  table.Put(kKnifeId, ItemEntry{.name = "Knife", .card_slots = 3});
  table.Put(kPoringCardId, ItemEntry{.name = "Poring Card"});
  table.Put(kFabreCardId, ItemEntry{.name = "Fabre Card"});
  return table;
}

[[nodiscard]] ItemWords SmallWords() {
  ItemWords words;
  words.cards[kPoringCardId] = ro::vfs::CardWord{.text = "Poring", .after_name = false};
  return words;
}

[[nodiscard]] CardIllustrations SmallIllustrations() {
  CardIllustrations illustrations;
  illustrations.by_id[kFabreCardId] = "fabre_card";
  return illustrations;
}

[[nodiscard]] StoreSearchPlan PlanCardSearch(
    const std::vector<StoreSearchCatalogueEntry>& catalogue) {
  const StoreSearchForm form{.item_name = "Knife", .card_name = "Bare Card", .similar = false};
  return PlanStoreSearch(form, catalogue);
}

}  // namespace

TEST_CASE("Catalogue des échoppes : une carte connue de la seule liste est acceptée",
          "[app][echoppes]") {
  const ItemTable table = SmallTable();
  const ItemWords words = SmallWords();
  const CardIllustrations illustrations = SmallIllustrations();
  CardItemList listed;
  listed.item_ids.insert(kBareCardId);

  const std::vector<StoreSearchCatalogueEntry> catalogue = StoreSearchCatalogueOf(
      table, words, StoreSearchCardTables{.illustrations = &illustrations, .listed = &listed});

  const StoreSearchPlan plan = PlanCardSearch(catalogue);
  CHECK(plan.fault == StoreSearchFault::kNone);
  CHECK(plan.card_ids == std::vector<std::uint32_t>{kBareCardId});
}

TEST_CASE("Catalogue des échoppes : sans la liste, rien ne change", "[app][echoppes]") {
  // La liste absente — ou vide, ce que rend sa lecture quand le fichier manque —
  // laisse les cartes à leur illustration et à leur mot, comme avant elle.
  const ItemTable table = SmallTable();
  const ItemWords words = SmallWords();
  const CardIllustrations illustrations = SmallIllustrations();
  const CardItemList empty_list;

  for (const CardItemList* listed : {static_cast<const CardItemList*>(nullptr), &empty_list}) {
    const std::vector<StoreSearchCatalogueEntry> catalogue = StoreSearchCatalogueOf(
        table, words, StoreSearchCardTables{.illustrations = &illustrations, .listed = listed});

    CHECK(PlanCardSearch(catalogue).fault == StoreSearchFault::kUnknownCard);
  }
}

TEST_CASE("Catalogue des échoppes : illustration et mot font toujours une carte",
          "[app][echoppes]") {
  const ItemTable table = SmallTable();
  const ItemWords words = SmallWords();
  const CardIllustrations illustrations = SmallIllustrations();

  const std::vector<StoreSearchCatalogueEntry> catalogue =
      StoreSearchCatalogueOf(table, words, StoreSearchCardTables{.illustrations = &illustrations});

  // Trié par identifiant, comme la recherche l'exige.
  REQUIRE(catalogue.size() == 4);
  CHECK(catalogue[0].item_id == kKnifeId);
  CHECK_FALSE(catalogue[0].card);
  CHECK(catalogue[1].item_id == kPoringCardId);
  CHECK(catalogue[1].card);
  CHECK(catalogue[1].card_word == "Poring");
  CHECK(catalogue[2].item_id == kFabreCardId);
  CHECK(catalogue[2].card);
  CHECK(catalogue[2].card_word.empty());
  CHECK(catalogue[3].item_id == kBareCardId);
  CHECK_FALSE(catalogue[3].card);
}
