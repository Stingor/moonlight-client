// LES INVOCATIONS VUES PAR L'IA D'UN COMPAGNON : celles du camp ne sont jamais
// ciblées seules, celles d'un autre joueur relèvent du partage, les sbires d'un
// monstre suivent la ligne « sbires », et un maître inconnu laisse un monstre
// ordinaire.

#include <cstdint>
#include <optional>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/companion_ai.h"
#include "ro/sim/companion_tactics.h"
#include "ro/sim/summon_master.h"
#include "ro/sim/world.h"

using ro::sim::Actor;
using ro::sim::ActorKind;
using ro::sim::AiCompanion;
using ro::sim::CellPosition;
using ro::sim::CompanionAiSettings;
using ro::sim::CompanionBrain;
using ro::sim::CompanionFacts;
using ro::sim::CompanionOrder;
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
constexpr std::uint32_t kMercenaryId = 210;
constexpr std::uint32_t kSummonId = 301;
constexpr std::uint32_t kBossId = 302;
constexpr std::uint32_t kMemberId = 400;
constexpr std::uint32_t kStrangerId = 401;
// L'homoncule d'un autre joueur, dont le serveur peut dire le maître.
constexpr std::uint32_t kOtherHomunculusId = 410;
// Un maître que le serveur nomme, mais qu'on ne voit pas.
constexpr std::uint32_t kUnseenMasterId = 999;
constexpr std::uint16_t kFilirJob = 6003;
constexpr std::uint16_t kMercenaryJob = 6017;
constexpr std::uint16_t kFloraJob = 1118;
constexpr std::uint16_t kHornetJob = 2158;
constexpr std::uint16_t kPoringJob = 1002;
constexpr std::uint16_t kChestJob = 1324;
constexpr std::uint16_t kOwnerX = 100;
constexpr std::uint16_t kRowY = 100;
// Hors de la vue du compagnon : le maître d'un sbire n'est pas une cible.
constexpr std::uint16_t kFarX = 140;
constexpr std::uint16_t kSpeedMsPerCell = 150;
constexpr std::uint32_t kAmotionMs = 500;
constexpr std::uint32_t kStartMs = 100000;

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

[[nodiscard]] CompanionTactic Generic(TacticKeyKind kind, TacticRank rank) {
  return CompanionTactic{.key = TacticKey{.kind = kind}, .rank = rank};
}

// Le maître en (100, 100), son homoncule Filir en (101, 100) et son mercenaire
// en (99, 100). Le cerveau éprouvé est celui de `kind`, qui attaque de lui-même.
struct Scene {
  World world;
  CompanionBrain brain;
  CompanionFacts facts;
  CompanionAiSettings settings;

  explicit Scene(AiCompanion kind = AiCompanion::kHomunculus) {
    world.Spawn(MakeActor(kOwnerId, ActorKind::kPlayer, kOwnerX));
    world.set_player_id(kOwnerId);
    Actor homunculus = MakeActor(kHomunculusId, ActorKind::kNonPlayer, kOwnerX + 1, kFilirJob);
    homunculus.familiar = Familiar::kHomunculus;
    world.Spawn(homunculus);
    Actor mercenary = MakeActor(kMercenaryId, ActorKind::kNonPlayer, kOwnerX - 1, kMercenaryJob);
    mercenary.familiar = Familiar::kMercenary;
    world.Spawn(mercenary);
    const bool homunculus_brain = kind == AiCompanion::kHomunculus;
    const std::uint16_t job = homunculus_brain ? kFilirJob : kMercenaryJob;
    facts = CompanionFacts{.actor_id = homunculus_brain ? kHomunculusId : kMercenaryId,
                           .kind = kind,
                           .job = job,
                           .active = true,
                           .hp = 100,
                           .max_hp = 100,
                           .sp = 100,
                           .max_sp = 100,
                           .amotion_ms = kAmotionMs,
                           .attack_range_cells = 1,
                           .partner_id = homunculus_brain ? kMercenaryId : kHomunculusId,
                           .sp_known = true};
    settings = ro::sim::BasicCompanionAiSettings(kind, job);
    settings.attack_on_own = true;
    settings.spare_others_monsters = true;
    settings.skills.offensive = OffensiveSkills::kNever;
  }

