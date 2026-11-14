#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "ro/sim/survivors_content.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_rules.h"

#include "support/survivors_fixture.h"

namespace ro::test_support {

// LES RÈGLES DE CONTENU : les règles minuscules, plus une pièce de chaque
// mécanique d'une arène complète — un skill de chaque archétype neuf, un
// rebond, une espèce par comportement, un boss scripté et final, un événement
// de chaque sorte, des élites, une évolution, un coffre et une plante par
// objet. Toutes acceptées par CheckSurvivorsRules ; un essai en retouche UNE
// valeur sur une copie fraîche (ContentTables) et attend un refus.

inline constexpr std::uint16_t kContentBoltId = 20;     // Lightning Bolt : rebond
inline constexpr std::uint16_t kContentBlastId = 11;    // Napalm Beat
inline constexpr std::uint16_t kContentLinearId = 15;   // Frost Diver
inline constexpr std::uint16_t kContentWallId = 18;     // Fire Wall
inline constexpr std::uint16_t kContentNovaId = 88;     // Frost Nova
inline constexpr std::uint16_t kContentRingId = 80;     // Fire Pillar
inline constexpr std::uint16_t kContentEvolvedId = 83;  // Meteor Storm

inline constexpr std::uint16_t kContentPupaId = 1008;
inline constexpr std::uint16_t kContentFamiliarId = 1005;
inline constexpr std::uint16_t kContentThiefBugId = 1051;
inline constexpr std::uint16_t kContentArcherId = 1016;
inline constexpr std::uint16_t kContentSavageId = 1166;
inline constexpr std::uint16_t kContentBathoryId = 1102;
inline constexpr std::uint16_t kContentSmokieId = 1056;
inline constexpr std::uint16_t kContentPoporingId = 1031;
inline constexpr std::uint16_t kContentWolfId = 1013;
inline constexpr std::uint16_t kContentBaphometId = 1039;
inline constexpr std::uint16_t kContentBaphometJrId = 1101;

inline constexpr std::uint16_t kContentChestItemId = 603;  // Old Blue Box

inline constexpr sim::SkillLevel kContentBoltLevel{.damage = 12,
                                                   .cooldown_ms = 1000,
                                                   .amount = 1,
                                                   .reach_units = sim::Cells(9),
                                                   .bounces = 2,
                                                   .bounce_pct = 60,
                                                   .bounce_reach_units = sim::Cells(3)};
inline constexpr sim::SkillLevel kContentBlastLevel{.damage = 24,
                                                    .cooldown_ms = 1400,
                                                    .amount = 1,
                                                    .interval_ms = 150,
                                                    .reach_units = sim::Cells(8),
                                                    .area_radius_units = sim::CellTenths(15),
                                                    .splash_pct = 60};
inline constexpr sim::SkillLevel kContentLinearLevel{.damage = 18,
                                                     .cooldown_ms = 1600,
                                                     .amount = 1,
                                                     .reach_units = sim::Cells(9),
                                                     .speed_units_per_s = sim::Cells(12),
                                                     .hit_radius_units = sim::CellTenths(4),
                                                     .pierce = 1,
                                                     .freeze_ms = 1500};
inline constexpr sim::SkillLevel kContentWallLevel{.damage = 12,
                                                   .cooldown_ms = 4500,
                                                   .amount = 1,
                                                   .reach_units = sim::Cells(2),
                                                   .life_ms = 4000,
                                                   .rehit_ms = 400,
                                                   .length_units = sim::Cells(3),
                                                   .zone_hits = 5,
                                                   .knockback_units = sim::Cells(2)};
inline constexpr sim::SkillLevel kContentNovaLevel{.damage = 40,
                                                   .cooldown_ms = 3000,
                                                   .amount = 1,
                                                   .area_radius_units = sim::Cells(4),
                                                   .freeze_ms = 2500};
inline constexpr sim::SkillLevel kContentRingLevel{.damage = 40,
                                                   .cooldown_ms = 6000,
                                                   .amount = 8,
                                                   .reach_units = sim::Cells(3),
                                                   .life_ms = 6000,
                                                   .rehit_ms = 1000,
                                                   .area_radius_units = sim::Cells(1)};
inline constexpr sim::SkillLevel kContentEvolvedLevel{.damage = 60,
                                                      .cooldown_ms = 2500,
                                                      .amount = 5,
                                                      .interval_ms = 200,
                                                      .reach_units = sim::Cells(10),
                                                      .area_radius_units = sim::CellTenths(25),
                                                      .splash_pct = 100};

// L'ordre des skills de contenu, après les trois skills minuscules.
inline constexpr std::size_t kTinySkillCount = 3;
inline constexpr std::size_t kContentBoltIndex = kTinySkillCount;
inline constexpr std::size_t kContentBlastIndex = kTinySkillCount + 1;
inline constexpr std::size_t kContentLinearIndex = kTinySkillCount + 2;
inline constexpr std::size_t kContentWallIndex = kTinySkillCount + 3;
inline constexpr std::size_t kContentNovaIndex = kTinySkillCount + 4;
inline constexpr std::size_t kContentRingIndex = kTinySkillCount + 5;
inline constexpr std::size_t kContentEvolvedIndex = kTinySkillCount + 6;
inline constexpr std::size_t kContentSkillCount = kTinySkillCount + 7;

[[nodiscard]] constexpr sim::SurvivorsSkillRule ContentSkill(std::uint16_t skill_id,
                                                             sim::SkillArchetype archetype,
                                                             sim::TargetRule target,
                                                             sim::SurvivorsElement element) {
  return {.skill_id = skill_id,
          .key = "content",
          .description = "Skill de contenu.",
          .archetype = archetype,
          .target = target,
          .element = element,
          .levels = {}};
}

// Une espèce commune de contenu : celle des règles minuscules, avec son
// comportement.
[[nodiscard]] constexpr sim::FoeRule ContentFoe(std::uint16_t foe_id,
                                                const sim::FoeBehaviour& behaviour) {
  return {.foe_id = foe_id,
          .ro_level = 10,
          .rank = sim::FoeRank::kCommon,
          .size = sim::FoeSize::kMedium,
          .element = sim::SurvivorsElement::kNeutral,
          .hp = 20,
          .ro_walk_ms_per_cell = 200,
          .touch_damage = 5,
          .touch_interval_ms = 1500,
          .exp = 2,
          .behaviour = behaviour};
}

inline constexpr std::size_t kContentFoeCount = 13;
inline constexpr std::array<sim::FoeRule, kContentFoeCount> kContentFoes{{
    kTinyFoes[0],
    kTinyFoes[1],
    ContentFoe(kContentPupaId, {.move = sim::FoeMove::kStill}),
    ContentFoe(kContentFamiliarId, {.move = sim::FoeMove::kFlyer, .traits = sim::kFoeOverHorde}),
    ContentFoe(kContentThiefBugId, {.move = sim::FoeMove::kThief}),
    ContentFoe(kContentArcherId, {.move = sim::FoeMove::kRanged,
                                  .period_ms = 2864,
                                  .telegraph_ms = 500,
                                  .reach_units = sim::Cells(6),
                                  .shot_damage = 14,
                                  .shot_units_per_s = sim::Cells(10),
                                  .shot_radius_units = sim::CellTenths(3)}),
    ContentFoe(kContentSavageId, {.move = sim::FoeMove::kCharger,
                                  .traits = sim::kFoeKnockbackImmune,
                                  .period_ms = 5000,
                                  .telegraph_ms = 500,
                                  .reach_units = sim::Cells(6),
                                  .dash_units_per_s = sim::Cells(16)}),
    ContentFoe(kContentBathoryId, {.move = sim::FoeMove::kBlinker,
                                   .period_ms = 4000,
                                   .telegraph_ms = 300,
                                   .reach_units = sim::Cells(3)}),
    ContentFoe(kContentSmokieId, {.move = sim::FoeMove::kChase,
                                  .traits = sim::kFoeHides,
                                  .period_ms = 6000,
                                  .active_ms = 1000}),
    ContentFoe(kContentPoporingId, {.puddle_damage = 5,
                                    .puddle_radius_units = sim::CellTenths(15),
                                    .puddle_ms = 4000,
                                    .puddle_period_ms = 1000}),
    ContentFoe(kContentWolfId, {.pack = 6}),
    {.foe_id = kContentBaphometId,
     .ro_level = 81,
     .rank = sim::FoeRank::kBoss,
     .size = sim::FoeSize::kLarge,
     .element = sim::SurvivorsElement::kDark,
     .hp = 50000,
     .ro_walk_ms_per_cell = 100,
     .touch_damage = 35,
     .touch_interval_ms = 768,
     .exp = 0,
     .behaviour = {.traits = static_cast<std::uint8_t>(sim::kFoeKnockbackImmune |
                                                       sim::kFoeFreezeImmune)}},
    ContentFoe(kContentBaphometJrId, {}),
}};

// Baphomet : une charge en phase 1, des invocations toujours, des séismes
// sous 60 %.
inline constexpr std::array<sim::BossMoveRule, 3> kContentBossMoves{{
    {.kind = sim::BossMoveKind::kCharge,
     .from_hp_pct = 100,
     .to_hp_pct = 60,
     .period_ms = 8000,
     .telegraph_ms = 800,
     .reach_units = sim::Cells(6)},
    {.kind = sim::BossMoveKind::kSummon,
     .from_hp_pct = 100,
     .to_hp_pct = 0,
     .period_ms = 12000,
     .foe_id = kContentBaphometJrId,
     .count = 4},
    {.kind = sim::BossMoveKind::kQuake,
     .from_hp_pct = 60,
     .to_hp_pct = 0,
     .period_ms = 6000,
     .telegraph_ms = 1000,
     .count = 3,
     .radius_units = sim::CellTenths(25),
     .damage = 40},
}};

inline constexpr std::array<sim::EventRule, 4> kContentEvents{{
    {.at_ms = 10000, .kind = sim::EventKind::kSwarm, .foe_id = kContentFamiliarId, .count = 8},
    {.at_ms = 15000, .kind = sim::EventKind::kRaid, .foe_id = kTinyLunaticId, .count = 6},
    {.at_ms = 20000,
     .kind = sim::EventKind::kRing,
     .foe_id = kContentPupaId,
     .count = 12,
     .radius_units = sim::Cells(6)},
    {.at_ms = 25000, .kind = sim::EventKind::kGroup, .foe_id = kContentThiefBugId, .count = 3},
}};

inline constexpr std::array<sim::EvolutionRule, 1> kContentEvolutions{{
    {.skill_id = kTinyStrikeId, .card_item_id = kTinyCardId, .evolved_skill_id = kContentEvolvedId},
}};

inline constexpr std::array<sim::PickupRule, 7> kContentPickups{{
    {.kind = sim::PickupKind::kChest, .item_id = kContentChestItemId},
    {.kind = sim::PickupKind::kMeat, .item_id = 517, .amount = 30},
    {.kind = sim::PickupKind::kGold, .item_id = 969, .amount = 10},
    {.kind = sim::PickupKind::kManual, .item_id = 12208, .amount = 100, .duration_ms = 30000},
    {.kind = sim::PickupKind::kHaste, .item_id = 12016, .amount = 50, .duration_ms = 10000},
    {.kind = sim::PickupKind::kHolyWater, .item_id = 523, .reach_units = sim::Cells(12)},
    {.kind = sim::PickupKind::kMagnetAll, .item_id = 12210},
}};

inline constexpr std::array<sim::PlantRule, 6> kContentPlants{{
    {.foe_id = 1078, .pickup = sim::PickupKind::kMeat, .weight = 30},
    {.foe_id = 1081, .pickup = sim::PickupKind::kGold, .weight = 25},
    {.foe_id = 1080, .pickup = sim::PickupKind::kHaste, .weight = 15},
    {.foe_id = 1079, .pickup = sim::PickupKind::kManual, .weight = 15},
    {.foe_id = 1083, .pickup = sim::PickupKind::kMagnetAll, .weight = 10},
    {.foe_id = 1082, .pickup = sim::PickupKind::kHolyWater, .weight = 5},
}};

// Les tables d'une variante des règles de contenu. Les spans pointent dans
// l'objet : il ne se copie pas.
struct ContentTables {
  sim::SkillLevel bolt = kContentBoltLevel;
  sim::SkillLevel blast = kContentBlastLevel;
  sim::SkillLevel linear = kContentLinearLevel;
  sim::SkillLevel wall = kContentWallLevel;
  sim::SkillLevel nova = kContentNovaLevel;
  sim::SkillLevel ring = kContentRingLevel;
  sim::SkillLevel evolved = kContentEvolvedLevel;
  std::array<sim::SurvivorsSkillRule, kContentSkillCount> skills{{
      kTinySkills[0],
      kTinySkills[1],
      kTinySkills[2],
      ContentSkill(kContentBoltId, sim::SkillArchetype::kTargetStrike, sim::TargetRule::kStrongest,
                   sim::SurvivorsElement::kWind),
      ContentSkill(kContentBlastId, sim::SkillArchetype::kClusterBlast, sim::TargetRule::kDensest,
                   sim::SurvivorsElement::kGhost),
      ContentSkill(kContentLinearId, sim::SkillArchetype::kLinear, sim::TargetRule::kNearest,
                   sim::SurvivorsElement::kWater),
      ContentSkill(kContentWallId, sim::SkillArchetype::kWall, sim::TargetRule::kNearest,
                   sim::SurvivorsElement::kFire),
      ContentSkill(kContentNovaId, sim::SkillArchetype::kNova, sim::TargetRule::kNearest,
                   sim::SurvivorsElement::kWater),
      ContentSkill(kContentRingId, sim::SkillArchetype::kRing, sim::TargetRule::kNearest,
                   sim::SurvivorsElement::kFire),
      ContentSkill(kContentEvolvedId, sim::SkillArchetype::kClusterBlast, sim::TargetRule::kDensest,
                   sim::SurvivorsElement::kFire),
  }};
  std::array<sim::ClassRule, 1> classes = kTinyClasses;
  std::array<sim::FoeRule, kContentFoeCount> foes = kContentFoes;
  std::array<sim::BossMoveRule, 3> boss_moves = kContentBossMoves;
  std::array<sim::BossRule, 1> bosses{{
      {.at_ms = 30000,
       .foe_id = kContentBaphometId,
       .final = true,
       .escort_foe_id = kContentBaphometJrId,
       .escort_count = 2},
  }};
  std::array<sim::EventRule, 4> events = kContentEvents;
  std::array<sim::EvolutionRule, 1> evolutions = kContentEvolutions;
  std::array<sim::PickupRule, 7> pickups = kContentPickups;
  std::array<sim::PlantRule, 6> plants = kContentPlants;
  sim::SurvivorsRules rules = TinyRules();

