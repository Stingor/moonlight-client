#include "ro/sim/survivors_score.h"

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_state.h"

#include "support/survivors_golden_bot.h"

using ro::sim::FreeRunScore;
using ro::sim::SurvivorsScore;
using ro::test_support::kGoldenBotTally;

namespace {

// Le score de la partie en or longue, que le serveur refera tel quel.
constexpr std::uint64_t kGoldenBotScore = 179442;

}  // namespace

TEST_CASE("Score survivors : la formule serveur ne bouge pas avec la méta", "[sim][survivors]") {
  // 166 972 points de tués + 10 × 807 secondes entières + 100 × niveau 44 ;
  // rien pour une chute.
  CHECK(SurvivorsScore(kGoldenBotTally) == kGoldenBotScore);
}

TEST_CASE("Score survivors : la partie libre majore de 10 % par rang de Cursed Water",
          "[sim][survivors]") {
  CHECK(FreeRunScore(kGoldenBotTally, 0) == kGoldenBotScore);
  CHECK(FreeRunScore(kGoldenBotTally, 1) == kGoldenBotScore * 110 / 100);
  CHECK(FreeRunScore(kGoldenBotTally, ro::sim::kMostCursedWater) == kGoldenBotScore * 150 / 100);
  // La division est entière, après la multiplication : 7 × 130 / 100 = 9.
  ro::sim::SurvivorsTally tiny;
  tiny.level = 0;
  tiny.kill_points = 7;
  CHECK(SurvivorsScore(tiny) == 7U);
  CHECK(FreeRunScore(tiny, 3) == 9U);
}