  // Un monstre à deux cases du compagnon, invoqué par `master_id` (zéro : aucun).
  void Summon(std::uint32_t master_id, std::uint16_t job = kPoringJob) {
    world.Spawn(MakeActor(kSummonId, ActorKind::kMonster, kOwnerX + 3, job));
    if (master_id != 0) {
      REQUIRE(ro::sim::TellSummonMaster(world, kSummonId, master_id));
    }
  }

  void Strikes(std::uint32_t attacker, std::uint32_t target, std::uint32_t now_ms) {
    static_cast<void>(world.RecordStrike(
        Strike{.attacker = attacker, .target = target, .started_ms = now_ms, .damage = 1}));
  }

  void Set(const CompanionTactic& tactic) { REQUIRE(settings.tactics.Set(tactic)); }

  // Vrai quand le compagnon s'en prend à l'invocation.
  [[nodiscard]] bool TakesSummon(std::uint32_t now_ms) {
    const std::optional<CompanionOrder> order = brain.Tick(world, facts, settings, now_ms);
    return brain.target_id() == kSummonId ||
           (order.has_value() && order->kind == CompanionOrder::Kind::kAttack &&
            order->target_id == kSummonId);
  }
};

}  // namespace

TEST_CASE("Invocations : la plante du maître n'est jamais ciblée, même si elle frappe",
          "[sim][companion_summons]") {
  Scene scene;
  scene.Summon(kOwnerId, kFloraJob);
  SECTION("ni par l'initiative, ni par la défense") {
    scene.Strikes(kSummonId, kHomunculusId, kStartMs - 100);
    scene.Strikes(kSummonId, kOwnerId, kStartMs - 100);
    CHECK_FALSE(scene.TakesSummon(kStartMs));
  }
  SECTION("ni par une ligne de sa classe, ni par « tout autre monstre »") {
    scene.Set(CompanionTactic{.key = TacticKey{.kind = TacticKeyKind::kMonster, .job = kFloraJob},
                              .rank = TacticRank::kAbsolute,
                              .trigger = TacticTrigger::kAlways});
    scene.Set(Generic(TacticKeyKind::kAnyMonster, TacticRank::kHigh));
    CHECK_FALSE(scene.TakesSummon(kStartMs));
  }
  SECTION("un ordre direct reste permis") {
    scene.brain.Command(DirectOrder{.kind = DirectOrder::Kind::kAttack, .target_id = kSummonId});
    CHECK(scene.TakesSummon(kStartMs));
  }
}

TEST_CASE("Invocations : la Légion de notre homoncule, ignorée par le mercenaire",
          "[sim][companion_summons]") {
  Scene scene(AiCompanion::kMercenary);
  scene.Summon(kHomunculusId, kHornetJob);
  scene.Strikes(kSummonId, kMercenaryId, kStartMs - 100);
  CHECK_FALSE(scene.TakesSummon(kStartMs));
}

TEST_CASE("Invocations : celle d'un membre du groupe est du camp, même membre hors de vue",
          "[sim][companion_summons]") {
  Scene scene;
  scene.Summon(kMemberId, kFloraJob);
  SECTION("sans groupe : un maître hors de vue laisse un monstre ordinaire") {
    CHECK(scene.TakesSummon(kStartMs));
  }
  SECTION("dans le groupe : jamais ciblée") {
    scene.facts.party_member_ids = {kMemberId};
    CHECK_FALSE(scene.TakesSummon(kStartMs));
  }
}

TEST_CASE("Invocations : celle d'un étranger relève du partage, quoi qu'elle fasse",
          "[sim][companion_summons]") {
  Scene scene;
  scene.world.Spawn(MakeActor(kStrangerId, ActorKind::kPlayer, kOwnerX + 6));
  // Elle ne frappe personne : seul son maître dit à qui elle est.
  scene.Summon(kStrangerId, kFloraJob);
  SECTION("le réglage du niveau 1 l'épargne") { CHECK_FALSE(scene.TakesSummon(kStartMs)); }
  SECTION("et la ligne de tactique peut la partager") {
    CompanionTactic line = Generic(TacticKeyKind::kAnyMonster, TacticRank::kMedium);
    line.share = ro::sim::TacticShare::kAlways;
    scene.Set(line);
    CHECK(scene.TakesSummon(kStartMs));
  }
}

