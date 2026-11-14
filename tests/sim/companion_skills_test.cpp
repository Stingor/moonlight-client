// LES SKILLS OFFENSIFS AUTOMATIQUES D'UN COMPAGNON : quand ils partent,
// lesquels, et ce qu'un refus du serveur suspend.

#include "ro/sim/companion_skills.h"

#include <cstdint>
#include <optional>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/companion_ai.h"
#include "ro/sim/skill_book.h"
#include "ro/sim/world.h"

using ro::sim::Actor;
using ro::sim::ActorKind;
using ro::sim::AiCompanion;
using ro::sim::CellMovement;
using ro::sim::CellPosition;
using ro::sim::CompanionAiSettings;
using ro::sim::CompanionBrain;
using ro::sim::CompanionFacts;
using ro::sim::CompanionOrder;
using ro::sim::CompanionSkillFact;
using ro::sim::DirectOrder;
using ro::sim::Familiar;
using ro::sim::OffensiveSkills;
using ro::sim::SkillPermission;
using ro::sim::Strike;
using ro::sim::World;

namespace {

constexpr std::uint32_t kOwnerId = 100;
constexpr std::uint32_t kHomunculusId = 200;
constexpr std::uint32_t kMonsterId = 300;
constexpr std::uint32_t kStrangerId = 400;
constexpr std::uint16_t kFilirJob = 6003;
constexpr std::uint16_t kOwnerX = 100;
constexpr std::uint16_t kRowY = 100;
constexpr std::uint16_t kSpeedMsPerCell = 150;
constexpr std::uint32_t kStartMs = 100000;
// Le délai de pose d'un déplacement, dépassé.
constexpr std::uint32_t kSettledMs = 1100;

// Un skill offensif de portée 9, appris au niveau 5, à 20 SP.
constexpr std::uint16_t kBoltId = 8013;
constexpr std::uint16_t kBoltLevel = 5;
constexpr int kBoltSp = 20;
constexpr int kBoltRangeCells = 9;
constexpr std::uint16_t kGroundId = 8020;
constexpr std::uint16_t kSupportId = 8001;
constexpr std::uint16_t kSelfId = 8002;

[[nodiscard]] Actor MakeActor(std::uint32_t id, ActorKind kind, std::uint16_t x) {
  Actor actor;
  actor.id = id;
  actor.kind = kind;
  actor.position = CellPosition{x, kRowY, 0};
  actor.speed_ms_per_cell = kSpeedMsPerCell;
  return actor;
}

[[nodiscard]] CompanionSkillFact Bolt() {
  return CompanionSkillFact{.skill_id = kBoltId,
                            .level = kBoltLevel,
                            .targeting = ro::sim::kSkillOnEnemy,
                            .sp_cost = kBoltSp,
                            .range_cells = kBoltRangeCells};
}

// Un Filir en (101, 100) près de son maître, un monstre sur la même rangée.
struct Scene {
  World world;
  CompanionBrain brain;
  CompanionFacts facts;
  CompanionAiSettings settings;

  explicit Scene(std::uint16_t monster_x, OffensiveSkills mode) {
    world.Spawn(MakeActor(kOwnerId, ActorKind::kPlayer, kOwnerX));
    world.set_player_id(kOwnerId);
    Actor homunculus = MakeActor(kHomunculusId, ActorKind::kNonPlayer, kOwnerX + 1);
    homunculus.familiar = Familiar::kHomunculus;
    homunculus.look.job = kFilirJob;
    world.Spawn(homunculus);
    world.Spawn(MakeActor(kMonsterId, ActorKind::kMonster, monster_x));
    facts = CompanionFacts{.actor_id = kHomunculusId,
                           .kind = AiCompanion::kHomunculus,
                           .job = kFilirJob,
                           .active = true,
                           .hp = 100,
                           .max_hp = 100,
                           .sp = 100,
                           .max_sp = 100,
                           .amotion_ms = 500,
                           .attack_range_cells = 1,
                           .sp_known = true,
                           .skills = {Bolt()}};
    settings = ro::sim::BasicCompanionAiSettings(AiCompanion::kHomunculus, kFilirJob);
    settings.skills.offensive = mode;
  }

