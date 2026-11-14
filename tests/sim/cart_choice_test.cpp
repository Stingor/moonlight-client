#include "ro/sim/cart_choice.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using ro::sim::CartChoice;
using ro::sim::ChangeCartRefusal;
using ro::sim::ChangeCartRefusalFor;
using ro::sim::ChangeCartTypesFor;

using Types = std::vector<std::uint8_t>;

TEST_CASE("Liste des charrettes : le niveau de base ouvre les types palier par palier",
          "[sim][cart]") {
  // Au niveau un, le premier palier — zéro — est déjà dépassé.
  CHECK(ChangeCartTypesFor(1) == Types{1});
  // Quarante ne DÉPASSE pas quarante.
  CHECK(ChangeCartTypesFor(40) == Types{1});
  CHECK(ChangeCartTypesFor(41) == Types{1, 2});
  CHECK(ChangeCartTypesFor(99) == Types{1, 2, 3, 4, 5});
  // Au-delà de cent, le type treize s'ajoute en fin de liste.
  CHECK(ChangeCartTypesFor(101) == Types{1, 2, 3, 4, 5, 6, 13});
  CHECK(ChangeCartTypesFor(175) == Types{1, 2, 3, 4, 5, 6, 7, 8, 9, 13});
}

TEST_CASE("Liste des charrettes : les points de magie d'abord, la charrette ensuite",
          "[sim][cart]") {
  CHECK(ChangeCartRefusalFor(39, 0) == ChangeCartRefusal::kNotEnoughSp);
  CHECK(ChangeCartRefusalFor(40, 0) == ChangeCartRefusal::kNoCart);
  CHECK(ChangeCartRefusalFor(40, 3) == ChangeCartRefusal::kNone);
}

TEST_CASE("Liste des charrettes : celle du serveur remplace, celle du client ne se refait pas",
          "[sim][cart]") {
  CartChoice choice;
  choice.OfferChange(50);
  REQUIRE(choice.open());
  CHECK(choice.offer().identity == 0);
  CHECK(choice.offer().cart_types == Types{1, 2});
  const std::uint64_t first = choice.serial();

  choice.OfferChange(150);
  CHECK(choice.offer().cart_types == Types{1, 2});
  CHECK(choice.serial() == first);

  const Types offered = {10, 11, 12};
  choice.OfferFromServer(2'000'001, offered);
  CHECK(choice.offer().identity == 2'000'001);
  CHECK(choice.offer().cart_types == offered);
  CHECK(choice.serial() != first);

  choice.Close();
  CHECK_FALSE(choice.open());
  // Une liste vide n'ouvre rien.
  choice.OfferFromServer(2'000'001, Types{});
  CHECK_FALSE(choice.open());
}