TEST_CASE("Invocations : la Légion de l'homoncule d'un autre se juge par son joueur",
          "[sim][companion_summons]") {
  Scene scene;
  Actor homunculus = MakeActor(kOtherHomunculusId, ActorKind::kNonPlayer, kOwnerX + 6, kFilirJob);
  homunculus.familiar = Familiar::kHomunculus;
  scene.world.Spawn(homunculus);
  scene.Summon(kOtherHomunculusId, kHornetJob);
  // Une ligne qui partage les invocations d'un autre joueur : seule celle du
  // camp lui échappe.
  CompanionTactic line = Generic(TacticKeyKind::kAnyMonster, TacticRank::kMedium);
  line.share = ro::sim::TacticShare::kAlways;
  scene.Set(line);
  SECTION("l'homoncule d'un membre du groupe, membre hors de vue : du camp") {
    REQUIRE(ro::sim::TellSummonMaster(scene.world, kOtherHomunculusId, kMemberId));
    scene.facts.party_member_ids = {kMemberId};
    CHECK_FALSE(scene.TakesSummon(kStartMs));
  }
  SECTION("l'homoncule d'un étranger : celle d'un autre joueur") {
    scene.world.Spawn(MakeActor(kStrangerId, ActorKind::kPlayer, kOwnerX + 8));
    REQUIRE(ro::sim::TellSummonMaster(scene.world, kOtherHomunculusId, kStrangerId));
    scene.facts.party_member_ids = {kMemberId};
    CHECK(scene.TakesSummon(kStartMs));
  }
  SECTION("un homoncule dont le joueur n'est pas dit : celle d'un autre joueur") {
    scene.facts.party_member_ids = {kMemberId};
    CHECK(scene.TakesSummon(kStartMs));
  }
}

TEST_CASE("Invocations : un sbire suit la ligne « sbires »", "[sim][companion_summons]") {
  Scene scene;
  scene.world.Spawn(MakeActor(kBossId, ActorKind::kMonster, kFarX, kPoringJob));
  SECTION("sans ligne, rien ne change") {
    scene.Summon(kBossId);
    CHECK(scene.TakesSummon(kStartMs));
  }
  SECTION("« sbires » à Ignorer") {
    scene.Summon(kBossId);
    scene.Set(Generic(TacticKeyKind::kMinion, TacticRank::kIgnore));
    CHECK_FALSE(scene.TakesSummon(kStartMs));
  }
  SECTION("une ligne de sa classe passe avant « sbires »") {
    scene.Summon(kBossId);
    scene.Set(Generic(TacticKeyKind::kMinion, TacticRank::kIgnore));
    scene.Set(CompanionTactic{.key = TacticKey{.kind = TacticKeyKind::kMonster, .job = kPoringJob},
                              .rank = TacticRank::kMedium});
    CHECK(scene.TakesSummon(kStartMs));
  }
  SECTION("« sbires » passe avant « coffres »") {
    scene.Summon(kBossId, kChestJob);
    scene.Set(Generic(TacticKeyKind::kChest, TacticRank::kIgnore));
    scene.Set(Generic(TacticKeyKind::kMinion, TacticRank::kMedium));
    CHECK(scene.TakesSummon(kStartMs));
  }
}

TEST_CASE("Invocations : un maître inconnu laisse un monstre ordinaire",
          "[sim][companion_summons]") {
  Scene scene;
  scene.Set(Generic(TacticKeyKind::kMinion, TacticRank::kIgnore));
  SECTION("aucun maître annoncé") {
    scene.Summon(0);
    CHECK(scene.TakesSummon(kStartMs));
  }
  SECTION("un maître hors de vue, hors du camp") {
    scene.Summon(kUnseenMasterId);
    CHECK(scene.TakesSummon(kStartMs));
  }
}
