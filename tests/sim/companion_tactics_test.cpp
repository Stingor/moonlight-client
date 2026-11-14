// LES TACTIQUES PAR MONSTRE D'UN COMPAGNON : rangs, déclencheurs, lignes
// génériques, skills, partage, poursuite et évitement.

#include "ro/sim/companion_tactics.h"

#include <cstddef>
#include <cstdint>
#include <optional>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/companion_ai.h"
#include "ro/sim/skill_book.h"
#include "ro/sim/world.h"

using ro::sim::Actor;
using ro::sim::ActorKind;
using ro::sim::AiCompanion;
using ro::sim::CellMovement;
using ro::sim::CellPosition;
using ro::sim::CompanionAiReason;
using ro::sim::CompanionAiSettings;
using ro::sim::CompanionBrain;
using ro::sim::CompanionFacts;
using ro::sim::CompanionOrder;
using ro::sim::CompanionSkillFact;
using ro::sim::CompanionTactic;
using ro::sim::DirectOrder;
using ro::sim::Familiar;
using ro::sim::OffensiveSkills;
using ro::sim::Strike;
using ro::sim::TacticKey;
using ro::sim::TacticKeyKind;
using ro::sim::TacticRank;
using ro::sim::TacticTrigger;
using ro::sim::World;

namespace {

constexpr std::uint32_t kOwnerId = 100;
constexpr std::uint32_t kHomunculusId = 200;
constexpr std::uint32_t kFirstMonsterId = 301;
constexpr std::uint32_t kSecondMonsterId = 302;
constexpr std::uint32_t kStrangerId = 400;
constexpr std::uint16_t kLifJob = 6001;
constexpr std::uint16_t kFilirJob = 6003;
constexpr std::uint16_t kPoringJob = 1002;
constexpr std::uint16_t kFabreJob = 1007;
constexpr std::uint16_t kChestJob = 1324;
constexpr std::uint16_t kOwnerX = 100;
constexpr std::uint16_t kRowY = 100;
constexpr std::uint16_t kSpeedMsPerCell = 150;
constexpr std::uint32_t kAmotionMs = 500;
constexpr std::uint32_t kStartMs = 100000;
// Le délai de pose d'un déplacement, dépassé.
constexpr std::uint32_t kSettledMs = 1100;

constexpr std::uint16_t kBoltId = 8013;
constexpr std::uint16_t kBoltLevel = 5;
constexpr std::uint16_t kOpenerId = 8019;
constexpr int kBoltRangeCells = 9;

[[nodiscard]] Actor MakeActor(std::uint32_t id, ActorKind kind, std::uint16_t x,
                              std::uint16_t job = 0) {
  Actor actor;
  actor.id = id;
  actor.kind = kind;
  actor.position = CellPosition{x, kRowY, 0};
  actor.speed_ms_per_cell = kSpeedMsPerCell;
  actor.look.job = job;
  return actor;
}

[[nodiscard]] CompanionTactic Line(std::uint16_t job, TacticRank rank,
                                   TacticTrigger trigger = TacticTrigger::kAlways) {
  return CompanionTactic{.key = TacticKey{.kind = TacticKeyKind::kMonster, .job = job},
                         .rank = rank,
                         .trigger = trigger};
}

[[nodiscard]] CompanionSkillFact Skill(std::uint16_t skill_id) {
  return CompanionSkillFact{.skill_id = skill_id,
                            .level = kBoltLevel,
                            .targeting = ro::sim::kSkillOnEnemy,
                            .sp_cost = 10,
                            .range_cells = kBoltRangeCells};
}

// Le maître en (100, 100), son homoncule en (101, 100).
struct Scene {
  World world;
  CompanionBrain brain;
  CompanionFacts facts;
  CompanionAiSettings settings;

  explicit Scene(std::uint16_t job) {
    world.Spawn(MakeActor(kOwnerId, ActorKind::kPlayer, kOwnerX));
    world.set_player_id(kOwnerId);
    Actor homunculus = MakeActor(kHomunculusId, ActorKind::kNonPlayer, kOwnerX + 1, job);
    homunculus.familiar = Familiar::kHomunculus;
    world.Spawn(homunculus);
    facts = CompanionFacts{.actor_id = kHomunculusId,
                           .kind = AiCompanion::kHomunculus,
                           .job = job,
                           .active = true,
                           .hp = 100,
                           .max_hp = 100,
                           .sp = 100,
                           .max_sp = 100,
                           .amotion_ms = kAmotionMs,
                           .attack_range_cells = 1,
                           .sp_known = true};
    settings = ro::sim::BasicCompanionAiSettings(AiCompanion::kHomunculus, job);
    settings.skills.offensive = OffensiveSkills::kNever;
  }

