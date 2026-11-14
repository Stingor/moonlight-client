// LE CARNET DES PANIERS D'ACHAT ENREGISTRÉS : ce qu'il range, ce qu'il refuse,
// et la forme qu'il donne aux noms et aux lignes.

#include "ro/sim/npc_shop_saved_carts.h"

#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/npc_shop.h"

using ro::sim::SavedCartBook;
using ro::sim::SavedCartLine;
using ro::sim::SavedCartRefusal;
using Lines = std::vector<SavedCartLine>;

namespace {

constexpr std::uint32_t kRedPotionId = 501;
constexpr std::uint32_t kFlyWingId = 601;

const std::vector<SavedCartLine> kPotions = {
    {.item_id = kRedPotionId, .amount = 100, .item_name = "Red Potion"},
    {.item_id = kFlyWingId, .amount = 20, .item_name = "Fly Wing"}};

}  // namespace

TEST_CASE("un panier s'enregistre, et le même nom le met à jour à sa place",
          "[npc_shop_saved_carts]") {
  SavedCartBook book;
  REQUIRE(book.Save("Potions", kPotions) == SavedCartRefusal::kNone);
  REQUIRE(book.Save("Ailes", Lines{{.item_id = kFlyWingId, .amount = 5}}) ==
          SavedCartRefusal::kNone);
  REQUIRE(book.Save("  Potions  ", Lines{{.item_id = kRedPotionId, .amount = 7}}) ==
          SavedCartRefusal::kNone);
  REQUIRE(book.all().size() == 2);
  CHECK(book.at(0)->name == "Potions");
  REQUIRE(book.at(0)->lines.size() == 1);
  CHECK(book.at(0)->lines[0].amount == 7);
  CHECK(book.at(1)->name == "Ailes");
}

TEST_CASE("un panier vide ou sans nom ne s'enregistre pas", "[npc_shop_saved_carts]") {
  SavedCartBook book;
  CHECK(book.Save("Rien", Lines{}) == SavedCartRefusal::kEmptyCart);
  // Des lignes sans objet ni quantité ne font pas un panier.
  CHECK(book.Save("Rien",
                  Lines{{.item_id = 0, .amount = 3}, {.item_id = kRedPotionId, .amount = 0}}) ==
        SavedCartRefusal::kEmptyCart);
  CHECK(book.Save(" \t ", kPotions) == SavedCartRefusal::kEmptyName);
  CHECK(book.all().empty());
}

TEST_CASE("le carnet tient vingt paniers, et une mise à jour passe encore",
          "[npc_shop_saved_carts]") {
  SavedCartBook book;
  for (std::size_t rank = 0; rank < ro::sim::kSavedCartLimit; ++rank) {
    REQUIRE(book.Save("panier " + std::to_string(rank), kPotions) == SavedCartRefusal::kNone);
  }
  CHECK(book.Save("un de trop", kPotions) == SavedCartRefusal::kFull);
  CHECK(book.Save("panier 3", Lines{{.item_id = kFlyWingId, .amount = 1}}) ==
        SavedCartRefusal::kNone);
  CHECK(book.all().size() == ro::sim::kSavedCartLimit);
}

TEST_CASE("les lignes d'un même objet se cumulent, bornées à une pile", "[npc_shop_saved_carts]") {
  SavedCartBook book;
  REQUIRE(book.Save("Double", Lines{{.item_id = kRedPotionId, .amount = 20000},
                                    {.item_id = kRedPotionId, .amount = 20000}}) ==
          SavedCartRefusal::kNone);
  REQUIRE(book.at(0)->lines.size() == 1);
  CHECK(book.at(0)->lines[0].amount == ro::sim::kNpcShopMostPerStack);
}

TEST_CASE("le nom se coupe à la saisie sans couper un caractère UTF-8", "[npc_shop_saved_carts]") {
  // Trente « é » : soixante octets, quand la saisie en tient trente et un.
  std::string long_name;
  for (int letter = 0; letter < 30; ++letter) {
    long_name += "é";
  }
  const std::string tidy = ro::sim::TidySavedCartName(long_name);
  CHECK(tidy.size() == 30);
  CHECK(tidy.size() < ro::sim::kSavedCartNameBytes);
  // Un « é » coupé laisserait un octet de tête sans sa suite.
  CHECK((static_cast<unsigned char>(tidy.back()) & 0xC0U) == 0x80U);
}

TEST_CASE("renommer refuse un nom pris, supprimer retire le panier", "[npc_shop_saved_carts]") {
  SavedCartBook book;
  REQUIRE(book.Save("Potions", kPotions) == SavedCartRefusal::kNone);
  REQUIRE(book.Save("Ailes", kPotions) == SavedCartRefusal::kNone);
  CHECK(book.Rename(1, "Potions") == SavedCartRefusal::kNameTaken);
  CHECK(book.Rename(1, "") == SavedCartRefusal::kEmptyName);
  CHECK(book.Rename(1, "Téléport") == SavedCartRefusal::kNone);
  CHECK(book.at(1)->name == "Téléport");
  CHECK(book.Rename(5, "x") == SavedCartRefusal::kUnknown);
  CHECK(book.Remove(0) == SavedCartRefusal::kNone);
  REQUIRE(book.all().size() == 1);
  CHECK(book.at(0)->name == "Téléport");
  CHECK(book.Remove(3) == SavedCartRefusal::kUnknown);
}
