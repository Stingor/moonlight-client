// LE RÉCAPITULATIF DE FIN : les tués par espèce et les dégâts par skill, que
// l'écran de fin montre, s'écrivent avec le bilan sans le changer.

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/survivors_play.h"
#include "ro/sim/survivors_state.h"

#include "support/survivors_fixture.h"
#include "support/survivors_golden.h"

using ro::sim::CountInRecap;
using ro::sim::SurvivorsCount;
using ro::sim::SurvivorsPlay;
using ro::test_support::kGoldenSeed;
using ro::test_support::kGoldenSteps;
using ro::test_support::kGoldenTally;
using ro::test_support::PlayGolden;

namespace {

[[nodiscard]] std::uint64_t SumOf(const std::vector<SurvivorsCount>& counts) {
  std::uint64_t sum = 0;
  for (const SurvivorsCount& count : counts) sum += count.value;
  return sum;
}

[[nodiscard]] bool IdsAreDistinct(const std::vector<SurvivorsCount>& counts) {
  for (std::size_t first = 0; first < counts.size(); ++first) {
    for (std::size_t second = first + 1; second < counts.size(); ++second) {
      if (counts[first].id == counts[second].id) return false;
    }
  }
  return true;
}

// ⚠ LE RÉCAPITULATIF DE LA PARTIE EN OR COURTE, dans l'ordre de première
// apparition : les mêmes valeurs sous MSVC et sous g++-14, comme son bilan.
inline constexpr std::array<SurvivorsCount, 11> kGoldenKillsByFoe{{
    {.id = 1063, .value = 65},  // Lunatic
    {.id = 1002, .value = 59},  // Poring
    {.id = 1007, .value = 55},  // Fabre
    {.id = 1113, .value = 60},  // Drops
    {.id = 1011, .value = 31},  // Chonchon
    {.id = 1008, .value = 21},  // Pupa
    {.id = 1010, .value = 13},  // Willow
    {.id = 1004, .value = 5},   // Hornet
    {.id = 1009, .value = 5},   // Condor
    {.id = 1052, .value = 1},   // Rocker
    {.id = 1167, .value = 1},   // Savage Babe
}};
inline constexpr std::array<SurvivorsCount, 6> kGoldenDamageBySkill{{
    {.id = 19, .value = 4976},     // Fire Bolt
    {.id = 60001, .value = 1017},  // le coup de baguette
    {.id = 21, .value = 3331},     // Thunderstorm
    {.id = 15, .value = 1022},     // Frost Diver
    {.id = 86, .value = 2488},     // Water Ball
    {.id = 18, .value = 57},       // Fire Wall
}};

}  // namespace

TEST_CASE("CountInRecap : une entrée par identifiant, dans l'ordre de première apparition",
          "[sim][survivors]") {
  std::vector<SurvivorsCount> counts;
  CountInRecap(counts, 19, 5);
  CountInRecap(counts, 13, 2);
  CountInRecap(counts, 19, 7);
  REQUIRE(counts.size() == 2);
  CHECK(counts[0] == SurvivorsCount{.id = 19, .value = 12});
  CHECK(counts[1] == SurvivorsCount{.id = 13, .value = 2});
}

TEST_CASE("Récapitulatif : ses sommes égalent le bilan de la partie en or courte",
          "[sim][survivors]") {
  const std::optional<SurvivorsPlay> play = PlayGolden(kGoldenSeed, kGoldenSteps);
  REQUIRE(play.has_value());
  const ro::sim::SurvivorsField& field = play->run().field();
  // Le bilan en or ne bouge pas : le récapitulatif est hors de lui.
  CHECK(field.tally == kGoldenTally);
  CHECK(SumOf(field.recap.kills_by_foe) == field.tally.kills);
  CHECK(SumOf(field.recap.damage_by_skill) == field.tally.damage_dealt);
  CHECK(IdsAreDistinct(field.recap.kills_by_foe));
  CHECK(IdsAreDistinct(field.recap.damage_by_skill));
  CHECK(field.recap.kills_by_foe ==
        std::vector<SurvivorsCount>(kGoldenKillsByFoe.begin(), kGoldenKillsByFoe.end()));
  CHECK(field.recap.damage_by_skill ==
        std::vector<SurvivorsCount>(kGoldenDamageBySkill.begin(), kGoldenDamageBySkill.end()));
}