  void Monster(std::uint32_t id, std::uint16_t x, std::uint16_t job) {
    world.Spawn(MakeActor(id, ActorKind::kMonster, x, job));
  }

  void Strikes(std::uint32_t attacker, std::uint32_t target, std::uint32_t now_ms,
               std::int32_t damage = 0, std::uint16_t skill_id = 0) {
    static_cast<void>(world.RecordStrike(Strike{.attacker = attacker,
                                                .target = target,
                                                .started_ms = now_ms,
                                                .damage = damage,
                                                .skill_id = skill_id}));
  }

  void Set(const CompanionTactic& tactic) { REQUIRE(settings.tactics.Set(tactic)); }

  [[nodiscard]] std::optional<CompanionOrder> Tick(std::uint32_t now_ms) {
    return brain.Tick(world, facts, settings, now_ms);
  }
};

[[nodiscard]] bool IsKind(const std::optional<CompanionOrder>& order, CompanionOrder::Kind kind) {
  return order.has_value() && order->kind == kind;
}

}  // namespace

TEST_CASE("Tactiques : le rang d'abord, puis la défense, la distance et l'id",
          "[sim][companion_tactics]") {
  Scene scene(kLifJob);
  SECTION("un rang haut passe devant un rang bas qui attaque le maître") {
    scene.Set(Line(kPoringJob, TacticRank::kHigh));
    scene.Set(Line(kFabreJob, TacticRank::kLow));
    scene.Monster(kFirstMonsterId, kOwnerX + 6, kPoringJob);
    scene.Monster(kSecondMonsterId, kOwnerX + 3, kFabreJob);
    scene.Strikes(kSecondMonsterId, kOwnerId, kStartMs - 100);
    static_cast<void>(scene.Tick(kStartMs));
    CHECK(scene.brain.target_id() == kFirstMonsterId);
  }
  SECTION("à rang égal, la défense du maître passe devant le plus proche") {
    scene.Set(Line(kPoringJob, TacticRank::kMedium));
    scene.Monster(kFirstMonsterId, kOwnerX + 8, kPoringJob);
    scene.Monster(kSecondMonsterId, kOwnerX + 2, kPoringJob);
    scene.Strikes(kFirstMonsterId, kOwnerId, kStartMs - 100);
    static_cast<void>(scene.Tick(kStartMs));
    CHECK(scene.brain.target_id() == kFirstMonsterId);
  }
  SECTION("à défense égale, le plus proche, puis le plus petit id") {
    scene.Set(Line(kPoringJob, TacticRank::kMedium));
    scene.Monster(kSecondMonsterId, kOwnerX + 4, kPoringJob);
    scene.Monster(kFirstMonsterId, kOwnerX - 2, kPoringJob);
    static_cast<void>(scene.Tick(kStartMs));
    CHECK(scene.brain.target_id() == kFirstMonsterId);
  }
}

TEST_CASE("Tactiques : « Ignorer » ignore même ce qui attaque, sauf ordre direct",
          "[sim][companion_tactics]") {
  Scene scene(kLifJob);
  scene.Set(Line(kPoringJob, TacticRank::kIgnore));
  scene.Monster(kFirstMonsterId, kOwnerX + 2, kPoringJob);
  scene.Strikes(kFirstMonsterId, kHomunculusId, kStartMs - 100);
  CHECK_FALSE(scene.Tick(kStartMs).has_value());
  scene.brain.Command(
      DirectOrder{.kind = DirectOrder::Kind::kAttack, .target_id = kFirstMonsterId});
  CHECK(IsKind(scene.Tick(kStartMs + 140), CompanionOrder::Kind::kAttack));
}

TEST_CASE("Tactiques : « toujours » attaque même sans initiative", "[sim][companion_tactics]") {
  Scene scene(kLifJob);
  REQUIRE_FALSE(scene.settings.attack_on_own);
  scene.Monster(kFirstMonsterId, kOwnerX + 2, kPoringJob);
  CHECK_FALSE(scene.Tick(kStartMs).has_value());
  scene.Set(Line(kPoringJob, TacticRank::kMedium));
  CHECK(IsKind(scene.Tick(kStartMs + 140), CompanionOrder::Kind::kAttack));
}

