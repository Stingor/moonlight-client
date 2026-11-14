// CE QU'UNE PARTIE CLASSÉE DIT AU SERVEUR : ses composantes, son départ
// neutre, le contrôle de la graine du défi et l'intervalle des règles classées.

#include "ro/sim/survivors_ranked.h"

#include <cstdint>
#include <optional>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/survivors_state.h"
#include "ro/sim/survivors_week.h"

#include "support/survivors_golden_terrain.h"

using ro::sim::ChallengeTicketMatches;
using ro::sim::EndComponentsOf;
using ro::sim::RankedStartOf;
using ro::sim::RulesetServed;
using ro::sim::SurvivorsEndComponents;
using ro::sim::SurvivorsOutcome;
using ro::sim::SurvivorsRulesetRange;
using ro::sim::SurvivorsStart;
using ro::sim::SurvivorsTally;
using ro::test_support::kGoldenTerrainTally;

namespace {

constexpr std::uint32_t kJournalBytes = 6'144;
constexpr std::uint64_t kServerSeed = 0x1234'5678'9ABC'DEF0ULL;

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
  const SurvivorsStart start = RankedStartOf(rules, kServerSeed, rules.classes.front().class_id);
  CHECK(ro::sim::IsNeutralStart(start));
  CHECK(start.seed == kServerSeed);
  CHECK(start.first_skill_id == 0);
  CHECK(start.class_id == rules.classes.front().class_id);
  // L'arène classée du catalogue.
  const ro::sim::ArenaRule* ranked_arena = ro::sim::FirstRankedArena(rules);
  REQUIRE(ranked_arena != nullptr);
  CHECK(ranked_arena->ranked);
  CHECK(start.arena_id == ranked_arena->arena_id);
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