  ContentTables() {
    skills[kContentBoltIndex].levels = {&bolt, 1};
    skills[kContentBlastIndex].levels = {&blast, 1};
    skills[kContentLinearIndex].levels = {&linear, 1};
    skills[kContentWallIndex].levels = {&wall, 1};
    skills[kContentNovaIndex].levels = {&nova, 1};
    skills[kContentRingIndex].levels = {&ring, 1};
    skills[kContentEvolvedIndex].levels = {&evolved, 1};
    bosses[0].moves = boss_moves;
    rules.skills = skills;
    rules.classes = classes;
    rules.foes = foes;
    rules.bosses = bosses;
    rules.events = events;
    rules.evolutions = evolutions;
    rules.pickups = pickups;
    rules.plants = plants;
    rules.elites = {.first_ms = 20000,
                    .interval_ms = 10000,
                    .last_ms = 50000,
                    .hp_pct = 1000,
                    .touch_pct = 150,
                    .exp_pct = 500};
    rules.chests = {.item_id = kContentChestItemId,
                    .evolution_from_ms = 30000,
                    .triple_per_10000 = 1200,
                    .quintuple_per_10000 = 200};
    rules.tuning.boss_hp_growth_pct_per_minute = 15;
    rules.tuning.boss_walk_scale_pct = 160;
    rules.tuning.knock_units_per_step = sim::CellTenths(2);
    rules.tuning.plant_block_cells = 12;
    rules.tuning.plant_cells_per_plant = 150;
    rules.tuning.pickup_radius_units = sim::CellTenths(5);
    rules.tuning.final_overtime_ms = 60000;
  }
  ContentTables(const ContentTables&) = delete;
  ContentTables& operator=(const ContentTables&) = delete;
};

// Les règles de contenu : une table immuable, qui survit à toute partie.
[[nodiscard]] inline const sim::SurvivorsRules& ContentRules() {
  static const ContentTables kTables;
  return kTables.rules;
}

}  // namespace ro::test_support
