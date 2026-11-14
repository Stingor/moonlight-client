// LA MONTÉE DE NIVEAU DU JOUEUR : l'effet que sa classe et son niveau de
// métier lui donnent.

#include "ro/sim/level_up_effect.h"

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Montée de niveau : l'effet de base suit la classe", "[sim]") {
  using ro::sim::BaseLevelUpEffectId;
  constexpr std::uint16_t kSwordman = 1;
  constexpr std::uint16_t kSuperNovice = 23;
  constexpr std::uint16_t kGangsi = 4050;
  constexpr std::uint16_t kTaekwon = 4046;
  constexpr std::uint16_t kSoulLinker = 4049;
  constexpr std::uint16_t kBabySoulLinker = 4227;
  constexpr std::uint16_t kSkyEmperor = 4302;
  constexpr std::uint16_t kSoulAscetic = 4303;
  constexpr std::uint16_t kAfterSoulAscetic = 4304;
  CHECK(BaseLevelUpEffectId(kSwordman) == 371);
  CHECK(BaseLevelUpEffectId(kSuperNovice) == 338);
  CHECK(BaseLevelUpEffectId(kGangsi) == 338);
  CHECK(BaseLevelUpEffectId(kTaekwon) == 582);
  CHECK(BaseLevelUpEffectId(kSoulLinker) == 582);
  CHECK(BaseLevelUpEffectId(kBabySoulLinker) == 582);
  CHECK(BaseLevelUpEffectId(kSkyEmperor) == 582);
  CHECK(BaseLevelUpEffectId(kSoulAscetic) == 582);
  CHECK(BaseLevelUpEffectId(kAfterSoulAscetic) == 371);
}

TEST_CASE("Montée de niveau : l'effet de métier suit le seul niveau", "[sim]") {
  using ro::sim::JobLevelUpEffectId;
  // Le seuil est strict : le cinquantième niveau garde l'effet ordinaire.
  CHECK(JobLevelUpEffectId(1) == 158);
  CHECK(JobLevelUpEffectId(50) == 158);
  CHECK(JobLevelUpEffectId(51) == 337);
}
