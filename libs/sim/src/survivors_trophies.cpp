#include "ro/sim/survivors_trophies.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/survivors_state.h"

#include "minigame_trophy_earn.h"
#include "survivors_shipped.h"

namespace ro::sim {
namespace {

// Les seuils des trophées. 5:00 de partie, en millisecondes.
constexpr std::uint32_t kHoldOutMs = 300000;
constexpr std::uint32_t kManyKills = 1000;
constexpr std::uint16_t kSeasonedLevel = 40;
constexpr std::uint64_t kManyFreezes = 100;

// Les trophées d'un skill évolué possédé, chacun le sien.
struct EvolvedTrophy {
  std::uint16_t skill_id;
  MinigameTrophy trophy;
};
constexpr std::array<EvolvedTrophy, 5> kEvolvedTrophies{{
    {survivors::kStormGustId, MinigameTrophy::kSurvivorsStormGust},
    {survivors::kJupitelThunderId, MinigameTrophy::kSurvivorsJupitelThunder},
    {survivors::kLordOfVermilionId, MinigameTrophy::kSurvivorsLordOfVermilion},
    {survivors::kHeavensDriveId, MinigameTrophy::kSurvivorsHeavensDrive},
    {survivors::kNapalmVulcanId, MinigameTrophy::kSurvivorsNapalmVulcan},
}};

[[nodiscard]] bool Owns(const SurvivorsField& field, std::uint16_t skill_id) {
  return std::ranges::any_of(field.hero.skills, [skill_id](const OwnedSkill& owned) {
    return owned.skill_id == skill_id;
  });
}

// Ajoute au record les évolués que la partie possède, et dit si le record les
// tient tous : chacun des évolués des règles, au moins un.
[[nodiscard]] bool RememberEvolved(MinigameRecord& record, const SurvivorsField& field) {
  if (field.rules == nullptr || field.rules->evolutions.empty()) return false;
  std::vector<std::uint16_t>& kept = record.survivors_evolved;
  for (const EvolutionRule& evolution : field.rules->evolutions) {
    if (Owns(field, evolution.evolved_skill_id) &&
        !std::ranges::binary_search(kept, evolution.evolved_skill_id)) {
      kept.insert(std::ranges::upper_bound(kept, evolution.evolved_skill_id),
                  evolution.evolved_skill_id);
    }
  }
  return std::ranges::all_of(field.rules->evolutions, [&kept](const EvolutionRule& evolution) {
    return std::ranges::binary_search(kept, evolution.evolved_skill_id);
  });
}

// Chaque skill possédé est de l'élément Fire, et il y en a au moins un.
[[nodiscard]] bool OnlyFireSkills(const SurvivorsField& field) {
  if (field.rules == nullptr || field.hero.skills.empty()) return false;
  return std::ranges::all_of(field.hero.skills, [&field](const OwnedSkill& owned) {
    const SurvivorsSkillRule* rule = FindSkill(*field.rules, owned.skill_id);
    return rule != nullptr && rule->element == SurvivorsElement::kFire;
  });
}

}  // namespace

std::vector<MinigameTrophy> RecordSurvivors(MinigameRecord& record, const SurvivorsRun& run,
                                            const SurvivorsTrophyFacts& facts) {
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
  for (const EvolvedTrophy& evolved : kEvolvedTrophies) {
    EarnMinigameTrophy(record, evolved.trophy, Owns(field, evolved.skill_id), fresh);
  }
  EarnMinigameTrophy(record, MinigameTrophy::kSurvivorsGrandWizard, RememberEvolved(record, field),
                     fresh);
  EarnMinigameTrophy(record, MinigameTrophy::kSurvivorsDeepFreeze,
                     field.recap.freezes >= kManyFreezes, fresh);
  EarnMinigameTrophy(record, MinigameTrophy::kSurvivorsBlindVictory,
                     victory && facts.without_minimap, fresh);
  return fresh;
}

}  // namespace ro::sim
