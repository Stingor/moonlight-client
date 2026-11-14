#pragma once

// LA SCÈNE DES ESSAIS DES RÈGLES SI/ALORS : le maître en (100, 100), son
// homoncule Lif en (101, 100), qui ne prend pas l'initiative et ne lance aucun
// skill automatique : seules les règles le font agir.

#include <cstdint>
#include <optional>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/companion_ai.h"
#include "ro/sim/companion_rules.h"
#include "ro/sim/monster_sheet.h"
#include "ro/sim/skill_book.h"
#include "ro/sim/world.h"

namespace companion_rule_scene {

inline constexpr std::uint32_t kOwnerId = 100;
inline constexpr std::uint32_t kHomunculusId = 200;
inline constexpr std::uint32_t kFirstMonsterId = 301;
inline constexpr std::uint32_t kSecondMonsterId = 302;
inline constexpr std::uint32_t kThirdMonsterId = 303;
inline constexpr std::uint32_t kFriendId = 400;
inline constexpr std::uint16_t kLifJob = 6001;
inline constexpr std::uint16_t kPoringJob = 1002;
inline constexpr std::uint16_t kOwnerX = 100;
inline constexpr std::uint16_t kRowY = 100;
inline constexpr std::uint16_t kSpeedMsPerCell = 150;
inline constexpr std::uint32_t kAmotionMs = 500;
inline constexpr std::uint32_t kStartMs = 100000;
inline constexpr std::uint32_t kTickMs = ro::sim::kCompanionAiTickMs;
inline constexpr std::int64_t kWholeHp = 100;

// LES SKILLS DE LA SCÈNE, un de chaque genre.
inline constexpr std::uint16_t kBoltId = 8013;
inline constexpr std::uint16_t kHealId = 8001;
inline constexpr std::uint16_t kSelfBuffId = 8002;
inline constexpr std::uint16_t kGroundId = 8020;
inline constexpr std::uint16_t kSkillLevel = 5;
inline constexpr int kSkillRangeCells = 9;
inline constexpr int kSkillSpCost = 10;

[[nodiscard]] inline ro::sim::Actor MakeActor(std::uint32_t id, ro::sim::ActorKind kind,
                                              std::uint16_t x, std::uint16_t job = 0) {
  ro::sim::Actor actor;
  actor.id = id;
  actor.kind = kind;
  actor.position = ro::sim::CellPosition{x, kRowY, 0};
  actor.speed_ms_per_cell = kSpeedMsPerCell;
  actor.look.job = job;
  return actor;
}

[[nodiscard]] inline ro::sim::CompanionSkillFact Skill(std::uint16_t skill_id,
                                                       std::uint16_t targeting) {
  return ro::sim::CompanionSkillFact{.skill_id = skill_id,
                                     .level = kSkillLevel,
                                     .targeting = targeting,
                                     .sp_cost = kSkillSpCost,
                                     .range_cells = kSkillRangeCells};
}

// UNE CONDITION SEULE, qui mène à « ne rien faire ».
[[nodiscard]] inline ro::sim::CompanionRule Guard(const ro::sim::RuleCondition& condition) {
  return ro::sim::CompanionRule{.action = ro::sim::RuleAction::kDoNothing,
                                .conditions = {condition}};
}

// UNE CONDITION VRAIE tant que le compagnon vit : ses PV au-dessus de zéro.
[[nodiscard]] inline ro::sim::RuleCondition Always() {
  return ro::sim::RuleCondition{.subject = ro::sim::RuleSubject::kCompanion,
                                .predicate = ro::sim::RulePredicate::kHpPercent,
                                .comparison = ro::sim::RuleComparison::kAbove,
                                .number = 0};
}

struct Scene {
  ro::sim::World world;
  ro::sim::CompanionBrain brain;
  ro::sim::CompanionFacts facts;
  ro::sim::CompanionAiSettings settings;
  ro::sim::MonsterSheetBook sheets;

  Scene() {
    world.Spawn(MakeActor(kOwnerId, ro::sim::ActorKind::kPlayer, kOwnerX));
    world.set_player_id(kOwnerId);
    ro::sim::Actor homunculus =
        MakeActor(kHomunculusId, ro::sim::ActorKind::kNonPlayer, kOwnerX + 1, kLifJob);
    homunculus.familiar = ro::sim::Familiar::kHomunculus;
    world.Spawn(homunculus);
    facts = ro::sim::CompanionFacts{
        .actor_id = kHomunculusId,
        .kind = ro::sim::AiCompanion::kHomunculus,
        .job = kLifJob,
        .active = true,
        .hp = kWholeHp,
        .max_hp = kWholeHp,
        .sp = kWholeHp,
        .max_sp = kWholeHp,
        .amotion_ms = kAmotionMs,
        .attack_range_cells = 1,
        .sp_known = true,
        .skills = {Skill(kBoltId, ro::sim::kSkillOnEnemy), Skill(kHealId, ro::sim::kSkillOnFriend),
                   Skill(kSelfBuffId, ro::sim::kSkillOnSelf),
                   Skill(kGroundId, ro::sim::kSkillOnGround)},
        .monster_sheets = &sheets};
    settings = ro::sim::BasicCompanionAiSettings(ro::sim::AiCompanion::kHomunculus, kLifJob);
    settings.skills.offensive = ro::sim::OffensiveSkills::kNever;
    settings.arrival_attack_hold_ms = 0;
  }

  void Monster(std::uint32_t id, std::uint16_t x, std::uint16_t job = kPoringJob) {
    world.Spawn(MakeActor(id, ro::sim::ActorKind::kMonster, x, job));
  }

  void Strikes(std::uint32_t attacker, std::uint32_t target, std::uint32_t now_ms) {
    static_cast<void>(world.RecordStrike(
        ro::sim::Strike{.attacker = attacker, .target = target, .started_ms = now_ms}));
  }

  [[nodiscard]] std::optional<ro::sim::CompanionOrder> Tick(std::uint32_t now_ms) {
    return brain.Tick(world, facts, settings, now_ms);
  }

  // LA CIBLE DU COMBAT : le monstre attaque le maître, que Lif défend.
  // Les règles se taisent le temps de l'engagement.
  void Engage(std::uint32_t monster_id, std::uint32_t now_ms) {
    settings.rules.clear();
    Strikes(monster_id, kOwnerId, now_ms);
    static_cast<void>(Tick(now_ms));
    REQUIRE(brain.target_id() == monster_id);
  }

  // LA CONDITION TIENT À CET INSTANT : sa règle « ne rien faire » décide.
  [[nodiscard]] bool Holds(const ro::sim::RuleCondition& condition, std::uint32_t now_ms) {
    settings.rules = {Guard(condition)};
    static_cast<void>(Tick(now_ms));
    const std::optional<ro::sim::DecidingRule>& decided = brain.deciding_rule();
    return decided.has_value() && decided->at_ms == now_ms;
  }
};

[[nodiscard]] inline bool IsKind(const std::optional<ro::sim::CompanionOrder>& order,
                                 ro::sim::CompanionOrder::Kind kind) {
  return order.has_value() && order->kind == kind;
}

}  // namespace companion_rule_scene
