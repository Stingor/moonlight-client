#include "ro/sim/survivors_ranked.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <span>

#include "ro/sim/survivors_week.h"

namespace ro::sim {

std::optional<SurvivorsEndComponents> EndComponentsOf(const SurvivorsTally& tally,
                                                      std::uint32_t journal_bytes) noexcept {
  switch (tally.outcome) {
    case SurvivorsOutcome::kFell:
    case SurvivorsOutcome::kSurvived:
    case SurvivorsOutcome::kVictory:
      break;
    case SurvivorsOutcome::kRunning:
    case SurvivorsOutcome::kAbandoned:
      return std::nullopt;
  }
  return SurvivorsEndComponents{.survived_steps = tally.survived_steps,
                                .kill_points = tally.kill_points,
                                .kills = tally.kills,
                                .boss_kills = tally.boss_kills,
                                .level = tally.level,
                                .outcome = tally.outcome,
                                .journal_bytes = journal_bytes};
}

std::uint16_t FirstSkillOf(const ClassRule& class_rule, std::uint16_t first_skill_id) noexcept {
  const std::span<const std::uint16_t> starters = class_rule.starter_skills;
  if (std::ranges::find(starters, first_skill_id) != starters.end()) return first_skill_id;
  return starters.empty() ? std::uint16_t{0} : starters.front();
}

SurvivorsStart RankedStartOf(const SurvivorsRules& rules, std::uint64_t seed,
                             std::uint16_t class_id, std::uint16_t first_skill_id) {
  const ArenaRule* ranked_arena = FirstRankedArena(rules);
  const ClassRule* class_rule = FindClass(rules, class_id);
  return SurvivorsStart{
      .seed = seed,
      .class_id = class_id,
      .arena_id = ranked_arena == nullptr ? std::uint16_t{0} : ranked_arena->arena_id,
      .first_skill_id =
          class_rule == nullptr ? first_skill_id : FirstSkillOf(*class_rule, first_skill_id),
      .loadout = {}};
}

bool ChallengeTicketMatches(std::uint64_t ticket_seed, std::int64_t issued_at_utc_s) noexcept {
  // Le jour UTC qui contient l'instant, négatifs compris.
  const IsoWeek week = IsoWeekOfDay(DayOfCivilDate(UtcInstantOf(issued_at_utc_s).date));
  return ticket_seed == SeedOfWeek(week.year, week.week);
}

bool RulesetServed(const SurvivorsRulesetRange& service, std::uint16_t ruleset) noexcept {
  return service.low <= ruleset && ruleset <= service.high;
}

}  // namespace ro::sim