TEST_CASE("Tactiques : « s'il attaque » et « s'il attaque le compagnon »",
          "[sim][companion_tactics]") {
  Scene scene(kFilirJob);
  scene.Monster(kFirstMonsterId, kOwnerX + 2, kPoringJob);
  SECTION("s'il attaque : le maître compte, même sans défendre le maître au niveau 1") {
    scene.settings.defend_owner = false;
    scene.Set(Line(kPoringJob, TacticRank::kMedium, TacticTrigger::kIfAttacks));
    CHECK_FALSE(scene.Tick(kStartMs).has_value());
    scene.Strikes(kFirstMonsterId, kOwnerId, kStartMs + 100);
    CHECK(IsKind(scene.Tick(kStartMs + 140), CompanionOrder::Kind::kAttack));
  }
  SECTION("s'il attaque le compagnon : le maître ne compte pas pour la ligne") {
    scene.settings.defend_owner = false;
    scene.Set(Line(kPoringJob, TacticRank::kMedium, TacticTrigger::kIfAttacksCompanion));
    scene.Strikes(kFirstMonsterId, kOwnerId, kStartMs - 100);
    CHECK_FALSE(scene.Tick(kStartMs).has_value());
    scene.Strikes(kFirstMonsterId, kHomunculusId, kStartMs + 100);
    CHECK(IsKind(scene.Tick(kStartMs + 140), CompanionOrder::Kind::kAttack));
  }
}

TEST_CASE("Tactiques : « En dernier » seulement sans autre candidat", "[sim][companion_tactics]") {
  Scene scene(kLifJob);
  scene.Set(Line(kPoringJob, TacticRank::kLast));
  scene.Set(Line(kFabreJob, TacticRank::kLow));
  scene.Monster(kFirstMonsterId, kOwnerX + 2, kPoringJob);
  scene.Monster(kSecondMonsterId, kOwnerX + 9, kFabreJob);
  static_cast<void>(scene.Tick(kStartMs));
  CHECK(scene.brain.target_id() == kSecondMonsterId);
}

TEST_CASE("Tactiques : « Tenir » frappe une fois, lâche, puis reprend",
          "[sim][companion_tactics]") {
  Scene scene(kLifJob);
  scene.Set(Line(kPoringJob, TacticRank::kHold));
  scene.Monster(kFirstMonsterId, kOwnerX + 2, kPoringJob);
  REQUIRE(IsKind(scene.Tick(kStartMs), CompanionOrder::Kind::kAttack));
  // La frappe touche : la cible est lâchée.
  scene.Strikes(kHomunculusId, kFirstMonsterId, kStartMs + 50, /*damage=*/12);
  CHECK_FALSE(scene.Tick(kStartMs + 140).has_value());
  CHECK(scene.brain.target_id() == 0);
  CHECK_FALSE(scene.Tick(kStartMs + 1000).has_value());

  SECTION("reprise après le délai") {
    CHECK(IsKind(scene.Tick(kStartMs + 1700), CompanionOrder::Kind::kAttack));
  }
  SECTION("pas de reprise tant qu'il vise le compagnon") {
    scene.Strikes(kFirstMonsterId, kHomunculusId, kStartMs + 1600);
    CHECK_FALSE(scene.Tick(kStartMs + 1700).has_value());
  }
}

TEST_CASE("Tactiques : en route, un rang supérieur prend la place", "[sim][companion_tactics]") {
  Scene scene(kLifJob);
  scene.settings.retarget_while_chasing = true;
  scene.Set(Line(kPoringJob, TacticRank::kLow));
  scene.Set(Line(kFabreJob, TacticRank::kAbsolute));
  scene.Monster(kFirstMonsterId, kOwnerX + 8, kPoringJob);
  static_cast<void>(scene.Tick(kStartMs));
  REQUIRE(scene.brain.target_id() == kFirstMonsterId);
  scene.Monster(kSecondMonsterId, kOwnerX + 10, kFabreJob);
  static_cast<void>(scene.Tick(kStartMs + 140));
  CHECK(scene.brain.target_id() == kSecondMonsterId);
}

