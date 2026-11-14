// LA FILE D'UNE APPLICATION DE PRÉRÉGLAGE : une montée à la fois, la suivante
// après la réponse, et l'arrêt au premier écart.

#include "ro/sim/stat_raise_run.h"

#include <optional>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/stat_presets.h"

using ro::sim::BaseStats;
using ro::sim::StatPlan;
using ro::sim::StatRaiseRun;
using ro::sim::StatRaiseSight;
using ro::sim::StatRunStop;

namespace {

constexpr std::int64_t kCostOfTwo = 2;
constexpr std::uint8_t kStrRank = 0;
constexpr std::uint8_t kAgiRank = 1;

// Un plan qui monte STR de `strength` crans puis AGI de `agility`.
[[nodiscard]] StatPlan PlanOf(std::int64_t strength, std::int64_t agility) {
  StatPlan plan;
  for (auto& row : plan.rows) {
    row.current = 1;
    row.after = 1;
  }
  plan.rows[0].after = 1 + strength;
  plan.rows[1].after = 1 + agility;
  return plan;
}

[[nodiscard]] StatRaiseSight Novice(std::int64_t points) {
  StatRaiseSight seen;
  seen.base = {1, 1, 1, 1, 1, 1};
  seen.next_cost = {kCostOfTwo, kCostOfTwo, kCostOfTwo, kCostOfTwo, kCostOfTwo, kCostOfTwo};
  seen.status_point = points;
  return seen;
}

// Ce que le serveur fait d'une montée acceptée : la valeur, les points, la
// fin de l'attente.
void Accept(StatRaiseSight& seen, std::uint8_t rank) {
  ++seen.base[rank];
  seen.status_point -= kCostOfTwo;
  seen.awaited = false;
}

}  // namespace

TEST_CASE("une montée à la fois, la suivante après la réponse", "[stat_raise_run]") {
  StatRaiseRun run;
  StatRaiseSight seen = Novice(10);
  REQUIRE(run.Start(PlanOf(2, 1), seen));
  CHECK(run.total() == 3);

  std::optional<std::uint8_t> rank = run.Tick(seen);
  REQUIRE(rank == kStrRank);
  run.Sent(true, 0);
  seen.awaited = true;
  // La réponse n'est pas là : rien ne part.
  CHECK_FALSE(run.Tick(seen).has_value());
  CHECK(run.running());

  Accept(seen, 0);
  rank = run.Tick(seen);
  REQUIRE(rank == kStrRank);
  run.Sent(true, 1);
  seen.awaited = true;
  Accept(seen, 0);
  rank = run.Tick(seen);
  REQUIRE(rank == kAgiRank);
  run.Sent(true, 2);
  seen.awaited = true;
  Accept(seen, 1);
  CHECK_FALSE(run.Tick(seen).has_value());
  CHECK_FALSE(run.running());
  CHECK(run.stop() == StatRunStop::kFinished);
  CHECK(run.done() == 3);
}

TEST_CASE("un refus du serveur arrête la file", "[stat_raise_run]") {
  StatRaiseRun run;
  StatRaiseSight seen = Novice(10);
  REQUIRE(run.Start(PlanOf(2, 0), seen));
  REQUIRE(run.Tick(seen).has_value());
  run.Sent(true, 0);
  ++seen.refusals;
  CHECK_FALSE(run.Tick(seen).has_value());
  CHECK(run.stop() == StatRunStop::kRefused);
  CHECK(run.done() == 0);
}

TEST_CASE("des points qui changent en cours arrêtent la file", "[stat_raise_run]") {
  StatRaiseRun run;
  StatRaiseSight seen = Novice(10);
  REQUIRE(run.Start(PlanOf(2, 0), seen));
  REQUIRE(run.Tick(seen).has_value());
  run.Sent(true, 0);
  Accept(seen, 0);
  REQUIRE(run.Tick(seen).has_value());
  run.Sent(true, 1);
  Accept(seen, 0);
  // Un niveau gagné juste avant la réponse : les points ne sont pas ceux
  // qu'on attend, la file attend puis s'arrête au délai.
  seen.status_point += 3;
  seen.now_ms = ro::sim::kStatRaiseReplyTimeoutMs + 1;
  CHECK_FALSE(run.Tick(seen).has_value());
  CHECK(run.stop() == StatRunStop::kPointsChanged);
}

