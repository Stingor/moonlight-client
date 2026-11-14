// LES PANIERS D'ACHAT ENREGISTRÉS SUR LE DISQUE : le texte du fichier, dans
// les deux sens, sans rien lire ni écrire.

#include "game/npc_shop_cart_file.h"

#include <string>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/npc_shop.h"

using moonclient::ReadSavedCartShelf;
using moonclient::SavedCartShelf;
using moonclient::WriteSavedCartShelf;
using ro::sim::SavedCart;

TEST_CASE("les paniers de chaque compte se relisent tels qu'écrits", "[npc_shop_cart_file]") {
  SavedCartShelf shelf;
  constexpr std::uint32_t kFirstAccount = 2000001;
  constexpr std::uint32_t kSecondAccount = 2000002;
  shelf[kFirstAccount] = {
      SavedCart{.name = "Potions",
                .lines = {{.item_id = 501, .amount = 100, .item_name = "Red Potion"},
                          {.item_id = 601, .amount = 20, .item_name = "Fly Wing"}}},
      SavedCart{.name = "Flèches — feu", .lines = {{.item_id = 1752, .amount = 2000}}}};
  shelf[kSecondAccount] = {
      SavedCart{.name = "Gemmes", .lines = {{.item_id = 717, .amount = 50, .item_name = "Gemme"}}}};

  const ro::core::Result<SavedCartShelf> read = ReadSavedCartShelf(WriteSavedCartShelf(shelf));
  REQUIRE(read.has_value());
  CHECK(*read == shelf);
}

TEST_CASE("une ligne ou un panier incomplet se saute, le reste se lit", "[npc_shop_cart_file]") {
  const std::string text =
      "npc_shop_carts:\n"
      "  - aid: 7\n"
      "    carts:\n"
      "      - name: complet\n"
      "        items:\n"
      "          - {id: 501, amount: 3, item: Red Potion}\n"
      "          - {id: 601}\n"
      "          - {id: 0, amount: 4}\n"
      "          - {id: 502, amount: 99999}\n"
      "      - name: vide\n"
      "        items:\n"
      "          - {id: 501, amount: 0}\n"
      "      - items:\n"
      "          - {id: 501, amount: 1}\n"
      "  - aid: 0\n"
      "    carts:\n"
      "      - name: sans compte\n"
      "        items: [{id: 501, amount: 1}]\n";
  const ro::core::Result<SavedCartShelf> read = ReadSavedCartShelf(text);
  REQUIRE(read.has_value());
  REQUIRE(read->size() == 1);
  const std::vector<SavedCart>& mine = read->at(7);
  REQUIRE(mine.size() == 1);
  CHECK(mine[0].name == "complet");
  REQUIRE(mine[0].lines.size() == 2);
  CHECK(mine[0].lines[0].item_name == "Red Potion");
  // Une quantité au-delà d'une pile se ramène à la pile.
  CHECK(mine[0].lines[1].amount == ro::sim::kNpcShopMostPerStack);
}

TEST_CASE("au-delà de vingt paniers, un compte n'en relit que vingt", "[npc_shop_cart_file]") {
  SavedCartShelf shelf;
  std::vector<SavedCart>& carts = shelf[42];
  for (std::size_t rank = 0; rank < ro::sim::kSavedCartLimit + 3; ++rank) {
    carts.push_back(SavedCart{.name = "panier " + std::to_string(rank),
                              .lines = {{.item_id = 501, .amount = 1}}});
  }
  const ro::core::Result<SavedCartShelf> read = ReadSavedCartShelf(WriteSavedCartShelf(shelf));
  REQUIRE(read.has_value());
  CHECK(read->at(42).size() == ro::sim::kSavedCartLimit);
}

TEST_CASE("un texte vide ne range rien, un texte abîmé est refusé", "[npc_shop_cart_file]") {
  const ro::core::Result<SavedCartShelf> empty = ReadSavedCartShelf("");
  REQUIRE(empty.has_value());
  CHECK(empty->empty());
  CHECK_FALSE(ReadSavedCartShelf("npc_shop_carts: [ {aid: 1, carts: [").has_value());
}