TEST_CASE("Tactiques : l'usage des skills sur un monstre", "[sim][companion_tactics]") {
  Scene scene(kLifJob);
  scene.settings.skills.offensive = OffensiveSkills::kChaseAndCombat;
  scene.facts.skills = {Skill(kBoltId)};
  CompanionTactic line = Line(kPoringJob, TacticRank::kMedium);
  scene.Monster(kFirstMonsterId, kOwnerX + 2, kPoringJob);
  // Chaque skill parti laisse sa trace, pour que le suivant puisse partir.
  const auto tick = [&scene](std::uint32_t later_ms) {
    const std::optional<CompanionOrder> order = scene.Tick(kStartMs + later_ms);
    if (IsKind(order, CompanionOrder::Kind::kSkill)) {
      scene.Strikes(kHomunculusId, kFirstMonsterId, kStartMs + later_ms + 1, 1, kBoltId);
    }
    return order;
  };
  SECTION("jamais") {
    line.skill_use = ro::sim::TacticSkillUse::kNever;
    scene.Set(line);
    CHECK(IsKind(tick(0), CompanionOrder::Kind::kAttack));
  }
  SECTION("deux fois par cible") {
    line.skill_use = ro::sim::TacticSkillUse::kTimes;
    line.skill_use_count = 2;
    scene.Set(line);
    CHECK(IsKind(tick(0), CompanionOrder::Kind::kSkill));
    CHECK(IsKind(tick(560), CompanionOrder::Kind::kSkill));
    CHECK(IsKind(tick(1120), CompanionOrder::Kind::kAttack));
  }
  SECTION("une fois au niveau 2") {
    line.skill_use = ro::sim::TacticSkillUse::kOnceAtLevel;
    line.skill_use_count = 2;
    scene.Set(line);
    const auto first = tick(0);
    REQUIRE(IsKind(first, CompanionOrder::Kind::kSkill));
    CHECK(first->skill_level == 2);
    CHECK(IsKind(tick(560), CompanionOrder::Kind::kAttack));
  }
}

TEST_CASE("Tactiques : le skill d'ouverture part une fois", "[sim][companion_tactics]") {
  Scene scene(kLifJob);
  scene.settings.skills.offensive = OffensiveSkills::kChaseAndCombat;
  scene.facts.skills = {Skill(kBoltId), Skill(kOpenerId)};
  CompanionTactic line = Line(kPoringJob, TacticRank::kMedium);
  line.opening_skill_id = kOpenerId;
  scene.Set(line);
  scene.Monster(kFirstMonsterId, kOwnerX + 2, kPoringJob);
  const auto first = scene.Tick(kStartMs);
  REQUIRE(IsKind(first, CompanionOrder::Kind::kSkill));
  CHECK(first->skill_id == kOpenerId);
  scene.Strikes(kHomunculusId, kFirstMonsterId, kStartMs + 1, 1, kOpenerId);
  const auto second = scene.Tick(kStartMs + 560);
  REQUIRE(IsKind(second, CompanionOrder::Kind::kSkill));
  CHECK(second->skill_id == kBoltId);
}

TEST_CASE("Tactiques : harceler un monstre pendant un autre combat", "[sim][companion_tactics]") {
  Scene scene(kLifJob);
  scene.settings.skills.offensive = OffensiveSkills::kChaseAndCombat;
  scene.facts.skills = {Skill(kBoltId)};
  // La cible courante n'accepte pas de skill ; l'autre est harcelable.
  CompanionTactic target = Line(kPoringJob, TacticRank::kHigh);
  target.skill_use = ro::sim::TacticSkillUse::kNever;
  CompanionTactic harassed = Line(kFabreJob, TacticRank::kIgnore);
  harassed.harass = true;
  scene.Set(target);
  scene.Set(harassed);
  scene.Monster(kFirstMonsterId, kOwnerX + 2, kPoringJob);
  scene.Monster(kSecondMonsterId, kOwnerX + 6, kFabreJob);
  const auto order = scene.Tick(kStartMs);
  REQUIRE(IsKind(order, CompanionOrder::Kind::kSkill));
  CHECK(order->target_id == kSecondMonsterId);
  CHECK(scene.brain.last_reason() == CompanionAiReason::kHarass);
  CHECK(scene.brain.target_id() == kFirstMonsterId);
  // Une fois par monstre.
  scene.Strikes(kHomunculusId, kSecondMonsterId, kStartMs + 1, 1, kBoltId);
  CHECK(IsKind(scene.Tick(kStartMs + 560), CompanionOrder::Kind::kAttack));
}

TEST_CASE("Tactiques : la SP à garder de la ligne remplace celle du niveau 1",
          "[sim][companion_tactics]") {
  Scene scene(kLifJob);
  scene.settings.skills.offensive = OffensiveSkills::kChaseAndCombat;
  scene.facts.skills = {Skill(kBoltId)};
  CompanionTactic line = Line(kPoringJob, TacticRank::kMedium);
  line.sp_reserve_points = scene.facts.sp;
  scene.Set(line);
  scene.Monster(kFirstMonsterId, kOwnerX + 2, kPoringJob);
  CHECK(IsKind(scene.Tick(kStartMs), CompanionOrder::Kind::kAttack));
}