  [[nodiscard]] std::optional<CompanionOrder> Tick(std::uint32_t now_ms) {
    return brain.Tick(world, facts, settings, now_ms);
  }

  // Le coup qui porte le skill : la trace qui dit qu'il est parti.
  void Landed(std::uint32_t now_ms) {
    static_cast<void>(world.RecordStrike(Strike{.attacker = kHomunculusId,
                                                .target = kMonsterId,
                                                .started_ms = now_ms,
                                                .skill_id = kBoltId,
                                                .skill_level = kBoltLevel}));
  }
};

[[nodiscard]] bool IsKind(const std::optional<CompanionOrder>& order, CompanionOrder::Kind kind) {
  return order.has_value() && order->kind == kind;
}

[[nodiscard]] bool IsSkill(const std::optional<CompanionOrder>& order) {
  return IsKind(order, CompanionOrder::Kind::kSkill);
}

}  // namespace

TEST_CASE("Skills de compagnon : d'usine, en combat", "[sim][companion_skills]") {
  CHECK(ro::sim::BasicCompanionAiSettings(AiCompanion::kHomunculus, kFilirJob).skills.offensive ==
        OffensiveSkills::kInCombat);
  CHECK(ro::sim::BasicCompanionAiSettings(AiCompanion::kMercenary, 6017).skills.offensive ==
        OffensiveSkills::kInCombat);
}

TEST_CASE("Skills de compagnon : « jamais » ne lance rien", "[sim][companion_skills]") {
  Scene scene(kOwnerX + 2, OffensiveSkills::kNever);
  for (std::uint32_t later_ms = 0; later_ms < 3000; later_ms += 560) {
    CHECK_FALSE(IsSkill(scene.Tick(kStartMs + later_ms)));
  }
}

TEST_CASE("Skills de compagnon : en combat, à portée d'attaque et pas en poursuite",
          "[sim][companion_skills]") {
  Scene scene(kOwnerX + 6, OffensiveSkills::kInCombat);
  // À portée du skill, mais pas d'attaque simple : il poursuit.
  REQUIRE(IsKind(scene.Tick(kStartMs), CompanionOrder::Kind::kMove));
  scene.world.Spawn(MakeActor(kMonsterId, ActorKind::kMonster, kOwnerX + 2));
  const auto order = scene.Tick(kStartMs + kSettledMs);
  REQUIRE(IsSkill(order));
  CHECK(order->target_id == kMonsterId);
  CHECK(order->skill_id == kBoltId);
  CHECK(order->skill_level == kBoltLevel);
}

TEST_CASE("Skills de compagnon : en poursuite et en combat, dès la portée du skill",
          "[sim][companion_skills]") {
  Scene scene(kOwnerX + 6, OffensiveSkills::kChaseAndCombat);
  CHECK(IsSkill(scene.Tick(kStartMs)));
  // Hors de la portée du skill : il poursuit.
  Scene far(kOwnerX + 12, OffensiveSkills::kChaseAndCombat);
  CHECK(IsKind(far.Tick(kStartMs), CompanionOrder::Kind::kMove));
}

TEST_CASE("Skills de compagnon : en skills seuls, jamais d'attaque simple",
          "[sim][companion_skills]") {
  Scene scene(kOwnerX + 2, OffensiveSkills::kSkillsOnly);
  REQUIRE(IsSkill(scene.Tick(kStartMs)));
  scene.Landed(kStartMs + 50);
  // Plus assez de SP : il attend à portée, sans frapper.
  scene.facts.sp = kBoltSp - 1;
  for (std::uint32_t later_ms = 140; later_ms < 3000; later_ms += 140) {
    CHECK_FALSE(scene.Tick(kStartMs + later_ms).has_value());
  }

  // Loin : il s'approche jusqu'à la portée du skill, pas au contact.
  Scene far(kOwnerX + 16, OffensiveSkills::kSkillsOnly);
  const auto approach = far.Tick(kStartMs);
  REQUIRE(IsKind(approach, CompanionOrder::Kind::kMove));
  CHECK(approach->cell == CellPosition{kOwnerX + 16 - kBoltRangeCells, kRowY, 0});
}

