// L'ÉCHOPPE D'ACHAT, sans fil : ses transitions, et ce qu'on refuse avant
// d'émettre.
//
// 🔴 CE BANC PORTE CE QU'AUCUNE SESSION NE MONTRERAIT VITE :
//
//   · un refus d'ouverture PERD la préparation ;
//   · le journal survit à la fermeture, et ne se vide qu'à l'ouverture suivante ;
//   · une fermeture demandée avant la réponse fait ignorer la liste qui suit ;
//   · une vente cumule les piles d'un même objet, ce que le serveur ne fait pas.

#include "ro/sim/buying_store.h"

#include <array>
#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/buying_store_checks.h"
#include "ro/sim/inventory.h"

using ro::sim::BuyingStore;
using ro::sim::BuyingStoreDraftFault;
using ro::sim::BuyingStoreLine;
using ro::sim::BuyingStorePhase;
using ro::sim::BuyingStorePurchase;
using ro::sim::BuyingStoreSaleFault;
using ro::sim::BuyingStoreSaleLine;
using ro::sim::BuyingStoreWish;
using ro::sim::InventoryItem;
using ro::sim::VisitedBuyingStore;

namespace {

constexpr std::uint32_t kJellopy = 909;
constexpr std::uint32_t kRedPotion = 501;
constexpr std::uint32_t kSword = 1101;

// Mène une échoppe jusqu'à l'ouverture : deux lignes, 10 000 z de limite.
void OpenStore(BuyingStore& store) {
  store.TellPreparation(2);
  REQUIRE(store.TellRequested("Achete Jellopy", 10'000));
  const std::array<BuyingStoreLine, 2> lines = {
      BuyingStoreLine{.item_id = kJellopy, .item_type = 3, .wanted = 100, .price = 50},
      BuyingStoreLine{.item_id = kRedPotion, .item_type = 0, .wanted = 5, .price = 400}};
  store.TellOpened(2'000'001, 10'000, lines);
}

InventoryItem Stack(std::uint16_t slot, std::uint32_t item_id, std::int16_t amount) {
  InventoryItem item;
  item.client_slot = slot;
  item.item_id = item_id;
  item.amount = amount;
  return item;
}

template <typename Problems, typename Fault>
bool Has(const Problems& problems, Fault fault, std::size_t line = ro::sim::kNoFaultLine) {
  for (const auto& problem : problems) {
    if (problem.fault == fault && problem.line == line) {
      return true;
    }
  }
  return false;
}

}  // namespace

TEST_CASE("Échoppe d'achat : de la préparation à l'ouverture", "[buying_store]") {
  BuyingStore store;
  CHECK(store.phase() == BuyingStorePhase::kClosed);
  // Hors préparation, aucune demande ne part.
  CHECK_FALSE(store.TellRequested("x", 1));

  store.TellPreparation(9);
  CHECK(store.phase() == BuyingStorePhase::kPreparing);
  // Le serveur n'en permet jamais plus de cinq.
  CHECK(store.slots() == ro::sim::kBuyingStoreMostSlots);
  CHECK(store.preparation_seq() == 1);

  REQUIRE(store.TellRequested("Achete Jellopy", 10'000));
  CHECK(store.phase() == BuyingStorePhase::kRequested);
  // Une seconde demande ne part pas.
  CHECK_FALSE(store.TellRequested("Encore", 1));

  const std::array<BuyingStoreLine, 1> lines = {
      BuyingStoreLine{.item_id = kJellopy, .item_type = 3, .wanted = 100, .price = 50}};
  store.TellOpened(2'000'001, 10'000, lines);
  CHECK(store.open());
  CHECK(store.title() == "Achete Jellopy");
  CHECK(store.owner_account_id() == 2'000'001);
  CHECK(store.zeny_limit() == 10'000);
  CHECK(store.opening_zeny_limit() == 10'000);
  REQUIRE(store.lines().size() == 1);
  CHECK(store.opening_seq() == 1);

  // Une préparation arrivée échoppe ouverte ne ferme rien.
  store.TellPreparation(3);
  CHECK(store.open());
}

TEST_CASE("Échoppe d'achat : un refus perd la préparation", "[buying_store]") {
  BuyingStore store;
  store.TellPreparation(2);
  REQUIRE(store.TellRequested("x", 1));
  store.TellOpenRefused(ro::sim::kBuyingStoreOpenOverweight, 24'000);
  CHECK(store.phase() == BuyingStorePhase::kClosed);
  CHECK(store.slots() == 0);
  CHECK(store.open_refusal() == ro::sim::kBuyingStoreOpenOverweight);
  CHECK(store.open_refusal_weight() == 24'000);
  CHECK(store.open_refusal_seq() == 1);
  CHECK_FALSE(store.TellRequested("x", 1));
}

TEST_CASE("Échoppe d'achat : la renonciation ne se dit pas comme une fermeture", "[buying_store]") {
  BuyingStore store;
  store.TellPreparation(2);
  store.Close();
  CHECK(store.phase() == BuyingStorePhase::kClosed);
  CHECK_FALSE(store.TakeFarewell());
}

TEST_CASE("Échoppe d'achat : les achats décrémentent et s'inscrivent au journal",
          "[buying_store]") {
  BuyingStore store;
  OpenStore(store);
  store.TellPurchase(BuyingStorePurchase{.item_id = kJellopy,
                                         .amount = 30,
                                         .zeny = 1'500,
                                         .seller_char_id = 150'000,
                                         .unix_s = 1'790'000'000},
                     8'500);
  CHECK(store.lines()[0].wanted == 70);
  CHECK(store.zeny_limit() == 8'500);
  REQUIRE(store.journal().size() == 1);
  CHECK(store.journal()[0].seller_char_id == 150'000);
  CHECK(store.journal()[0].unix_s == 1'790'000'000);
  CHECK(store.spent_zeny() == 1'500);

  // Une ligne ne passe jamais sous zéro.
  store.TellPurchase(BuyingStorePurchase{.item_id = kRedPotion, .amount = 9, .zeny = 3'600}, 4'900);
  CHECK(store.lines()[1].wanted == 0);
  CHECK(store.spent_zeny() == 5'100);

  // La fin ferme l'échoppe, sans rien à dire au serveur, et garde le journal.
  store.TellEnded(ro::sim::kBuyingStoreEndAllBought);
  CHECK_FALSE(store.open());
  CHECK(store.end_result() == ro::sim::kBuyingStoreEndAllBought);
  CHECK(store.end_seq() == 1);
  CHECK_FALSE(store.TakeFarewell());
  CHECK(store.journal().size() == 2);

  // L'ouverture suivante repart d'un journal vide.
  OpenStore(store);
  CHECK(store.journal().empty());
  CHECK(store.spent_zeny() == 0);
}

TEST_CASE("Échoppe d'achat : fermer se dit au serveur, une fois", "[buying_store]") {
  BuyingStore store;
  OpenStore(store);
  store.TellPurchase(BuyingStorePurchase{.item_id = kJellopy, .amount = 1, .zeny = 50}, 9'950);
  store.Close();
  CHECK_FALSE(store.open());
  CHECK(store.TakeFarewell());
  CHECK_FALSE(store.TakeFarewell());
  CHECK(store.journal().size() == 1);
}

TEST_CASE("Échoppe d'achat : fermée avant la réponse, la liste n'ouvre plus rien",
          "[buying_store]") {
  BuyingStore store;
  store.TellPreparation(2);
  REQUIRE(store.TellRequested("x", 100));
  store.Close();
  CHECK(store.TakeFarewell());
  store.TellOpened(1, 100, {});
  CHECK_FALSE(store.open());
  // La suivante, elle, ouvre.
  OpenStore(store);
  CHECK(store.open());
}

TEST_CASE("Échoppe d'achat : les changements de carte", "[buying_store]") {
  BuyingStore store;
  OpenStore(store);
  store.TellVisit(VisitedBuyingStore{.buyer_account_id = 7, .store_id = 1, .zeny_limit = 10});
  store.CloseForMapChange();
  // Même serveur : l'échoppe court toujours, la visite est perdue.
  CHECK(store.open());
  CHECK_FALSE(store.visiting());
  CHECK_FALSE(store.TakeFarewell());

  store.CloseSilently();
  CHECK_FALSE(store.open());
  CHECK_FALSE(store.TakeFarewell());
}

TEST_CASE("Échoppe d'achat : une vente confirmée décrémente l'échoppe visitée", "[buying_store]") {
  BuyingStore store;
  VisitedBuyingStore visited;
  visited.buyer_account_id = 2'000'001;
  visited.store_id = 42;
  visited.zeny_limit = 10'000;
  visited.lines = {BuyingStoreLine{.item_id = kJellopy, .wanted = 100, .price = 50},
                   BuyingStoreLine{.item_id = kRedPotion, .wanted = 0, .price = 400}};
  store.TellVisit(visited);
  REQUIRE(store.visiting());

  const std::array<BuyingStoreSaleLine, 2> sale = {
      BuyingStoreSaleLine{.client_slot = 7, .item_id = kJellopy, .amount = 30},
      BuyingStoreSaleLine{.client_slot = 9, .item_id = kJellopy, .amount = 10}};
  REQUIRE(store.TellSaleRequested(sale));
  CHECK(store.sale_pending());
  // Une seconde vente attend la réponse de la première.
  CHECK_FALSE(store.TellSaleRequested(sale));

  store.TellSold(7, 30, 50);
  CHECK(store.sale_pending());
  store.TellSold(9, 10, 50);
  CHECK_FALSE(store.sale_pending());
  REQUIRE(store.sales().size() == 2);
  CHECK(store.sales()[0].item_id == kJellopy);
  CHECK(store.sales()[0].zeny == 1'500);
  CHECK(store.earned_zeny() == 2'000);
  CHECK(store.visited().lines[0].wanted == 60);
  CHECK(store.visited().zeny_limit == 8'000);

  // Un refus lève l'attente.
  REQUIRE(store.TellSaleRequested(sale));
  store.TellSaleRefused(ro::sim::kBuyingStoreSaleOverCount, kJellopy);
  CHECK_FALSE(store.sale_pending());
  CHECK(store.sale_refusal() == ro::sim::kBuyingStoreSaleOverCount);
  CHECK(store.sale_refusal_item() == kJellopy);

  // La même échoppe relue garde ses ventes ; une autre les efface.
  store.TellVisit(visited);
  CHECK(store.sales().size() == 2);
  visited.store_id = 43;
  store.TellVisit(visited);
  CHECK(store.sales().empty());
  CHECK(store.earned_zeny() == 0);

  store.LeaveVisit();
  CHECK_FALSE(store.visiting());
  CHECK_FALSE(store.TellSaleRequested(sale));
}

TEST_CASE("Échoppe d'achat : la préparation nomme chaque faute et sa ligne", "[buying_store]") {
  const std::vector<InventoryItem> bag = {Stack(2, kJellopy, 9'950), Stack(3, kRedPotion, 1),
                                          Stack(4, kJellopy, 5)};

  SECTION("une échoppe correcte ne vaut aucun problème") {
    const std::array<BuyingStoreWish, 2> lines = {
        BuyingStoreWish{.item_id = kJellopy, .amount = 49, .price = 10},
        BuyingStoreWish{.item_id = kRedPotion, .amount = 10, .price = 100}};
    CHECK(ro::sim::CheckBuyingStoreDraft("Achat", 1'490, lines, 2, bag, 5'000).empty());
    CHECK(ro::sim::BuyingStoreDraftCost(lines) == 1'490);
  }

  SECTION("vide, sans titre, sans limite") {
    const auto problems = ro::sim::CheckBuyingStoreDraft("", 0, {}, 2, bag, 5'000);
    CHECK(Has(problems, BuyingStoreDraftFault::kNoLine));
    CHECK(Has(problems, BuyingStoreDraftFault::kEmptyTitle));
    CHECK(Has(problems, BuyingStoreDraftFault::kLimitNotPositive));
  }

  SECTION("les fautes de ligne") {
    const std::array<BuyingStoreWish, 5> lines = {
        // La PREMIÈRE pile compte : 9 950 + 51 dépasse 9 999.
        BuyingStoreWish{.item_id = kJellopy, .amount = 51, .price = 10},
        BuyingStoreWish{.item_id = kJellopy, .amount = 1, .price = 10},
        BuyingStoreWish{.item_id = kRedPotion, .amount = 0, .price = 0},
        BuyingStoreWish{.item_id = kSword, .amount = 1, .price = 99'990'001},
        BuyingStoreWish{.item_id = 12'345, .amount = 1, .price = 1}};
    const auto problems = ro::sim::CheckBuyingStoreDraft("Achat", 900, lines, 4, bag, 800);
    CHECK(Has(problems, BuyingStoreDraftFault::kTooManyLines));
    CHECK(Has(problems, BuyingStoreDraftFault::kOwnedAboveMost, 0));
    CHECK(Has(problems, BuyingStoreDraftFault::kDuplicateItem, 1));
    CHECK(Has(problems, BuyingStoreDraftFault::kZeroAmount, 2));
    CHECK(Has(problems, BuyingStoreDraftFault::kZeroPrice, 2));
    CHECK(Has(problems, BuyingStoreDraftFault::kPriceAboveMost, 3));
    CHECK(Has(problems, BuyingStoreDraftFault::kNotInBag, 3));
    CHECK(Has(problems, BuyingStoreDraftFault::kNotInBag, 4));
    CHECK(Has(problems, BuyingStoreDraftFault::kLimitAboveZeny));
  }

  SECTION("une limite trop courte n'est qu'un avertissement") {
    const std::array<BuyingStoreWish, 1> lines = {
        BuyingStoreWish{.item_id = kRedPotion, .amount = 10, .price = 100}};
    const auto problems = ro::sim::CheckBuyingStoreDraft("Achat", 500, lines, 2, bag, 5'000);
    REQUIRE(problems.size() == 1);
    CHECK(problems[0].fault == BuyingStoreDraftFault::kLimitBelowTotalCost);
    CHECK_FALSE(ro::sim::BlocksOpening(problems[0].fault));
    CHECK(ro::sim::BlocksOpening(BuyingStoreDraftFault::kNotInBag));
  }

  SECTION("la base du serveur refuse un item, ligne nommée") {
    const std::array<BuyingStoreWish, 2> lines = {
        BuyingStoreWish{.item_id = kJellopy, .amount = 10, .price = 10},
        BuyingStoreWish{.item_id = kRedPotion, .amount = 10, .price = 10}};
    const std::array<ro::sim::BuyingStoreItemFacts, 2> facts = {
        ro::sim::BuyingStoreItemFacts{.allowed = true, .unit_weight = 10},
        ro::sim::BuyingStoreItemFacts{.allowed = false, .unit_weight = 70}};
    const auto problems = ro::sim::CheckBuyingStoreDraft("Achat", 200, lines, 2, bag, 5'000, facts,
                                                         {.weight = 0, .max_weight = 8'000});
    REQUIRE(problems.size() == 1);
    CHECK(Has(problems, BuyingStoreDraftFault::kNotForBuyingStore, 1));
    CHECK(ro::sim::BlocksOpening(BuyingStoreDraftFault::kNotForBuyingStore));
  }

  SECTION("la surcharge se juge comme le serveur : 90 pour cent du maximum, atteints") {
    const std::array<BuyingStoreWish, 1> lines = {
        BuyingStoreWish{.item_id = kRedPotion, .amount = 100, .price = 1}};
    const std::array<ro::sim::BuyingStoreItemFacts, 1> facts = {
        ro::sim::BuyingStoreItemFacts{.allowed = true, .unit_weight = 10}};
    // 6 200 + 100 × 10 = 7 200 : 90 pour cent de 8 000, tout juste.
    const auto heavy = ro::sim::CheckBuyingStoreDraft("Achat", 100, lines, 2, bag, 5'000, facts,
                                                      {.weight = 6'200, .max_weight = 8'000});
    CHECK(Has(heavy, BuyingStoreDraftFault::kOverweight));
    CHECK(ro::sim::BlocksOpening(BuyingStoreDraftFault::kOverweight));
    // Un dixième de moins : 89 pour cent, accepté.
    CHECK(ro::sim::CheckBuyingStoreDraft("Achat", 100, lines, 2, bag, 5'000, facts,
                                         {.weight = 6'190, .max_weight = 8'000})
              .empty());
  }

  SECTION("sans la table, ni le drapeau ni le poids ne se jugent") {
    const std::array<BuyingStoreWish, 2> lines = {
        BuyingStoreWish{.item_id = kRedPotion, .amount = 100, .price = 1},
        BuyingStoreWish{.item_id = kJellopy, .amount = 1, .price = 1}};
    const ro::sim::CarriedWeight loaded{.weight = 7'999, .max_weight = 8'000};
    // Aucun fait : rien.
    CHECK(ro::sim::CheckBuyingStoreDraft("Achat", 101, lines, 2, bag, 5'000, {}, loaded).empty());
    // Un poids inconnu sur une ligne : la surcharge ne se juge pas.
    const std::array<ro::sim::BuyingStoreItemFacts, 2> partial = {
        ro::sim::BuyingStoreItemFacts{.allowed = true, .unit_weight = 10},
        ro::sim::BuyingStoreItemFacts{}};
    CHECK(ro::sim::CheckBuyingStoreDraft("Achat", 101, lines, 2, bag, 5'000, partial, loaded)
              .empty());
    // Le poids du personnage pas encore dit : non plus.
    const std::array<ro::sim::BuyingStoreItemFacts, 2> weighed = {
        ro::sim::BuyingStoreItemFacts{.allowed = true, .unit_weight = 10},
        ro::sim::BuyingStoreItemFacts{.allowed = true, .unit_weight = 70}};
    CHECK(ro::sim::CheckBuyingStoreDraft("Achat", 101, lines, 2, bag, 5'000, weighed, {}).empty());
  }
}

TEST_CASE("Échoppe d'achat : la vente nomme chaque faute et sa ligne", "[buying_store]") {
  VisitedBuyingStore store;
  store.zeny_limit = 1'000;
  store.lines = {BuyingStoreLine{.item_id = kJellopy, .wanted = 20, .price = 10},
                 BuyingStoreLine{.item_id = kRedPotion, .wanted = 0, .price = 100},
                 BuyingStoreLine{.item_id = kSword, .wanted = 5, .price = 100}};

  InventoryItem carded = Stack(5, kSword, 1);
  carded.cards[3] = 4001;
  InventoryItem bound = Stack(6, kSword, 1);
  bound.bind_on_equip = ro::sim::kItemBoundToOwner;
  InventoryItem worn = Stack(8, kSword, 1);
  worn.worn_at = 2;
  InventoryItem rented = Stack(9, kSword, 1);
  rented.rental_until_unix_s = 1'800'000'000;
  const std::vector<InventoryItem> bag = {Stack(2, kJellopy, 15),
                                          Stack(3, kJellopy, 15),
                                          Stack(4, kRedPotion, 3),
                                          carded,
                                          bound,
                                          worn,
                                          rented};

  SECTION("une vente correcte ne vaut aucun problème") {
    const std::array<BuyingStoreSaleLine, 1> lines = {
        BuyingStoreSaleLine{.client_slot = 2, .item_id = kJellopy, .amount = 15}};
    CHECK(ro::sim::CheckBuyingStoreSale(store, lines, bag, 0).empty());
    CHECK(ro::sim::BuyingStoreSaleTotal(store, lines) == 150);
  }

  SECTION("les fautes de ligne") {
    const std::array<BuyingStoreSaleLine, 9> lines = {
        BuyingStoreSaleLine{.client_slot = 2, .item_id = kJellopy, .amount = 15},
        // Deux piles du même objet : 15 + 15 dépasse les 20 voulus.
        BuyingStoreSaleLine{.client_slot = 3, .item_id = kJellopy, .amount = 15},
        BuyingStoreSaleLine{.client_slot = 2, .item_id = kJellopy, .amount = 1},
        BuyingStoreSaleLine{.client_slot = 4, .item_id = kRedPotion, .amount = 1},
        BuyingStoreSaleLine{.client_slot = 5, .item_id = kSword, .amount = 1},
        BuyingStoreSaleLine{.client_slot = 6, .item_id = kSword, .amount = 1},
        BuyingStoreSaleLine{.client_slot = 8, .item_id = kSword, .amount = 1},
        BuyingStoreSaleLine{.client_slot = 9, .item_id = kSword, .amount = 0},
        BuyingStoreSaleLine{.client_slot = 40, .item_id = kSword, .amount = 1}};
    const auto problems = ro::sim::CheckBuyingStoreSale(store, lines, bag, 0);
    CHECK(Has(problems, BuyingStoreSaleFault::kAboveWanted, 1));
    CHECK(Has(problems, BuyingStoreSaleFault::kDuplicateSlot, 2));
    CHECK(Has(problems, BuyingStoreSaleFault::kNotWanted, 3));
    CHECK(Has(problems, BuyingStoreSaleFault::kCarded, 4));
    CHECK(Has(problems, BuyingStoreSaleFault::kBound, 5));
    CHECK(Has(problems, BuyingStoreSaleFault::kEquipped, 6));
    CHECK(Has(problems, BuyingStoreSaleFault::kZeroAmount, 7));
    CHECK(Has(problems, BuyingStoreSaleFault::kRented, 7));
    CHECK(Has(problems, BuyingStoreSaleFault::kNotInBag, 8));
  }

  SECTION("les fautes de total") {
    const std::array<BuyingStoreSaleLine, 1> lines = {
        BuyingStoreSaleLine{.client_slot = 2, .item_id = kJellopy, .amount = 15}};
    store.zeny_limit = 100;
    const auto problems =
        ro::sim::CheckBuyingStoreSale(store, lines, bag, ro::sim::kBuyingStoreMostZeny - 10);
    CHECK(Has(problems, BuyingStoreSaleFault::kAboveBuyerLimit));
    CHECK(Has(problems, BuyingStoreSaleFault::kAboveMyPurse));
  }

  SECTION("aucune ligne") {
    CHECK(Has(ro::sim::CheckBuyingStoreSale(store, {}, bag, 0), BuyingStoreSaleFault::kNoLine));
  }
}