TEST_CASE("Tactiques : le partage avec les autres joueurs", "[sim][companion_tactics]") {
  Scene scene(kLifJob);
  scene.world.Spawn(MakeActor(kStrangerId, ActorKind::kPlayer, kOwnerX + 4));
  scene.Monster(kFirstMonsterId, kOwnerX + 2, kPoringJob);
  scene.Strikes(kStrangerId, kFirstMonsterId, kStartMs - 100);
  CompanionTactic line = Line(kPoringJob, TacticRank::kMedium);
  SECTION("jamais") {
    line.share = ro::sim::TacticShare::kNever;
    scene.Set(line);
    CHECK_FALSE(scene.Tick(kStartMs).has_value());
  }
  SECTION("toujours") {
    line.share = ro::sim::TacticShare::kAlways;
    scene.Set(line);
    CHECK(IsKind(scene.Tick(kStartMs), CompanionOrder::Kind::kAttack));
  }
  SECTION("s'il ne bouge pas") {
    line.share = ro::sim::TacticShare::kIfStill;
    scene.Set(line);
    CHECK(IsKind(scene.Tick(kStartMs), CompanionOrder::Kind::kAttack));
  }
  SECTION("s'il ne bouge pas, et il bouge") {
    line.share = ro::sim::TacticShare::kIfStill;
    scene.Set(line);
    REQUIRE(scene.world.StartMoving(kFirstMonsterId,
                                    CellMovement{CellPosition{kOwnerX + 2, kRowY, 0},
                                                 CellPosition{kOwnerX + 2, kRowY + 5, 0}, 8, 8},
                                    kStartMs - 50));
    CHECK_FALSE(scene.Tick(kStartMs).has_value());
  }
}

TEST_CASE("Tactiques : la poursuite de la ligne remplace celle du niveau 1",
          "[sim][companion_tactics]") {
  Scene scene(kLifJob);
  CompanionTactic line = Line(kPoringJob, TacticRank::kMedium);
  scene.Monster(kFirstMonsterId, kOwnerX + 8, kPoringJob);
  SECTION("toujours, quand le niveau 1 ne poursuit pas") {
    scene.settings.chase = false;
    line.chase = ro::sim::TacticChase::kAlways;
    scene.Set(line);
    CHECK(IsKind(scene.Tick(kStartMs), CompanionOrder::Kind::kMove));
  }
  SECTION("jamais, quand le niveau 1 poursuit") {
    line.chase = ro::sim::TacticChase::kNever;
    scene.Set(line);
    CHECK_FALSE(scene.Tick(kStartMs).has_value());
    CHECK(scene.brain.target_id() == 0);
  }
}

TEST_CASE("Tactiques : éviter un monstre, borné par l'éloignement maximum",
          "[sim][companion_tactics]") {
  Scene scene(kLifJob);
  CompanionTactic line = Line(kPoringJob, TacticRank::kIgnore);
  scene.Monster(kFirstMonsterId, kOwnerX + 3, kPoringJob);
  SECTION("toujours") {
    line.avoid = ro::sim::TacticAvoid::kAlways;
    scene.Set(line);
    const auto order = scene.Tick(kStartMs);
    REQUIRE(IsKind(order, CompanionOrder::Kind::kMove));
    // La case la plus proche à portée de suivi du maître, hors du seuil.
    CHECK(order->cell == CellPosition{kOwnerX - 1, kRowY, 0});
    CHECK(scene.brain.last_reason() == CompanionAiReason::kAvoid);
  }
  SECTION("s'il attaque : pas avant qu'il ait attaqué") {
    line.avoid = ro::sim::TacticAvoid::kIfAttacks;
    scene.Set(line);
    CHECK_FALSE(scene.Tick(kStartMs).has_value());
    scene.Strikes(kFirstMonsterId, kOwnerId, kStartMs + 100);
    CHECK(IsKind(scene.Tick(kStartMs + 140), CompanionOrder::Kind::kMove));
  }
  SECTION("sans case sûre près du maître, le pas se raccourcit dans l'éloignement maximum") {
    line.avoid = ro::sim::TacticAvoid::kAlways;
    scene.settings.leash_still_cells = 3;
    scene.settings.follow_distance_cells = 0;
    scene.Set(line);
    const auto order = scene.Tick(kStartMs);
    REQUIRE(IsKind(order, CompanionOrder::Kind::kMove));
    CHECK(order->cell == CellPosition{kOwnerX - 3, kRowY, 0});
  }
}

