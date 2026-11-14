// LES COMMANDES DU STAFF SUR LE FAMILIER : le texte qu'une intention de la fiche
// devient, ramené dans les bornes.

#include "game/pet_staff_words.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Staff pet : la faim devient @pethungry, dans ses bornes", "[app][pet_staff]") {
  CHECK(moonclient::PetStaffHungerCommand(42) == "@pethungry 42");
  CHECK(moonclient::PetStaffHungerCommand(0) == "@pethungry 0");
  CHECK(moonclient::PetStaffHungerCommand(-5) == "@pethungry 0");
  CHECK(moonclient::PetStaffHungerCommand(500) == "@pethungry 100");
}

TEST_CASE("Staff pet : l'intimité devient @petfriendly, et jamais zéro", "[app][pet_staff]") {
  CHECK(moonclient::PetStaffIntimacyCommand(910) == "@petfriendly 910");
  CHECK(moonclient::PetStaffIntimacyCommand(1000) == "@petfriendly 1000");
  CHECK(moonclient::PetStaffIntimacyCommand(5000) == "@petfriendly 1000");
  // À zéro, le serveur détruit l'œuf.
  CHECK(moonclient::PetStaffIntimacyCommand(0) == "@petfriendly 1");
  CHECK(moonclient::PetStaffIntimacyCommand(-1) == "@petfriendly 1");
}
