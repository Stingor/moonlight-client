// L'ŒUF D'UN FAMILIER : ses quatre cases sont la fiche du familier, telle que
// le serveur la réécrit pour le fil.

#include "ro/sim/pet_egg.h"

#include <array>
#include <cstdint>

#include <catch2/catch_test_macros.hpp>

using ro::sim::DecodePetEgg;
using ro::sim::IsPetEgg;
using ro::sim::PetEggFacts;

TEST_CASE("Œuf de familier : la plage d'identifiants du client", "[sim][description]") {
  CHECK(IsPetEgg(9001));
  CHECK(IsPetEgg(9499));
  CHECK_FALSE(IsPetEgg(9000));
  CHECK_FALSE(IsPetEgg(9500));
}

TEST_CASE("Œuf de familier : rang d'intimité et renommage", "[sim][description]") {
  const PetEggFacts told = DecodePetEgg(std::array<std::uint32_t, 4>{0, 0, 4, 1});
  CHECK(told.has_pet);
  CHECK(told.intimacy_rank == 4);
  CHECK(told.renamed);
  // Un rang hors bornes est inconnu, pas un sixième rang.
  const PetEggFacts odd = DecodePetEgg(std::array<std::uint32_t, 4>{0, 0, 9, 0});
  CHECK(odd.intimacy_rank == 0);
  CHECK_FALSE(odd.has_pet);
}

TEST_CASE("Œuf de familier : tout à zéro, personne dedans", "[sim][description]") {
  const PetEggFacts empty = DecodePetEgg(std::array<std::uint32_t, 4>{});
  CHECK_FALSE(empty.has_pet);
  CHECK_FALSE(empty.renamed);
  CHECK(empty.intimacy_rank == 0);
}
