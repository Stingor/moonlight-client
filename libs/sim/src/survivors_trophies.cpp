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
// Ceux des évolués du Chevalier.
constexpr std::array<EvolvedTrophy, survivors::kKnightEvolvedSkillCount> kKnightTrophies{{
    {survivors::kHeadCrushId, MinigameTrophy::kSurvivorsHeadCrush},
    {survivors::kSpiralPierceId, MinigameTrophy::kSurvivorsSpiralPierce},
    {survivors::kJointBeatId, MinigameTrophy::kSurvivorsJointBeat},
    {survivors::kAuraBladeId, MinigameTrophy::kSurvivorsAuraBlade},
    {survivors::kConcentrationId, MinigameTrophy::kSurvivorsConcentration},
    {survivors::kFrenzyId, MinigameTrophy::kSurvivorsFrenzy},
    {survivors::kIgnitionBreakId, MinigameTrophy::kSurvivorsIgnitionBreak},
}};
// Ceux des évolués du Voleur.
constexpr std::array<EvolvedTrophy, survivors::kThiefEvolvedSkillCount> kThiefTrophies{{
    {survivors::kSonicBlowId, MinigameTrophy::kSurvivorsSonicBlow},
    {survivors::kSoulDestroyerId, MinigameTrophy::kSurvivorsSoulDestroyer},
    {survivors::kVenomSplasherId, MinigameTrophy::kSurvivorsVenomSplasher},
    {survivors::kEnchantDeadlyPoisonId, MinigameTrophy::kSurvivorsEnchantDeadlyPoison},
    {survivors::kCloakingId, MinigameTrophy::kSurvivorsCloaking},
    {survivors::kThrowVenomKnifeId, MinigameTrophy::kSurvivorsThrowVenomKnife},
    {survivors::kMeteorAssaultId, MinigameTrophy::kSurvivorsMeteorAssault},
}};