TEST_CASE("Tactiques : les lignes génériques", "[sim][companion_tactics]") {
  Scene scene(kFilirJob);
  SECTION("les coffres") {
    CHECK(ro::sim::IsTreasureChest(kChestJob));
    CHECK(ro::sim::IsTreasureChest(1946));
    CHECK_FALSE(ro::sim::IsTreasureChest(1364));
    scene.Set(CompanionTactic{.key = TacticKey{.kind = TacticKeyKind::kChest},
                              .rank = TacticRank::kIgnore});
    scene.Monster(kFirstMonsterId, kOwnerX + 2, kChestJob);
    CHECK_FALSE(scene.Tick(kStartMs).has_value());
  }
  SECTION("les immobiles : vus un moment sans bouger, oubliés à la carte") {
    Scene lif(kLifJob);
    lif.Set(CompanionTactic{.key = TacticKey{.kind = TacticKeyKind::kStill},
                            .rank = TacticRank::kHigh});
    lif.Monster(kFirstMonsterId, kOwnerX + 2, kPoringJob);
    // À peine vu : pas encore une plante.
    CHECK_FALSE(lif.Tick(kStartMs).has_value());
    const std::uint32_t still_ms = kStartMs + ro::sim::kStillObservationMs + 100;
    CHECK(IsKind(lif.Tick(still_ms), CompanionOrder::Kind::kAttack));
    // Un monstre qui a incanté n'en est pas une, même immobile.
    Actor caster = MakeActor(kSecondMonsterId, ActorKind::kMonster, kOwnerX - 2, kPoringJob);
    caster.cast = ro::sim::Cast{.skill_id = 1, .started_ms = kStartMs, .duration_ms = 1};
    Scene other(kLifJob);
    other.Set(CompanionTactic{.key = TacticKey{.kind = TacticKeyKind::kStill},
                              .rank = TacticRank::kHigh});
    other.world.Spawn(caster);
    CHECK_FALSE(other.Tick(kStartMs).has_value());
    CHECK_FALSE(other.Tick(still_ms).has_value());
    // Une nouvelle carte : tout est oublié, l'observation recommence.
    lif.brain.ForgetMap(false, kStartMs - ro::sim::kServerCompanionAttackHoldMs);
    CHECK_FALSE(lif.Tick(still_ms + 140).has_value());
  }
  SECTION("tout autre monstre") {
    scene.Set(CompanionTactic{.key = TacticKey{.kind = TacticKeyKind::kAnyMonster},
                              .rank = TacticRank::kIgnore});
    scene.Monster(kFirstMonsterId, kOwnerX + 2, kPoringJob);
    CHECK_FALSE(scene.Tick(kStartMs).has_value());
  }
}

TEST_CASE("Tactiques : au plus soixante-quatre lignes", "[sim][companion_tactics]") {
  ro::sim::CompanionTactics tactics;
  for (std::size_t rank = 0; rank < ro::sim::kCompanionTacticMost; ++rank) {
    REQUIRE(tactics.Set(Line(static_cast<std::uint16_t>(kPoringJob + rank), TacticRank::kLow)));
  }
  CHECK_FALSE(tactics.Set(Line(static_cast<std::uint16_t>(kPoringJob + 500), TacticRank::kLow)));
  // Remplacer une ligne existante passe toujours.
  CHECK(tactics.Set(Line(kPoringJob, TacticRank::kHigh)));
  CHECK(tactics.Find(TacticKey{.kind = TacticKeyKind::kMonster, .job = kPoringJob})->rank ==
        TacticRank::kHigh);
  CHECK(tactics.Remove(TacticKey{.kind = TacticKeyKind::kMonster, .job = kPoringJob}));
  CHECK(tactics.lines().size() == ro::sim::kCompanionTacticMost - 1);
  // Une ligne hors du cadre est refusée.
  CompanionTactic wrong = Line(kFabreJob, TacticRank::kLow);
  wrong.avoid_step_cells = 0;
  CHECK_FALSE(ro::sim::ValidTactic(wrong));
}

