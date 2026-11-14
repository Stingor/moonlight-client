// LA SESSION DE LA BOUTIQUE : la bascule, la fermeture due au serveur, et le
// catalogue qui arrive en morceaux.

#include "ro/sim/cash_shop.h"

#include <array>

#include <catch2/catch_test_macros.hpp>

using ro::sim::CashShop;
using ro::sim::CashShopItem;

TEST_CASE("Boutique : l'ouverture BASCULE, et la fermeture se dit au serveur", "[sim][cash_shop]") {
  CashShop shop;
  shop.TellOpening(120, 30, 6);
  CHECK(shop.open());
  CHECK(shop.vote_points() == 120);
  CHECK(shop.event_points() == 30);
  CHECK(shop.opening_tab() == 6);
  CHECK(shop.opening_seq() == 1);
  CHECK_FALSE(shop.TakeFarewell());

  // La même réponse, boutique ouverte, la ferme — sans reprendre ses points.
  shop.TellOpening(999, 999, 0);
  CHECK_FALSE(shop.open());
  CHECK(shop.vote_points() == 120);
  CHECK(shop.TakeFarewell());
  // Une seule fois.
  CHECK_FALSE(shop.TakeFarewell());
}

TEST_CASE("Boutique : un changement de carte ferme et se dit, un changement de serveur se tait",
          "[sim][cash_shop]") {
  CashShop shop;
  shop.CloseForMapChange();
  CHECK_FALSE(shop.TakeFarewell());

  shop.TellOpening(1, 1, 0);
  shop.CloseForMapChange();
  CHECK_FALSE(shop.open());
  CHECK(shop.TakeFarewell());

  shop.TellOpening(1, 1, 0);
  shop.CloseSilently();
  CHECK_FALSE(shop.open());
  CHECK_FALSE(shop.TakeFarewell());
}

TEST_CASE("Boutique : un onglet coupé en deux paquets s'additionne", "[sim][cash_shop]") {
  CashShop shop;
  const std::array<CashShopItem, 2> first = {CashShopItem{.item_id = 1, .price = 10},
                                             CashShopItem{.item_id = 2, .price = 20}};
  const std::array<CashShopItem, 1> second = {CashShopItem{.item_id = 3, .price = 30}};
  shop.TellCatalogue(6, first);
  shop.TellCatalogue(6, second);
  CHECK(shop.tab(6).size() == 3);

  SECTION("un autre onglet, puis le premier : celui-ci repart de zéro") {
    shop.TellCatalogue(7, second);
    shop.TellCatalogue(6, second);
    CHECK(shop.tab(6).size() == 1);
    CHECK(shop.tab(7).size() == 1);
  }
  SECTION("une salve annoncée vide l'onglet, même s'il finissait la précédente") {
    shop.ExpectCatalogue();
    shop.TellCatalogue(6, second);
    CHECK(shop.tab(6).size() == 1);
  }
  SECTION("un onglet hors de la liste est ignoré") {
    const std::uint32_t before = shop.catalogue_seq();
    shop.TellCatalogue(9, second);
    CHECK(shop.catalogue_seq() == before);
    CHECK(shop.tab(9).empty());
  }
}

TEST_CASE("Boutique : un résultat d'achat porte les soldes et change de rang", "[sim][cash_shop]") {
  CashShop shop;
  CHECK(shop.purchase_seq() == 0);
  shop.TellPurchase(2, 50, 5);
  CHECK(shop.purchase_seq() == 1);
  CHECK(shop.purchase_result() == 2);
  CHECK(shop.vote_points() == 50);
  CHECK(shop.event_points() == 5);
  shop.TellPurchase(2, 50, 5);
  CHECK(shop.purchase_seq() == 2);
}
