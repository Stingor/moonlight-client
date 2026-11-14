#include "ro/sim/survivors_ranked.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <vector>

#include "ro/sim/survivors_content.h"
#include "ro/sim/survivors_score.h"
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

const ArenaRule* RankedArenaOr(const SurvivorsRules& rules, std::uint16_t arena_id) noexcept {
  const ArenaRule* chosen = arena_id != 0 ? FindArena(rules, arena_id) : nullptr;
  return chosen != nullptr && chosen->ranked ? chosen : FirstRankedArena(rules);
}

SurvivorsStart RankedStartOf(const SurvivorsRules& rules, std::uint64_t seed,
                             std::uint16_t class_id, std::uint16_t arena_id) {
  const ArenaRule* ranked_arena = RankedArenaOr(rules, arena_id);
  return SurvivorsStart{
      .seed = seed,
      .class_id = class_id,
      .arena_id = ranked_arena == nullptr ? std::uint16_t{0} : ranked_arena->arena_id,
      .first_skill_id = 0,
      .loadout = {}};
}

namespace {

// Les points et l'XP d'un tué de l'espèce `foe_id` au plus : en élite, s'il
// est ordinaire.
void RaiseCommonBounds(const SurvivorsRules& rules, std::uint16_t foe_id,
                       SurvivorsArenaBounds& bounds) {
  const FoeRule* foe = FindFoe(rules, foe_id);
  if (foe == nullptr) return;
  const bool can_be_elite = foe->rank == FoeRank::kCommon;
  constexpr std::uint64_t kWholePct = 100;
  const std::uint64_t exp_pct =
      can_be_elite ? std::max<std::uint64_t>(
                         kWholePct, static_cast<std::uint64_t>(std::max(0, rules.elites.exp_pct)))
                   : kWholePct;
  const auto exp = static_cast<std::uint32_t>(std::min<std::uint64_t>(
      std::uint64_t{foe->exp} * exp_pct / kWholePct, std::numeric_limits<std::uint32_t>::max()));
  bounds.most_common_kill_points =
      std::max(bounds.most_common_kill_points, KillPoints(*foe, can_be_elite));
  bounds.most_common_kill_exp = std::max(bounds.most_common_kill_exp, exp);
}

// Les boss d'un tirage, versés rang par rang dans les bornes.
void MergeBosses(const SurvivorsRules& rules, std::span<const BossRule> bosses,
                 SurvivorsArenaBounds& bounds) {
  for (std::size_t slot = 0; slot < bosses.size(); ++slot) {
    const BossRule& boss = bosses[slot];
    const FoeRule* foe = FindFoe(rules, boss.foe_id);
    const SurvivorsBossBound bound{.at_ms = boss.at_ms,
                                   .kill_points = foe == nullptr ? 0U : KillPoints(*foe, false),
                                   .exp = foe == nullptr ? 0U : foe->exp,
                                   .final = boss.final};
    if (slot == bounds.bosses.size()) {
      bounds.bosses.push_back(bound);
      continue;
    }
    SurvivorsBossBound& held = bounds.bosses[slot];
    held.at_ms = std::min(held.at_ms, bound.at_ms);
    held.kill_points = std::max(held.kill_points, bound.kill_points);
    held.exp = std::max(held.exp, bound.exp);
    held.final = held.final || bound.final;
  }
}

}  // namespace

SurvivorsArenaBounds RankingBoundsOf(const SurvivorsRules& rules, const ArenaRule& arena) {
  SurvivorsArenaBounds bounds;
  const ArenaContent content = ContentOf(rules, arena);
  for (const WaveRule& wave : content.waves) {
    for (const std::uint16_t foe_id : wave.foe_ids) RaiseCommonBounds(rules, foe_id, bounds);
  }
  for (const EventRule& event : content.events) RaiseCommonBounds(rules, event.foe_id, bounds);

  std::vector<std::span<const BossRule>> draws;
  if (!arena.waves.empty() && !arena.boss_draws.empty()) {
    draws.assign(arena.boss_draws.begin(), arena.boss_draws.end());
  } else {
    draws.push_back(content.bosses);
  }
  for (const std::span<const BossRule> bosses : draws) {
    MergeBosses(rules, bosses, bounds);
    for (const BossRule& boss : bosses) {
      if (boss.escort_count > 0) RaiseCommonBounds(rules, boss.escort_foe_id, bounds);
      for (const BossMoveRule& move : boss.moves) {
        if (move.kind == BossMoveKind::kSummon) RaiseCommonBounds(rules, move.foe_id, bounds);
      }
    }
  }
  // Le rang garde l'ordre des instants de chaque tirage ; leur minimum aussi.
  std::ranges::stable_sort(bounds.bosses, {}, &SurvivorsBossBound::at_ms);
  return bounds;
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