TEST_CASE("Tactiques : une cible à éviter se combat à distance, sans osciller",
          "[sim][companion_tactics]") {
  Scene scene(kLifJob);
  CompanionTactic line = Line(kPoringJob, TacticRank::kMedium);
  line.avoid = ro::sim::TacticAvoid::kAlways;
  scene.Set(line);
  scene.Monster(kFirstMonsterId, kOwnerX + 4, kPoringJob);
  bool with_skill = false;
  SECTION("au corps à corps seulement : il ne s'approche pas") {}
  SECTION("avec un skill qui porte : il le lance de loin") { with_skill = true; }
  if (with_skill) {
    scene.settings.skills.offensive = OffensiveSkills::kInCombat;
    scene.facts.skills = {Skill(kBoltId)};
  }
  int moves = 0;
  int attacks = 0;
  int casts = 0;
  for (std::uint32_t later_ms = 0; later_ms < 5000; later_ms += 140) {
    const std::optional<CompanionOrder> order = scene.Tick(kStartMs + later_ms);
    if (IsKind(order, CompanionOrder::Kind::kMove)) {
      ++moves;
      Actor moved = *scene.world.Find(kHomunculusId);
      moved.position = order->cell;
      scene.world.Spawn(moved);
    } else if (IsKind(order, CompanionOrder::Kind::kAttack)) {
      ++attacks;
    } else if (IsKind(order, CompanionOrder::Kind::kSkill)) {
      ++casts;
      scene.Strikes(kHomunculusId, kFirstMonsterId, kStartMs + later_ms + 1, 1, kBoltId);
    }
  }
  CHECK(moves <= 1);
  CHECK(attacks == 0);
  CHECK((casts > 0) == with_skill);
}

TEST_CASE("Tactiques : éviter et suivre ne se renvoient pas le compagnon",
          "[sim][companion_tactics]") {
  // Valeurs d'usine : suivi 3, seuil 3, pas 5. Le maître se tient à deux cases
  // d'un monstre évité ; le serveur ramène le compagnon contre le maître.
  Scene scene(kLifJob);
  CompanionTactic line = Line(kPoringJob, TacticRank::kIgnore);
  line.avoid = ro::sim::TacticAvoid::kAlways;
  scene.Set(line);
  scene.Monster(kFirstMonsterId, kOwnerX + 2, kPoringJob);
  int walks = 0;
  for (std::uint32_t later_ms = 0; later_ms < 8000; later_ms += 140) {
    const std::optional<CompanionOrder> order = scene.Tick(kStartMs + later_ms);
    Actor moved = *scene.world.Find(kHomunculusId);
    if (IsKind(order, CompanionOrder::Kind::kMove)) {
      ++walks;
      moved.position = order->cell;
    } else if (IsKind(order, CompanionOrder::Kind::kReturnToOwner)) {
      ++walks;
      moved.position = CellPosition{kOwnerX + 1, kRowY, 0};
    }
    scene.world.Spawn(moved);
  }
  CHECK(walks <= 2);
}

TEST_CASE("Tactiques : l'usage des skills et l'ouverture se comptent par cible",
          "[sim][companion_tactics]") {
  Scene scene(kLifJob);
  scene.settings.skills.offensive = OffensiveSkills::kChaseAndCombat;
  scene.facts.skills = {Skill(kBoltId), Skill(kOpenerId)};
  CompanionTactic line = Line(kPoringJob, TacticRank::kMedium);
  line.skill_use = ro::sim::TacticSkillUse::kTimes;
  line.skill_use_count = 1;
  line.opening_skill_id = kOpenerId;
  scene.Set(line);
  scene.Monster(kFirstMonsterId, kOwnerX + 2, kPoringJob);
  scene.Monster(kSecondMonsterId, kOwnerX + 6, kPoringJob);

  // La première cible : son ouverture, puis son skill, puis l'attaque.
  const auto opener = scene.Tick(kStartMs);
  REQUIRE(IsKind(opener, CompanionOrder::Kind::kSkill));
  CHECK(opener->skill_id == kOpenerId);
  CHECK(opener->target_id == kFirstMonsterId);
  scene.Strikes(kHomunculusId, kFirstMonsterId, kStartMs + 1, 1, kOpenerId);
  CHECK(IsKind(scene.Tick(kStartMs + 560), CompanionOrder::Kind::kAttack));

  // Elle tombe : la seconde a droit à son ouverture à elle.
  Actor dead = *scene.world.Find(kFirstMonsterId);
  dead.posture = ro::sim::Posture::kDead;
  scene.world.Spawn(dead);
  const auto second = scene.Tick(kStartMs + 1120);
  REQUIRE(IsKind(second, CompanionOrder::Kind::kSkill));
  CHECK(second->skill_id == kOpenerId);
  CHECK(second->target_id == kSecondMonsterId);
}

