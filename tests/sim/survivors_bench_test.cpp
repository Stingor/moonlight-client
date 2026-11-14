#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/log.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/survivors_state.h"

#include "support/survivors_fixture.h"

using ro::sim::SurvivorsRun;
using ro::test_support::PillarArena;
using ro::test_support::ScriptedInput;
using ro::test_support::TinyStart;

namespace {

constexpr std::uint16_t kBenchFoes = 400;
constexpr std::uint32_t kBenchSteps = 3000;
constexpr std::int32_t kBenchArenaCells = 160;
// Assez long pour que les 3 000 pas se jouent tous.
constexpr std::uint32_t kBenchRunMs = 2 * kBenchSteps * ro::sim::kSurvivorsStepMs;
constexpr double kMsPerSecond = 1000.0;

// Les règles minuscules, au plafond de 400 monstres tenus par le plancher, et
// des monstres sans contact : le héros ne tombe pas, la horde reste pleine.
struct BenchRules {
  std::array<ro::sim::FoeRule, 2> foes = ro::test_support::kTinyFoes;
  std::array<ro::sim::WaveRule, 1> waves = ro::test_support::kTinyWaves;
  ro::sim::SurvivorsRules rules = ro::test_support::TinyRules();

  BenchRules() {
    for (ro::sim::FoeRule& foe : foes) foe.touch_damage = 0;
    waves[0].floor = kBenchFoes;
    rules.foes = foes;
    rules.waves = waves;
    rules.run_ms = kBenchRunMs;
    rules.caps.most_foes = kBenchFoes;
  }
  BenchRules(const BenchRules&) = delete;
  BenchRules& operator=(const BenchRules&) = delete;
};

}  // namespace

// Un banc, lancé à la main : il mesure, il ne juge pas. Ses chiffres ne valent
// que pour la configuration bâtie (Debug ou Release), qu'il faut citer avec eux.
TEST_CASE("Banc survivors : 400 monstres au plancher, 3 000 pas", "[sim][survivors][.bench]") {
  const BenchRules bench;
  auto run = SurvivorsRun::Start(bench.rules, PillarArena(kBenchArenaCells), TinyStart(7));
  REQUIRE(run.has_value());

  using Clock = std::chrono::steady_clock;
  double total_ms = 0.0;
  double worst_ms = 0.0;
  std::uint32_t steps = 0;
  std::size_t most_alive = 0;
  while (steps < kBenchSteps && run->field().phase != ro::sim::SurvivorsPhase::kOver) {
    if (run->field().phase == ro::sim::SurvivorsPhase::kChoosing) {
      REQUIRE(run->Answer({.kind = ro::sim::AnswerKind::kPick, .offer_index = 0}));
      continue;
    }
    const Clock::time_point before = Clock::now();
    run->Step(ScriptedInput(run->field()));
    const double spent_ms =
        std::chrono::duration<double, std::milli>(Clock::now() - before).count();
    total_ms += spent_ms;
    worst_ms = std::max(worst_ms, spent_ms);
    most_alive = std::max(most_alive, run->field().foes.size());
    ++steps;
  }
  const double simulated_s = steps * static_cast<double>(ro::sim::kSurvivorsStepMs) / kMsPerSecond;
  ro::core::LogInfo(
      "banc survivors : {} pas, {:.3f} ms par pas en moyenne, {:.3f} ms au pire, {} monstres au "
      "plus, {} inondations ({:.2f} par seconde simulée)",
      steps, total_ms / steps, worst_ms, most_alive, run->field().flow.floods(),
      run->field().flow.floods() / simulated_s);
  CHECK(steps == kBenchSteps);
  CHECK(most_alive == kBenchFoes);
}