TEST_CASE("des points gagnés entre deux montées arrêtent la file", "[stat_raise_run]") {
  StatRaiseRun run;
  StatRaiseSight seen = Novice(10);
  REQUIRE(run.Start(PlanOf(1, 1), seen));
  seen.status_point += 3;
  CHECK_FALSE(run.Tick(seen).has_value());
  CHECK(run.stop() == StatRunStop::kPointsChanged);
}

TEST_CASE("une stat montée à la main arrête la file", "[stat_raise_run]") {
  StatRaiseRun run;
  StatRaiseSight seen = Novice(10);
  REQUIRE(run.Start(PlanOf(1, 1), seen));
  ++seen.base[3];
  CHECK_FALSE(run.Tick(seen).has_value());
  CHECK(run.stop() == StatRunStop::kStatsChanged);
}

TEST_CASE("un point trop cher arrête la file", "[stat_raise_run]") {
  StatRaiseRun run;
  StatRaiseSight seen = Novice(1);
  REQUIRE(run.Start(PlanOf(1, 0), seen));
  CHECK_FALSE(run.Tick(seen).has_value());
  CHECK(run.stop() == StatRunStop::kShort);
}

TEST_CASE("un coût nul — au plafond — arrête la file", "[stat_raise_run]") {
  StatRaiseRun run;
  StatRaiseSight seen = Novice(10);
  seen.next_cost[0] = 0;
  REQUIRE(run.Start(PlanOf(1, 0), seen));
  CHECK_FALSE(run.Tick(seen).has_value());
  CHECK(run.stop() == StatRunStop::kShort);
}

TEST_CASE("un envoi qui n'est pas parti arrête la file", "[stat_raise_run]") {
  StatRaiseRun run;
  StatRaiseSight seen = Novice(10);
  REQUIRE(run.Start(PlanOf(1, 0), seen));
  REQUIRE(run.Tick(seen).has_value());
  run.Sent(false, 0);
  CHECK(run.stop() == StatRunStop::kNotSent);
}

TEST_CASE("sans réponse dans le délai, la file s'arrête", "[stat_raise_run]") {
  StatRaiseRun run;
  StatRaiseSight seen = Novice(10);
  REQUIRE(run.Start(PlanOf(1, 0), seen));
  REQUIRE(run.Tick(seen).has_value());
  run.Sent(true, 0);
  seen.awaited = true;
  seen.now_ms = ro::sim::kStatRaiseReplyTimeoutMs - 1;
  CHECK_FALSE(run.Tick(seen).has_value());
  CHECK(run.running());
  seen.now_ms = ro::sim::kStatRaiseReplyTimeoutMs;
  CHECK_FALSE(run.Tick(seen).has_value());
  CHECK(run.stop() == StatRunStop::kNoReply);
}

TEST_CASE("une réponse en deux trames au-delà de 255 se laisse finir", "[stat_raise_run]") {
  StatRaiseRun run;
  StatRaiseSight seen = Novice(1000);
  seen.base[0] = 255;
  StatPlan plan = PlanOf(0, 0);
  plan.rows[0].current = 255;
  plan.rows[0].after = 256;
  REQUIRE(run.Start(plan, seen));
  REQUIRE(run.Tick(seen).has_value());
  run.Sent(true, 0);
  // L'accusé porte la valeur bornée à 255 : la file attend la correction.
  seen.status_point -= kCostOfTwo;
  CHECK_FALSE(run.Tick(seen).has_value());
  CHECK(run.running());
  seen.base[0] = 256;
  CHECK_FALSE(run.Tick(seen).has_value());
  CHECK(run.stop() == StatRunStop::kFinished);
}

TEST_CASE("une seule application à la fois, et l'arrêt du joueur", "[stat_raise_run]") {
  StatRaiseRun run;
  const StatRaiseSight seen = Novice(10);
  REQUIRE(run.Start(PlanOf(1, 0), seen));
  CHECK_FALSE(run.Start(PlanOf(2, 0), seen));
  run.Cancel();
  CHECK(run.stop() == StatRunStop::kCancelled);
  CHECK_FALSE(run.running());
  // Un plan qui ne monte rien ne lance rien.
  CHECK_FALSE(run.Start(PlanOf(0, 0), seen));
}