TEST_CASE("Skills de compagnon : la SP à garder, et une SP inconnue", "[sim][companion_skills]") {
  Scene scene(kOwnerX + 2, OffensiveSkills::kChaseAndCombat);
  SECTION("SP à garder") { scene.settings.skills.sp_reserve_points = scene.facts.sp - kBoltSp + 1; }
  SECTION("SP inconnue") { scene.facts.sp_known = false; }
  CHECK(IsKind(scene.Tick(kStartMs), CompanionOrder::Kind::kAttack));
}

TEST_CASE("Skills de compagnon : le délai entre deux skills", "[sim][companion_skills]") {
  Scene scene(kOwnerX + 2, OffensiveSkills::kChaseAndCombat);
  REQUIRE(IsSkill(scene.Tick(kStartMs)));
  scene.Landed(kStartMs + 50);
  // Avant le délai, l'attaque simple ; un seul ordre par tick.
  CHECK(IsKind(scene.Tick(kStartMs + 140), CompanionOrder::Kind::kAttack));
  CHECK(IsSkill(scene.Tick(kStartMs + ro::sim::kFactorySkillDelayMs + 20)));
}

TEST_CASE("Skills de compagnon : un skill décoché, un niveau borné à l'appris",
          "[sim][companion_skills]") {
  Scene scene(kOwnerX + 2, OffensiveSkills::kChaseAndCombat);
  SECTION("décoché") {
    scene.settings.skills.permissions = {SkillPermission{.skill_id = kBoltId, .allowed = false}};
    CHECK(IsKind(scene.Tick(kStartMs), CompanionOrder::Kind::kAttack));
  }
  SECTION("niveau réglé") {
    scene.settings.skills.permissions = {SkillPermission{.skill_id = kBoltId, .level = 3}};
    const auto order = scene.Tick(kStartMs);
    REQUIRE(IsSkill(order));
    CHECK(order->skill_level == 3);
  }
  SECTION("niveau réglé au-dessus de l'appris") {
    scene.settings.skills.permissions = {SkillPermission{.skill_id = kBoltId, .level = 9}};
    const auto order = scene.Tick(kStartMs);
    REQUIRE(IsSkill(order));
    CHECK(order->skill_level == kBoltLevel);
  }
}

TEST_CASE("Skills de compagnon : ni au sol, ni de soutien, ni sur soi", "[sim][companion_skills]") {
  Scene scene(kOwnerX + 2, OffensiveSkills::kChaseAndCombat);
  scene.facts.skills = {
      CompanionSkillFact{.skill_id = kGroundId,
                         .level = 1,
                         .targeting = ro::sim::kSkillOnGround,
                         .range_cells = kBoltRangeCells},
      CompanionSkillFact{.skill_id = kSupportId,
                         .level = 1,
                         .targeting = ro::sim::kSkillOnFriend,
                         .range_cells = kBoltRangeCells},
      CompanionSkillFact{.skill_id = kSelfId, .level = 1, .targeting = ro::sim::kSkillOnSelf},
  };
  for (std::uint32_t later_ms = 0; later_ms < 3000; later_ms += 560) {
    CHECK_FALSE(IsSkill(scene.Tick(kStartMs + later_ms)));
  }
}

