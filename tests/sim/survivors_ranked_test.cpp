// CE QU'UNE PARTIE CLASSÉE DIT AU SERVEUR : ses composantes, son départ
// neutre, le contrôle de la graine du défi et l'intervalle des règles classées.

#include "ro/sim/survivors_ranked.h"

#include <cstdint>
#include <optional>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/survivors_score.h"
#include "ro/sim/survivors_state.h"
#include "ro/sim/survivors_week.h"

#include "support/survivors_golden_terrain.h"

using ro::sim::ChallengeTicketMatches;
using ro::sim::EndComponentsOf;
using ro::sim::RankedStartOf;
using ro::sim::RankingBoundsOf;
using ro::sim::RulesetServed;
using ro::sim::SurvivorsArenaBounds;
using ro::sim::SurvivorsBossBound;
using ro::sim::SurvivorsEndComponents;
using ro::sim::SurvivorsOutcome;
using ro::sim::SurvivorsRulesetRange;
using ro::sim::SurvivorsStart;
using ro::sim::SurvivorsTally;
using ro::test_support::kGoldenTerrainTally;

namespace {

constexpr std::uint32_t kJournalBytes = 6'144;
constexpr std::uint64_t kServerSeed = 0x1234'5678'9ABC'DEF0ULL;
constexpr std::uint16_t kUnknownArenaId = 999;

// Le lundi 2026-10-26 à 00:00 UTC, premier instant de la semaine 2026-W44.
[[nodiscard]] std::int64_t MondayOfWeek44() {
  return ro::sim::DayOfCivilDate(ro::sim::CivilDate{.year = 2026, .month = 10, .day = 26}) *
         ro::sim::kSecondsPerDay;
}

}  // namespace

TEST_CASE("Classement survivors : les composantes d'une victoire et d'une chute",
          "[sim][survivors]") {
  REQUIRE(kGoldenTerrainTally.outcome == SurvivorsOutcome::kVictory);
  const std::optional<SurvivorsEndComponents> victory =
      EndComponentsOf(kGoldenTerrainTally, kJournalBytes);
  REQUIRE(victory.has_value());
  CHECK(*victory == SurvivorsEndComponents{.survived_steps = kGoldenTerrainTally.survived_steps,
                                           .kill_points = kGoldenTerrainTally.kill_points,
                                           .kills = kGoldenTerrainTally.kills,
                                           .boss_kills = kGoldenTerrainTally.boss_kills,
                                           .level = kGoldenTerrainTally.level,
                                           .outcome = SurvivorsOutcome::kVictory,
                                           .journal_bytes = kJournalBytes});

  SurvivorsTally fell = kGoldenTerrainTally;
  fell.outcome = SurvivorsOutcome::kFell;
  const std::optional<SurvivorsEndComponents> fallen = EndComponentsOf(fell, 0);
  REQUIRE(fallen.has_value());
  CHECK(fallen->outcome == SurvivorsOutcome::kFell);
  CHECK(fallen->journal_bytes == 0);

  SurvivorsTally survived = kGoldenTerrainTally;
  survived.outcome = SurvivorsOutcome::kSurvived;
  CHECK(EndComponentsOf(survived, 0).has_value());
}

TEST_CASE("Classement survivors : rien pour un abandon ni une partie en cours",
          "[sim][survivors]") {
  SurvivorsTally abandoned = kGoldenTerrainTally;
  abandoned.outcome = SurvivorsOutcome::kAbandoned;
  CHECK_FALSE(EndComponentsOf(abandoned, kJournalBytes).has_value());
  SurvivorsTally running = kGoldenTerrainTally;
  running.outcome = SurvivorsOutcome::kRunning;
  CHECK_FALSE(EndComponentsOf(running, kJournalBytes).has_value());
}

