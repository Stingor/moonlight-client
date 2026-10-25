#include "ro/sim/survivors_trophies.h"

#include <algorithm>
#include <cstdint>

#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/survivors_state.h"

#include "minigame_trophy_earn.h"

namespace ro::sim {
namespace {

// Les seuils des trophées. 5:00 de partie, en millisecondes.
constexpr std::uint32_t kHoldOutMs = 300000;
constexpr std::uint32_t kManyKills = 1000;
constexpr std::uint16_t kSeasonedLevel = 40;

// Chaque skill possédé est de l'élément Fire, et il y en a au moins un.
[[nodiscard]] bool OnlyFireSkills(const SurvivorsField& field) {
  if (field.rules == nullptr || field.hero.skills.empty()) return false;
  return std::ranges::all_of(field.hero.skills, [&field](const OwnedSkill& owned) {
    const SurvivorsSkillRule* rule = FindSkill(*field.rules, owned.skill_id);
    return rule != nullptr && rule->element == SurvivorsElement::kFire;
  });
}

}  // namespace

std::vector<MinigameTrophy> RecordSurvivors(MinigameRecord& record, const SurvivorsRun& run) {
  std::vector<MinigameTrophy> fresh;
  const SurvivorsField& field = run.field();
  const SurvivorsTally& tally = field.tally;
  const bool victory = tally.outcome == SurvivorsOutcome::kVictory;
  const bool no_card = field.hero.cards.empty();
  EarnMinigameTrophy(record, MinigameTrophy::kSurvivorsFiveMinutes, run.elapsed_ms() >= kHoldOutMs,
                     fresh);
  EarnMinigameTrophy(record, MinigameTrophy::kSurvivorsVictory, victory, fresh);
  EarnMinigameTrophy(record, MinigameTrophy::kSurvivorsFirstEvolution, tally.evolutions > 0, fresh);
  EarnMinigameTrophy(record, MinigameTrophy::kSurvivorsKills1000, tally.kills >= kManyKills, fresh);
  EarnMinigameTrophy(record, MinigameTrophy::kSurvivorsLevel40, tally.level >= kSeasonedLevel,
                     fresh);
  // Première carte : une carte tombée et RAMASSÉE au sol, celle qui entre
  // dans l'Album ; une carte prise dans une offre n'en est pas une.
  EarnMinigameTrophy(record, MinigameTrophy::kSurvivorsFirstCard, !field.recap.cards_found.empty(),
                     fresh);
  EarnMinigameTrophy(record, MinigameTrophy::kSurvivorsPyromaniac, victory && OnlyFireSkills(field),
                     fresh);
  EarnMinigameTrophy(record, MinigameTrophy::kSurvivorsPureMind, victory && no_card, fresh);
  return fresh;
}

}  // namespace ro::sim