TEST_CASE("Skills de compagnon : pas en marche, ni pendant une incantation",
          "[sim][companion_skills]") {
  Scene scene(kOwnerX + 6, OffensiveSkills::kChaseAndCombat);
  SECTION("en marche") {
    REQUIRE(scene.world.StartMoving(kHomunculusId,
                                    CellMovement{CellPosition{kOwnerX + 1, kRowY, 0},
                                                 CellPosition{kOwnerX + 3, kRowY, 0}, 8, 8},
                                    kStartMs));
    CHECK_FALSE(IsSkill(scene.Tick(kStartMs + 100)));
  }
  SECTION("en incantation") {
    Actor casting = *scene.world.Find(kHomunculusId);
    casting.cast = ro::sim::Cast{.skill_id = kBoltId,
                                 .target_id = kMonsterId,
                                 .started_ms = kStartMs - 100,
                                 .duration_ms = 2000};
    scene.world.Spawn(casting);
    CHECK_FALSE(IsSkill(scene.Tick(kStartMs)));
  }
}

TEST_CASE("Skills de compagnon : un refus suspend le skill, puis il reprend",
          "[sim][companion_skills]") {
  Scene scene(kOwnerX + 2, OffensiveSkills::kChaseAndCombat);
  REQUIRE(IsSkill(scene.Tick(kStartMs)));
  // Après le délai entre deux skills : sans refus, le skill repartirait ici.
  constexpr std::uint32_t kAfterDelayMs = 560;
  SECTION("le serveur le dit") {
    scene.brain.NoteSkillRefused(kBoltId, kStartMs + 50);
    CHECK_FALSE(IsSkill(scene.Tick(kStartMs + kAfterDelayMs)));
  }
  SECTION("l'homoncule pense") {
    REQUIRE(scene.world.RecordEmotion(
        kHomunculusId,
        ro::sim::Emotion{.pack = ro::sim::kBaseEmotionPack, .id = ro::sim::kEmotionThink},
        kStartMs + 50));
    CHECK_FALSE(IsSkill(scene.Tick(kStartMs + kAfterDelayMs)));
  }
  SECTION("aucune trace") {
    // Le skill ne laisse rien : il ne se réémet pas en attendant sa trace, et
    // au bout du délai de trace il est suspendu.
    CHECK_FALSE(IsSkill(scene.Tick(kStartMs + kAfterDelayMs)));
    static_cast<void>(scene.Tick(kStartMs + ro::sim::kSkillTraceMs));
  }
  for (std::uint32_t later_ms = ro::sim::kSkillTraceMs + 140; later_ms < 4000; later_ms += 140) {
    CHECK_FALSE(IsSkill(scene.Tick(kStartMs + later_ms)));
  }
  CHECK(IsSkill(scene.Tick(kStartMs + ro::sim::kSkillTraceMs + ro::sim::kSkillRefusalSuspendMs +
                           kSettledMs)));
}

TEST_CASE("Skills de compagnon : pas de skill sur le monstre d'un autre joueur",
          "[sim][companion_skills]") {
  Scene scene(kOwnerX + 2, OffensiveSkills::kChaseAndCombat);
  scene.world.Spawn(MakeActor(kStrangerId, ActorKind::kPlayer, kOwnerX + 4));
  static_cast<void>(scene.world.RecordStrike(
      Strike{.attacker = kMonsterId, .target = kStrangerId, .started_ms = kStartMs - 100}));
  scene.brain.Command(DirectOrder{.kind = DirectOrder::Kind::kAttack, .target_id = kMonsterId});
  CHECK(IsKind(scene.Tick(kStartMs), CompanionOrder::Kind::kAttack));
}