TEST_CASE("Tactiques : un skill refusé ne consomme pas l'usage", "[sim][companion_tactics]") {
  Scene scene(kLifJob);
  scene.settings.skills.offensive = OffensiveSkills::kChaseAndCombat;
  scene.facts.skills = {Skill(kBoltId)};
  CompanionTactic line = Line(kPoringJob, TacticRank::kMedium);
  line.skill_use = ro::sim::TacticSkillUse::kTimes;
  line.skill_use_count = 1;
  scene.Set(line);
  scene.Monster(kFirstMonsterId, kOwnerX + 2, kPoringJob);
  REQUIRE(IsKind(scene.Tick(kStartMs), CompanionOrder::Kind::kSkill));
  scene.brain.NoteSkillRefused(kBoltId, kStartMs + 50);
  // Suspendu, il attaque ; la suspension finie, le skill repart : rien n'a été
  // consommé.
  CHECK(IsKind(scene.Tick(kStartMs + 560), CompanionOrder::Kind::kAttack));
  CHECK(IsKind(scene.Tick(kStartMs + ro::sim::kSkillRefusalSuspendMs + 200),
               CompanionOrder::Kind::kSkill));
}

TEST_CASE("Tactiques : « En dernier » passe avant « Tenir »", "[sim][companion_tactics]") {
  Scene scene(kLifJob);
  scene.Set(Line(kPoringJob, TacticRank::kHold));
  scene.Set(Line(kFabreJob, TacticRank::kLast));
  scene.Monster(kFirstMonsterId, kOwnerX + 2, kPoringJob);
  scene.Monster(kSecondMonsterId, kOwnerX + 8, kFabreJob);
  static_cast<void>(scene.Tick(kStartMs));
  CHECK(scene.brain.target_id() == kSecondMonsterId);
}

TEST_CASE("Tactiques : le niveau imposé est borné à l'appris", "[sim][companion_tactics]") {
  Scene scene(kLifJob);
  scene.settings.skills.offensive = OffensiveSkills::kChaseAndCombat;
  scene.facts.skills = {Skill(kBoltId)};
  CompanionTactic line = Line(kPoringJob, TacticRank::kMedium);
  line.skill_use = ro::sim::TacticSkillUse::kOnceAtLevel;
  line.skill_use_count = kBoltLevel + 4;
  scene.Set(line);
  scene.Monster(kFirstMonsterId, kOwnerX + 2, kPoringJob);
  const auto order = scene.Tick(kStartMs);
  REQUIRE(IsKind(order, CompanionOrder::Kind::kSkill));
  CHECK(order->skill_level == kBoltLevel);
  // Et « N fois » ou « une fois au niveau L » à zéro sortent du cadre.
  line.skill_use_count = 0;
  CHECK_FALSE(ro::sim::ValidTactic(line));
  line.skill_use = ro::sim::TacticSkillUse::kTimes;
  CHECK_FALSE(ro::sim::ValidTactic(line));
}

TEST_CASE("Tactiques : un « Ignorer » posé en plein combat lâche la cible",
          "[sim][companion_tactics]") {
  Scene scene(kLifJob);
  scene.Set(Line(kPoringJob, TacticRank::kMedium));
  scene.Monster(kFirstMonsterId, kOwnerX + 2, kPoringJob);
  REQUIRE(IsKind(scene.Tick(kStartMs), CompanionOrder::Kind::kAttack));
  scene.Set(Line(kPoringJob, TacticRank::kIgnore));
  CHECK_FALSE(scene.Tick(kStartMs + 560).has_value());
  CHECK(scene.brain.target_id() == 0);
}

TEST_CASE("Tactiques : « s'il ne bouge pas » se juge au choix de la cible",
          "[sim][companion_tactics]") {
  Scene scene(kLifJob);
  scene.world.Spawn(MakeActor(kStrangerId, ActorKind::kPlayer, kOwnerX + 4));
  scene.Monster(kFirstMonsterId, kOwnerX + 2, kPoringJob);
  scene.Strikes(kStrangerId, kFirstMonsterId, kStartMs - 100);
  CompanionTactic line = Line(kPoringJob, TacticRank::kMedium);
  line.share = ro::sim::TacticShare::kIfStill;
  scene.Set(line);
  REQUIRE(IsKind(scene.Tick(kStartMs), CompanionOrder::Kind::kAttack));
  // Engagée, elle fait un pas : il ne la lâche pas.
  REQUIRE(scene.world.StartMoving(
      kFirstMonsterId,
      CellMovement{CellPosition{kOwnerX + 2, kRowY, 0}, CellPosition{kOwnerX + 3, kRowY, 0}, 8, 8},
      kStartMs + 100));
  static_cast<void>(scene.Tick(kStartMs + 140));
  CHECK(scene.brain.target_id() == kFirstMonsterId);
}