TEST_CASE("Classement survivors : le départ classé est neutre, sur la graine du serveur",
          "[sim][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  REQUIRE_FALSE(rules.classes.empty());
  REQUIRE_FALSE(rules.arenas.empty());
  const SurvivorsStart start = RankedStartOf(rules, kServerSeed, rules.classes.front().class_id, 0);
  CHECK(ro::sim::IsNeutralStart(start));
  CHECK(start.seed == kServerSeed);
  CHECK(start.first_skill_id == 0);
  CHECK(start.class_id == rules.classes.front().class_id);
  // Sans arène choisie, la première arène classée du catalogue.
  const ro::sim::ArenaRule* ranked_arena = ro::sim::FirstRankedArena(rules);
  REQUIRE(ranked_arena != nullptr);
  CHECK(ranked_arena->ranked);
  CHECK(start.arena_id == ranked_arena->arena_id);
  // Chaque arène livrée est classée, et une partie classée s'y joue.
  for (const ro::sim::ArenaRule& arena : rules.arenas) {
    CHECK(arena.ranked);
    CHECK(RankedStartOf(rules, kServerSeed, start.class_id, arena.arena_id).arena_id ==
          arena.arena_id);
  }
  // Une arène inconnue retombe sur la première.
  CHECK(RankedStartOf(rules, kServerSeed, start.class_id, kUnknownArenaId).arena_id ==
        ranked_arena->arena_id);
}

TEST_CASE("Classement survivors : les bornes de la prairie sont celles que le serveur tient",
          "[sim][survivors]") {
  // Le serveur classe la prairie depuis le ruleset 5 avec ces bornes ; la
  // fonction qui les tire du catalogue doit les retrouver.
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  const ro::sim::ArenaRule* plains = ro::sim::FirstRankedArena(rules);
  REQUIRE(plains != nullptr);
  const SurvivorsArenaBounds bounds = RankingBoundsOf(rules, *plains);
  constexpr std::uint32_t kMinute = 60'000;
  constexpr std::uint32_t kMini = ro::sim::kMiniBossKillFactor;
  constexpr std::uint32_t kBoss = ro::sim::kBossKillFactor;
  CHECK(bounds.bosses == std::vector<SurvivorsBossBound>{
                             {.at_ms = 3 * kMinute, .kill_points = 2 * kMini, .exp = 20},
                             {.at_ms = 5 * kMinute, .kill_points = 6 * kMini, .exp = 30},
                             {.at_ms = 7 * kMinute, .kill_points = 8 * kMini, .exp = 40},
                             {.at_ms = 9 * kMinute, .kill_points = 24 * kMini, .exp = 60},
                             {.at_ms = 15 * kMinute, .kill_points = 81 * kBoss, .final = true},
                         });
  CHECK(bounds.most_common_kill_points == 58 * ro::sim::kEliteKillFactor);
  CHECK(bounds.most_common_kill_exp == 22 * ro::sim::kEliteKillFactor);
}