TEST_CASE("Skills de compagnon : un skill qui laisse sa trace n'est pas suspendu",
          "[sim][companion_skills]") {
  // Sans la lecture des traces, chaque skill réussi serait tenu pour refusé au
  // bout du délai de trace, et ne repartirait plus.
  constexpr std::uint32_t kStepMs = 140;
  constexpr std::uint32_t kPastTraceMs = ro::sim::kSkillTraceMs * 2;
  Scene scene(kOwnerX + 2, OffensiveSkills::kChaseAndCombat);
  bool by_cast = false;
  SECTION("trace par un coup") {}
  SECTION("trace par une incantation") { by_cast = true; }
  int late_casts = 0;
  for (std::uint32_t later_ms = 0; later_ms < kPastTraceMs; later_ms += kStepMs) {
    const std::uint32_t now_ms = kStartMs + later_ms;
    if (!IsSkill(scene.Tick(now_ms))) {
      continue;
    }
    if (later_ms > ro::sim::kSkillTraceMs) {
      ++late_casts;
    }
    if (by_cast) {
      // Une incantation sans durée : le serveur l'annonce quand même.
      Actor casting = *scene.world.Find(kHomunculusId);
      casting.cast =
          ro::sim::Cast{.skill_id = kBoltId, .target_id = kMonsterId, .started_ms = now_ms + 1};
      scene.world.Spawn(casting);
    } else {
      scene.Landed(now_ms + 1);
    }
  }
  CHECK(late_casts > 0);
}

TEST_CASE("Skills de compagnon : en poursuite, il s'arrête à portée du skill et lance",
          "[sim][companion_skills]") {
  Scene scene(kOwnerX + 20, OffensiveSkills::kChaseAndCombat);
  const auto approach = scene.Tick(kStartMs);
  REQUIRE(IsKind(approach, CompanionOrder::Kind::kMove));
  // Il vise la portée du skill, pas le contact.
  CHECK(approach->cell == CellPosition{kOwnerX + 20 - kBoltRangeCells, kRowY, 0});
  // Dix cases à 150 ms : il arrive au bout de 1500 ms.
  REQUIRE(scene.world.StartMoving(
      kHomunculusId, CellMovement{CellPosition{kOwnerX + 1, kRowY, 0}, approach->cell, 8, 8},
      kStartMs));
  CHECK_FALSE(IsSkill(scene.Tick(kStartMs + 140)));
  CHECK_FALSE(IsSkill(scene.Tick(kStartMs + 1400)));
  CHECK(IsSkill(scene.Tick(kStartMs + 1600)));
}

TEST_CASE("Skills de compagnon : pas de skill avant que le déplacement soit posé",
          "[sim][companion_skills]") {
  // Un déplacement vient de partir et le serveur ne l'a pas encore annoncé : le
  // compagnon paraît immobile, mais rien ne part avant le délai de pose.
  Scene scene(kOwnerX + 13, OffensiveSkills::kChaseAndCombat);
  REQUIRE(IsKind(scene.Tick(kStartMs), CompanionOrder::Kind::kMove));
  scene.world.Spawn(MakeActor(kMonsterId, ActorKind::kMonster, kOwnerX + 6));
  CHECK_FALSE(IsSkill(scene.Tick(kStartMs + 140)));
  CHECK(IsSkill(scene.Tick(kStartMs + kSettledMs)));
}

TEST_CASE("Skills de compagnon : une émoticône antérieure à l'envoi ne suspend rien",
          "[sim][companion_skills]") {
  Scene scene(kOwnerX + 2, OffensiveSkills::kChaseAndCombat);
  REQUIRE(scene.world.RecordEmotion(
      kHomunculusId,
      ro::sim::Emotion{.pack = ro::sim::kBaseEmotionPack, .id = ro::sim::kEmotionThink},
      kStartMs - 500));
  REQUIRE(IsSkill(scene.Tick(kStartMs)));
  scene.Landed(kStartMs + 50);
  CHECK(IsSkill(scene.Tick(kStartMs + 560)));
}

TEST_CASE("Skills de compagnon : un skill d'ordre direct compte dans le délai",
          "[sim][companion_skills]") {
  Scene scene(kOwnerX + 2, OffensiveSkills::kChaseAndCombat);
  scene.brain.Command(DirectOrder{.kind = DirectOrder::Kind::kSkillOnActor,
                                  .target_id = kMonsterId,
                                  .skill_id = kBoltId,
                                  .skill_level = 1,
                                  .skill_range_cells = kBoltRangeCells});
  REQUIRE(IsSkill(scene.Tick(kStartMs)));
  // Au tick suivant, le Filir prend le monstre de lui-même : l'attaque, pas un
  // second skill avant le délai.
  CHECK(IsKind(scene.Tick(kStartMs + 140), CompanionOrder::Kind::kAttack));
}

