#include "ro/sim/survivors_trophies.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
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
constexpr std::uint64_t kManyFreezes = 1000;

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
// Ceux des évolués du Warlock, au rang qui suit les skills de niveau maximum.
constexpr std::array<EvolvedTrophy, 2> kWarlockTrophies{{
    {survivors::kCrimsonRockId, MinigameTrophy::kSurvivorsCrimsonRock},
    {survivors::kJackFrostId, MinigameTrophy::kSurvivorsJackFrost},
}};

// Les trophées d'un skill de base porté à son plus haut niveau.
constexpr std::array<EvolvedTrophy, 3> kTopLevelTrophies{{
    {survivors::kFireBallId, MinigameTrophy::kSurvivorsFireBall},
    {survivors::kWaterBallId, MinigameTrophy::kSurvivorsWaterBall},
    {survivors::kIceWallId, MinigameTrophy::kSurvivorsIceWall},
}};

[[nodiscard]] bool Owns(const SurvivorsField& field, std::uint16_t skill_id) {
  return std::ranges::any_of(field.hero.skills, [skill_id](const OwnedSkill& owned) {
    return owned.skill_id == skill_id;
  });
}

// Vrai si la partie possède `skill_id` à son plus haut niveau.
[[nodiscard]] bool OwnsAtTop(const SurvivorsField& field, std::uint16_t skill_id) {
  if (field.rules == nullptr) return false;
  const SurvivorsSkillRule* rule = FindSkill(*field.rules, skill_id);
  return rule != nullptr && !rule->levels.empty() &&
         std::ranges::any_of(field.hero.skills, [&](const OwnedSkill& owned) {
           return owned.skill_id == skill_id && std::size_t{owned.level} == rule->levels.size();
         });
}

// Vrai si l'évolution part d'un skill de la réserve de la première classe, le
// Magicien : « Grand sorcier » ne demande que les siennes.
[[nodiscard]] bool OfFirstClass(const SurvivorsRules& rules, const EvolutionRule& evolution) {
  if (rules.classes.empty()) return false;
  const std::span<const std::uint16_t> pool = rules.classes.front().skill_pool;
  return std::ranges::find(pool, evolution.skill_id) != pool.end();
}

// Ajoute au record les évolués que la partie possède, et dit si le record
// tient tous ceux de la première classe, qui en a au moins un.
[[nodiscard]] bool RememberEvolved(MinigameRecord& record, const SurvivorsField& field) {
  if (field.rules == nullptr) return false;
  const SurvivorsRules& rules = *field.rules;
  std::vector<std::uint16_t>& kept = record.survivors_evolved;
  for (const EvolutionRule& evolution : rules.evolutions) {
    if (Owns(field, evolution.evolved_skill_id) &&
        !std::ranges::binary_search(kept, evolution.evolved_skill_id)) {
      kept.insert(std::ranges::upper_bound(kept, evolution.evolved_skill_id),
                  evolution.evolved_skill_id);
    }
  }
  const auto of_first_class = [&rules](const EvolutionRule& evolution) {
    return OfFirstClass(rules, evolution);
  };
  if (std::ranges::none_of(rules.evolutions, of_first_class)) return false;
  return std::ranges::all_of(rules.evolutions, [&](const EvolutionRule& evolution) {
    return !of_first_class(evolution) ||
           std::ranges::binary_search(kept, evolution.evolved_skill_id);
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
  for (const EvolvedTrophy& top : kTopLevelTrophies) {
    EarnMinigameTrophy(record, top.trophy, OwnsAtTop(field, top.skill_id), fresh);
  }
  for (const EvolvedTrophy& evolved : kWarlockTrophies) {
    EarnMinigameTrophy(record, evolved.trophy, Owns(field, evolved.skill_id), fresh);
  }
  return fresh;
}

}  // namespace ro::sim