TEST_CASE("Classement survivors : les bornes de chaque arène, que le serveur recopie",
          "[sim][survivors]") {
  // 🔴 LE SERVEUR RECOPIE CES VALEURS dans son ruleset 8
  // (survivors_ranking_rules.cpp) : une valeur qui change ici se reporte
  // là-bas, sous une version de règles neuve.
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  constexpr std::uint32_t kMinute = 60'000;
  constexpr std::uint32_t kMini = ro::sim::kMiniBossKillFactor;
  constexpr std::uint32_t kBoss = ro::sim::kBossKillFactor;
  constexpr std::uint32_t kElite = ro::sim::kEliteKillFactor;
  struct Expected {
    std::uint16_t arena_id = 0;
    SurvivorsArenaBounds bounds;
  };
  const std::vector<Expected> expected{
      {.arena_id = 3,
       .bounds = {.bosses = {{.at_ms = 3 * kMinute, .kill_points = 2 * kMini, .exp = 20},
                             {.at_ms = 5 * kMinute, .kill_points = 69 * kMini, .exp = 30},
                             {.at_ms = 7 * kMinute, .kill_points = 81 * kMini, .exp = 40},
                             {.at_ms = 9 * kMinute, .kill_points = 24 * kMini, .exp = 60},
                             {.at_ms = 15 * kMinute, .kill_points = 93 * kBoss, .final = true}},
                  .most_common_kill_points = 61 * kElite,
                  .most_common_kill_exp = 20 * kElite}},
      {.arena_id = 4,
       .bounds = {.bosses = {{.at_ms = 3 * kMinute, .kill_points = 42 * kMini, .exp = 25},
                             {.at_ms = 5 * kMinute, .kill_points = 38 * kMini, .exp = 30},
                             {.at_ms = 7 * kMinute, .kill_points = 77 * kMini, .exp = 40},
                             {.at_ms = 9 * kMinute, .kill_points = 72 * kMini, .exp = 55},
                             {.at_ms = 11 * kMinute, .kill_points = 10 * kMini, .exp = 40},
                             {.at_ms = 15 * kMinute, .kill_points = 73 * kBoss, .final = true}},
                  .most_common_kill_points = 61 * kElite,
                  .most_common_kill_exp = 18 * kElite}},
      {.arena_id = 5,
       .bounds = {.bosses = {{.at_ms = 3 * kMinute, .kill_points = 72 * kMini, .exp = 25},
                             {.at_ms = 5 * kMinute, .kill_points = 79 * kMini, .exp = 30},
                             {.at_ms = 7 * kMinute, .kill_points = 99 * kMini, .exp = 40},
                             {.at_ms = 9 * kMinute, .kill_points = 99 * kMini, .exp = 50},
                             {.at_ms = 11 * kMinute, .kill_points = 99 * kMini, .exp = 40},
                             {.at_ms = 15 * kMinute, .kill_points = 99 * kBoss, .final = true}},
                  .most_common_kill_points = 92 * kElite,
                  .most_common_kill_exp = 18 * kElite}},
  };
  for (const Expected& arena : expected) {
    CAPTURE(arena.arena_id);
    const ro::sim::ArenaRule* rule = ro::sim::FindArena(rules, arena.arena_id);
    REQUIRE(rule != nullptr);
    CHECK(RankingBoundsOf(rules, *rule) == arena.bounds);
  }
}

TEST_CASE("Classement survivors : la graine du défi suit la semaine de l'heure du jeton",
          "[sim][survivors]") {
  const std::int64_t monday = MondayOfWeek44();
  const ro::sim::IsoWeek week =
      ro::sim::IsoWeekOfDay(ro::sim::DayOfCivilDate(ro::sim::UtcInstantOf(monday).date));
  REQUIRE(week == ro::sim::IsoWeek{.year = 2026, .week = 44});
  const std::uint64_t seed_44 = ro::sim::SeedOfWeek(2026, 44);
  // Acceptée dès le lundi 00:00 UTC…
  CHECK(ChallengeTicketMatches(seed_44, monday));
  // … refusée le dimanche 23:59:59 d'avant, qui est encore la semaine 43.
  CHECK_FALSE(ChallengeTicketMatches(seed_44, monday - 1));
  CHECK(ChallengeTicketMatches(ro::sim::SeedOfWeek(2026, 43), monday - 1));
  // Une graine d'une autre semaine n'est pas acceptée.
  CHECK_FALSE(ChallengeTicketMatches(ro::sim::SeedOfWeek(2026, 45), monday));
}

TEST_CASE("Classement survivors : l'intervalle des règles classées, bornes comprises",
          "[sim][survivors]") {
  const SurvivorsRulesetRange range{.low = 2, .high = 3};
  CHECK_FALSE(RulesetServed(range, 1));
  CHECK(RulesetServed(range, 2));
  CHECK(RulesetServed(range, 3));
  CHECK_FALSE(RulesetServed(range, 4));
  CHECK(RulesetServed({.low = 2, .high = 2}, 2));
  CHECK_FALSE(RulesetServed({.low = 2, .high = 2}, ro::sim::kSurvivorsRulesetVersion));
  // Un intervalle vide ne classe rien.
  CHECK_FALSE(RulesetServed({.low = 3, .high = 2}, 3));
}