TEST_CASE("Skills de compagnon : ni les skills ruineux ni les debuffs ne partent seuls",
          "[sim][companion_skills]") {
  constexpr std::uint16_t kSbr44 = 8012;
  constexpr std::uint16_t kProvoke = 8232;
  constexpr std::uint16_t kSense = 8237;
  CHECK(ro::sim::NeverCastAutomatically(kSbr44));
  CHECK(ro::sim::IsCompanionDebuff(kProvoke));
  CHECK_FALSE(ro::sim::NeverCastAutomatically(kBoltId));
  CHECK_FALSE(ro::sim::IsCompanionDebuff(kBoltId));

  Scene scene(kOwnerX + 2, OffensiveSkills::kChaseAndCombat);
  scene.facts.skills = {};
  for (const std::uint16_t skill_id : {kSbr44, kProvoke, kSense}) {
    scene.facts.skills.push_back(CompanionSkillFact{.skill_id = skill_id,
                                                    .level = 1,
                                                    .targeting = ro::sim::kSkillOnEnemy,
                                                    .sp_cost = 1,
                                                    .range_cells = kBoltRangeCells});
  }
  for (std::uint32_t later_ms = 0; later_ms < 3000; later_ms += 560) {
    CHECK_FALSE(IsSkill(scene.Tick(kStartMs + later_ms)));
  }
}

TEST_CASE("Skills de compagnon : la recharge annoncée par le serveur est respectée",
          "[sim][companion_skills]") {
  Scene scene(kOwnerX + 2, OffensiveSkills::kChaseAndCombat);
  scene.facts.skills.front().cooldown_remaining_ms = 3000;
  CHECK(IsKind(scene.Tick(kStartMs), CompanionOrder::Kind::kAttack));
  scene.facts.skills.front().cooldown_remaining_ms = 0;
  CHECK(IsSkill(scene.Tick(kStartMs + 560)));
}

TEST_CASE("Skills de compagnon : le délai d'arrivée les retient, directs compris",
          "[sim][companion_skills]") {
  Scene scene(kOwnerX + 2, OffensiveSkills::kChaseAndCombat);
  scene.brain.ForgetMap(false, kStartMs);
  SECTION("automatique") {}
  SECTION("ordre direct") {
    scene.brain.Command(DirectOrder{.kind = DirectOrder::Kind::kSkillOnActor,
                                    .target_id = kMonsterId,
                                    .skill_id = kBoltId,
                                    .skill_level = 1,
                                    .skill_range_cells = kBoltRangeCells});
  }
  for (std::uint32_t later_ms = 0; later_ms < ro::sim::kServerCompanionAttackHoldMs;
       later_ms += 560) {
    CHECK_FALSE(IsSkill(scene.Tick(kStartMs + later_ms)));
  }
  CHECK(IsSkill(scene.Tick(kStartMs + ro::sim::kServerCompanionAttackHoldMs + 40)));
}

TEST_CASE("Skills de compagnon : en skills seuls, une cible qui ne prend aucun coup est lâchée",
          "[sim][companion_skills]") {
  constexpr std::uint32_t kStepMs = 560;
  constexpr std::uint32_t kPastBoundMs = 11000;
  Scene scene(kOwnerX + 2, OffensiveSkills::kSkillsOnly);
  REQUIRE(IsSkill(scene.Tick(kStartMs)));
  for (std::uint32_t later_ms = kStepMs; later_ms < kPastBoundMs; later_ms += kStepMs) {
    static_cast<void>(scene.Tick(kStartMs + later_ms));
  }
  CHECK(scene.brain.target_id() == 0);
}