// Ceux des évolués de l'Archer.
constexpr std::array<EvolvedTrophy, survivors::kArcherEvolvedSkillCount> kArcherTrophies{{
    {survivors::kBeastStrafingId, MinigameTrophy::kSurvivorsBeastStrafing},
    {survivors::kSharpShootingId, MinigameTrophy::kSurvivorsSharpShooting},
    {survivors::kFalconAssaultId, MinigameTrophy::kSurvivorsFalconAssault},
    {survivors::kTrueSightId, MinigameTrophy::kSurvivorsTrueSight},
    {survivors::kClaymoreTrapId, MinigameTrophy::kSurvivorsClaymoreTrap},
    {survivors::kBlastMineId, MinigameTrophy::kSurvivorsBlastMine},
    {survivors::kPhantasmicArrowId, MinigameTrophy::kSurvivorsPhantasmicArrow},
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

// Ajoute au record les évolués que la partie possède.
void RememberEvolved(MinigameRecord& record, const SurvivorsField& field) {
  if (field.rules == nullptr) return;
  std::vector<std::uint16_t>& kept = record.survivors_evolved;
  for (const EvolutionRule& evolution : field.rules->evolutions) {
    if (Owns(field, evolution.evolved_skill_id) &&
        !std::ranges::binary_search(kept, evolution.evolved_skill_id)) {
      kept.insert(std::ranges::upper_bound(kept, evolution.evolved_skill_id),
                  evolution.evolved_skill_id);
    }
  }
}

// Vrai si le record tient tous les évolués issus de la réserve `pool`, et que
// les règles en connaissent au moins un.
[[nodiscard]] bool HoldsAllEvolvedOf(const MinigameRecord& record, const SurvivorsField& field,
                                     std::span<const std::uint16_t> pool) {
  if (field.rules == nullptr) return false;
  const auto from_pool = [pool](const EvolutionRule& evolution) {
    return std::ranges::find(pool, evolution.skill_id) != pool.end();
  };
  const std::span<const EvolutionRule> evolutions = field.rules->evolutions;
  if (std::ranges::none_of(evolutions, from_pool)) return false;
  return std::ranges::all_of(evolutions, [&](const EvolutionRule& evolution) {
    return !from_pool(evolution) ||
           std::ranges::binary_search(record.survivors_evolved, evolution.evolved_skill_id);
  });
}

// La réserve de la première classe, le Magicien ; vide sans règles ni classe.
[[nodiscard]] std::span<const std::uint16_t> FirstClassPool(const SurvivorsField& field) {
  if (field.rules == nullptr || field.rules->classes.empty()) return {};
  return field.rules->classes.front().skill_pool;
}

// Vrai si le héros en scène est de la première classe, le Magicien.
[[nodiscard]] bool PlaysFirstClass(const SurvivorsField& field) {
  return field.rules != nullptr && !field.rules->classes.empty() && field.hero_class != nullptr &&
         field.hero_class->class_id == field.rules->classes.front().class_id;
}

// Vrai si le boss dont la chute finit cette arène est Baphomet.
[[nodiscard]] bool FinalBossIsBaphomet(const SurvivorsField& field) {
  return std::ranges::any_of(field.content.bosses, [](const BossRule& boss) {
    return boss.final && boss.foe_id == survivors::kBaphometId;
  });
}

// Chaque skill possédé est de l'élément Fire, et il y en a au moins un. Le coup
// de l'arme, que tout héros porte sans l'avoir choisi, ne compte pas.
[[nodiscard]] bool OnlyFireSkills(const SurvivorsField& field) {
  if (field.rules == nullptr) return false;
  bool any_fire = false;
  for (const OwnedSkill& owned : field.hero.skills) {
    const SurvivorsSkillRule* rule = FindSkill(*field.rules, owned.skill_id);
    if (rule == nullptr) return false;
    if (IsWeaponSkill(*rule)) continue;
    if (rule->element != SurvivorsElement::kFire) return false;
    any_fire = true;
  }
  return any_fire;
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
  EarnMinigameTrophy(record, MinigameTrophy::kSurvivorsVictory,
                     victory && FinalBossIsBaphomet(field), fresh);
  EarnMinigameTrophy(record, MinigameTrophy::kSurvivorsFirstEvolution, tally.evolutions > 0, fresh);
  EarnMinigameTrophy(record, MinigameTrophy::kSurvivorsKills1000, tally.kills >= kManyKills, fresh);
  EarnMinigameTrophy(record, MinigameTrophy::kSurvivorsLevel40,
                     PlaysFirstClass(field) && tally.level >= kSeasonedLevel, fresh);
  // Première carte : une carte tombée et RAMASSÉE au sol, celle qui entre
  // dans l'Album ; une carte prise dans une offre n'en est pas une.
  EarnMinigameTrophy(record, MinigameTrophy::kSurvivorsFirstCard, !field.recap.cards_found.empty(),
                     fresh);
  EarnMinigameTrophy(record, MinigameTrophy::kSurvivorsPyromaniac,
                     victory && PlaysFirstClass(field) && OnlyFireSkills(field), fresh);
  EarnMinigameTrophy(record, MinigameTrophy::kSurvivorsPureMind, victory && no_card, fresh);
  for (const EvolvedTrophy& evolved : kEvolvedTrophies) {
    EarnMinigameTrophy(record, evolved.trophy, Owns(field, evolved.skill_id), fresh);
  }
  RememberEvolved(record, field);
  EarnMinigameTrophy(record, MinigameTrophy::kSurvivorsGrandWizard,
                     HoldsAllEvolvedOf(record, field, FirstClassPool(field)), fresh);
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
  for (const EvolvedTrophy& evolved : kKnightTrophies) {
    EarnMinigameTrophy(record, evolved.trophy, Owns(field, evolved.skill_id), fresh);
  }
  EarnMinigameTrophy(record, MinigameTrophy::kSurvivorsGrandKnight,
                     HoldsAllEvolvedOf(record, field, survivors::KnightClass().skill_pool), fresh);
  for (const EvolvedTrophy& evolved : kThiefTrophies) {
    EarnMinigameTrophy(record, evolved.trophy, Owns(field, evolved.skill_id), fresh);
  }
  EarnMinigameTrophy(record, MinigameTrophy::kSurvivorsGrandThief,
                     HoldsAllEvolvedOf(record, field, survivors::ThiefClass().skill_pool), fresh);
  for (const EvolvedTrophy& evolved : kArcherTrophies) {
    EarnMinigameTrophy(record, evolved.trophy, Owns(field, evolved.skill_id), fresh);
  }
  EarnMinigameTrophy(record, MinigameTrophy::kSurvivorsGrandArcher,
                     HoldsAllEvolvedOf(record, field, survivors::ArcherClass().skill_pool), fresh);
  return fresh;
}

}  // namespace ro::sim
