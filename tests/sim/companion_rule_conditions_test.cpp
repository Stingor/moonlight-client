// LES CONDITIONS DES RÈGLES SI/ALORS : chaque prédicat vrai, faux, et inconnu
// tenu pour faux — formes négatives comprises —, et le vocabulaire fermé.

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/companion_ai.h"
#include "ro/sim/companion_rules.h"
#include "ro/sim/companion_statuses.h"
#include "ro/sim/monster_sheet.h"
#include "ro/sim/world.h"

#include "companion_rule_scene.h"

using namespace companion_rule_scene;
using ro::sim::RuleComparison;
using ro::sim::RuleCondition;
using ro::sim::RulePredicate;
using ro::sim::RuleSubject;

namespace {

constexpr int kHalf = 50;
constexpr std::int32_t kLowHealth = 20;
constexpr std::int32_t kFullHealth = 100;
constexpr ro::sim::StatusEffectId kBlessing = 10;
constexpr std::uint32_t kStatusLastingMs = 60000;
constexpr int kTwoSeconds = 2;
constexpr std::uint32_t kTwoSecondsMs = 2000;
constexpr std::uint8_t kDemonRace = 6;
constexpr std::uint8_t kBossRank = 2;

[[nodiscard]] RuleCondition Of(RuleSubject subject, RulePredicate predicate,
                               RuleComparison comparison) {
  return RuleCondition{.subject = subject, .predicate = predicate, .comparison = comparison};
}

}  // namespace

TEST_CASE("Conditions : les PV en pour cent, connus ou non", "[sim][companion_rules]") {
  Scene scene;
  std::uint32_t now = kStartMs;
  RuleCondition below = Of(RuleSubject::kOwner, RulePredicate::kHpPercent, RuleComparison::kBelow);
  below.number = kHalf;
  RuleCondition above = below;
  above.comparison = RuleComparison::kAbove;
  SECTION("le maître") {
    REQUIRE(scene.world.SetHealth(kOwnerId, kLowHealth, kFullHealth));
    CHECK(scene.Holds(below, now));
    CHECK_FALSE(scene.Holds(above, now += kTickMs));
    REQUIRE(scene.world.SetHealth(kOwnerId, ro::sim::kUnknownHealth, ro::sim::kUnknownHealth));
    CHECK_FALSE(scene.Holds(below, now += kTickMs));
    CHECK_FALSE(scene.Holds(above, now += kTickMs));
  }
  SECTION("le compagnon") {
    below.subject = RuleSubject::kCompanion;
    above.subject = RuleSubject::kCompanion;
    scene.facts.hp = kLowHealth;
    CHECK(scene.Holds(below, now));
    CHECK_FALSE(scene.Holds(above, now += kTickMs));
    scene.facts.max_hp = 0;
    CHECK_FALSE(scene.Holds(below, now += kTickMs));
    CHECK_FALSE(scene.Holds(above, now += kTickMs));
  }
  SECTION("la cible : présente, de PV inconnus, absente") {
    below.subject = RuleSubject::kTarget;
    CHECK_FALSE(scene.Holds(below, now));
    scene.Monster(kFirstMonsterId, kOwnerX + 2);
    scene.Engage(kFirstMonsterId, now += kTickMs);
    CHECK_FALSE(scene.Holds(below, now += kTickMs));
    REQUIRE(scene.world.SetHealth(kFirstMonsterId, kLowHealth, kFullHealth));
    CHECK(scene.Holds(below, now += kTickMs));
  }
}

TEST_CASE("Conditions : les SP du compagnon", "[sim][companion_rules]") {
  Scene scene;
  std::uint32_t now = kStartMs;
  scene.facts.sp = kLowHealth;
  RuleCondition percent =
      Of(RuleSubject::kCompanion, RulePredicate::kSpPercent, RuleComparison::kBelow);
  percent.number = kHalf;
  RuleCondition points =
      Of(RuleSubject::kCompanion, RulePredicate::kSpPoints, RuleComparison::kAbove);
  points.number = kLowHealth;
  CHECK(scene.Holds(percent, now));
  CHECK_FALSE(scene.Holds(points, now += kTickMs));
  points.number = kLowHealth - 1;
  CHECK(scene.Holds(points, now += kTickMs));
  scene.facts.sp_known = false;
  CHECK_FALSE(scene.Holds(percent, now += kTickMs));
  CHECK_FALSE(scene.Holds(points, now += kTickMs));
}

TEST_CASE("Conditions : attaqué depuis, et le poids des assaillants", "[sim][companion_rules]") {
  Scene scene;
  std::uint32_t now = kStartMs;
  scene.Monster(kFirstMonsterId, kOwnerX + 3);
  scene.Monster(kSecondMonsterId, kOwnerX + 4);
  RuleCondition since =
      Of(RuleSubject::kCompanion, RulePredicate::kAttackedFor, RuleComparison::kIs);
  since.seconds = kTwoSeconds;
  RuleCondition weight =
      Of(RuleSubject::kCompanion, RulePredicate::kAttackersWeight, RuleComparison::kIs);
  weight.number = 2;
  CHECK_FALSE(scene.Holds(since, now));
  scene.Strikes(kFirstMonsterId, kHomunculusId, now);
  CHECK_FALSE(scene.Holds(since, now += kTickMs));
  CHECK_FALSE(scene.Holds(weight, now += kTickMs));
  scene.Strikes(kSecondMonsterId, kHomunculusId, now);
  CHECK(scene.Holds(weight, now += kTickMs));
  now += kTwoSecondsMs;
  scene.Strikes(kFirstMonsterId, kHomunculusId, now);
  CHECK(scene.Holds(since, now));
  SECTION("le poids d'une tactique compte") {
    weight.number = 3;
    CHECK_FALSE(scene.Holds(weight, now += kTickMs));
    REQUIRE(scene.settings.tactics.Set(ro::sim::CompanionTactic{
        .key = ro::sim::TacticKey{.kind = ro::sim::TacticKeyKind::kAnyMonster}, .weight = 2.0F}));
    CHECK(scene.Holds(weight, now += kTickMs));
  }
}

TEST_CASE("Conditions : les statuts, vus ou non", "[sim][companion_rules]") {
  Scene scene;
  std::uint32_t now = kStartMs;
  RuleCondition wears = Of(RuleSubject::kOwner, RulePredicate::kHasStatus, RuleComparison::kIs);
  wears.id = kBlessing;
  RuleCondition lacks = wears;
  lacks.comparison = RuleComparison::kIsNot;
  SECTION("le maître : toujours connu") {
    CHECK_FALSE(scene.Holds(wears, now));
    CHECK(scene.Holds(lacks, now += kTickMs));
    scene.world.NotePlayerStatus(kBlessing, true, kStatusLastingMs, now);
    CHECK(scene.Holds(wears, now += kTickMs));
    CHECK_FALSE(scene.Holds(lacks, now += kTickMs));
  }
  SECTION("le compagnon : un statut absent est inconnu, « ne porte pas » est faux") {
    wears.subject = RuleSubject::kCompanion;
    lacks.subject = RuleSubject::kCompanion;
    CHECK_FALSE(scene.Holds(wears, now));
    CHECK_FALSE(scene.Holds(lacks, now += kTickMs));
    REQUIRE(scene.world.SetStatusEffect(kHomunculusId, kBlessing, true));
    CHECK(scene.Holds(wears, now += kTickMs));
    CHECK_FALSE(scene.Holds(lacks, now += kTickMs));
  }
  SECTION("le compagnon : une liste du serveur fraîche décide les deux formes") {
    wears.subject = RuleSubject::kCompanion;
    lacks.subject = RuleSubject::kCompanion;
    // Une liste valide et vide : rien n'est porté.
    scene.facts.statuses_ms = now;
    CHECK(scene.Holds(lacks, now += kTickMs));
    CHECK_FALSE(scene.Holds(wears, now += kTickMs));
    // Périmée : « ne porte pas » redevient faux.
    CHECK_FALSE(scene.Holds(lacks, now += ro::sim::kCompanionStatusListFreshMs));
    // Fraîche, et le statut y est.
    scene.facts.statuses = {kBlessing};
    scene.facts.statuses_ms = now;
    CHECK(scene.Holds(wears, now += kTickMs));
    CHECK_FALSE(scene.Holds(lacks, now += kTickMs));
  }
}

TEST_CASE("Conditions : la liste des statuts du compagnon, demandée au bon rythme",
          "[sim][companion_rules]") {
  ro::sim::CompanionStatusWatch watch;
  const ro::sim::Actor self = MakeActor(kHomunculusId, ro::sim::ActorKind::kNonPlayer, kOwnerX);
  std::uint32_t now = kStartMs;
  // Aucune règle ne lit de statut, ou le compagnon hors de vue : rien.
  CHECK_FALSE(watch.ShouldAsk(false, &self, now));
  CHECK_FALSE(watch.ShouldAsk(true, nullptr, now));
  CHECK(watch.ShouldAsk(true, &self, now));
  CHECK_FALSE(watch.ShouldAsk(true, &self, now += kTickMs));
  CHECK(watch.ShouldAsk(true, &self, now += ro::sim::kCompanionStatusAskMs));
  // UNE TRACE NEUVE D'UN SKILL DU COMPAGNON redemande aussitôt.
  ro::sim::Actor casting = self;
  casting.skill_use = ro::sim::SkillUse{.skill_id = kSelfBuffId, .started_ms = now + kTickMs};
  CHECK(watch.ShouldAsk(true, &casting, now += kTickMs));
  CHECK_FALSE(watch.ShouldAsk(true, &casting, now += kTickMs));
  // Une réponse qui ne fait pas foi ne compte pas.
  const std::vector<ro::sim::StatusEffectId> blessed = {kBlessing};
  watch.Receive(false, blessed, now);
  CHECK_FALSE(watch.received_ms().has_value());
  watch.Receive(true, {}, now);
  CHECK(watch.received_ms() == now);
  CHECK(watch.statuses().empty());
  watch.Forget();
  CHECK_FALSE(watch.received_ms().has_value());
}

TEST_CASE("Conditions : le maître assis, l'activité du compagnon", "[sim][companion_rules]") {
  Scene scene;
  std::uint32_t now = kStartMs;
  const RuleCondition sitting =
      Of(RuleSubject::kOwner, RulePredicate::kSitting, RuleComparison::kIs);
  CHECK_FALSE(scene.Holds(sitting, now));
  REQUIRE(scene.world.SetPosture(kOwnerId, ro::sim::Posture::kSitting, now));
  CHECK(scene.Holds(sitting, now += kTickMs));
  REQUIRE(scene.world.SetPosture(kOwnerId, ro::sim::Posture::kStanding, now));

  RuleCondition resting =
      Of(RuleSubject::kCompanion, RulePredicate::kActivity, RuleComparison::kIs);
  resting.choice = static_cast<int>(ro::sim::CompanionActivity::kResting);
  RuleCondition fighting = resting;
  fighting.choice = static_cast<int>(ro::sim::CompanionActivity::kFighting);
  RuleCondition chasing = resting;
  chasing.choice = static_cast<int>(ro::sim::CompanionActivity::kChasing);
  CHECK(scene.Holds(resting, now += kTickMs));
  scene.Monster(kFirstMonsterId, kOwnerX + 6);
  scene.Engage(kFirstMonsterId, now += kTickMs);
  CHECK(scene.Holds(chasing, now += kTickMs));
  CHECK_FALSE(scene.Holds(fighting, now += kTickMs));
  scene.Monster(kSecondMonsterId, kOwnerX + 2);
  scene.brain = ro::sim::CompanionBrain();
  scene.settings.rules.clear();
  scene.Engage(kSecondMonsterId, now += kTickMs);
  CHECK(scene.Holds(fighting, now += kTickMs));
}

TEST_CASE("Conditions : un skill lancé, une apparition, la distance au maître",
          "[sim][companion_rules]") {
  Scene scene;
  std::uint32_t now = kStartMs;
  RuleCondition cast = Of(RuleSubject::kCompanion, RulePredicate::kCastWithin, RuleComparison::kIs);
  cast.id = kSelfBuffId;
  cast.seconds = kTwoSeconds;
  CHECK_FALSE(scene.Holds(cast, now));
  REQUIRE(scene.world.RecordCast(kHomunculusId,
                                 ro::sim::Cast{.skill_id = kSelfBuffId, .started_ms = now}));
  CHECK(scene.Holds(cast, now += kTickMs));
  CHECK_FALSE(scene.Holds(cast, now += kTwoSecondsMs));

  RuleCondition appeared =
      Of(RuleSubject::kCompanion, RulePredicate::kAppearedWithin, RuleComparison::kIs);
  appeared.seconds = kTwoSeconds;
  // Ni apparition notée ni carte : inconnu.
  CHECK_FALSE(scene.Holds(appeared, now += kTickMs));
  scene.brain.ForgetMap(false, now);
  CHECK(scene.Holds(appeared, now += kTickMs));
  CHECK_FALSE(scene.Holds(appeared, now += kTwoSecondsMs));

  RuleCondition far = Of(RuleSubject::kCompanion, RulePredicate::kFartherThan, RuleComparison::kIs);
  far.cells = 3;
  CHECK_FALSE(scene.Holds(far, now += kTickMs));
  far.cells = 0;
  CHECK(scene.Holds(far, now += kTickMs));
}

TEST_CASE("Conditions : la fiche de la cible, absente puis arrivée", "[sim][companion_rules]") {
  Scene scene;
  std::uint32_t now = kStartMs;
  scene.Monster(kFirstMonsterId, kOwnerX + 2);
  scene.Engage(kFirstMonsterId, now);
  RuleCondition demon = Of(RuleSubject::kTarget, RulePredicate::kRace, RuleComparison::kIs);
  demon.choice = kDemonRace;
  RuleCondition not_demon = demon;
  not_demon.comparison = RuleComparison::kIsNot;
  RuleCondition boss = Of(RuleSubject::kTarget, RulePredicate::kMonsterRank, RuleComparison::kIs);
  boss.choice = kBossRank;
  // ABSENTE : faux des deux côtés, et la fiche se demande.
  CHECK_FALSE(scene.Holds(demon, now += kTickMs));
  CHECK(scene.brain.wanted_monster_sheet() == kPoringJob);
  CHECK_FALSE(scene.Holds(not_demon, now += kTickMs));
  scene.sheets.Receive(kPoringJob, ro::sim::MonsterSheet{.race = kDemonRace, .boss_rank = 0});
  CHECK(scene.Holds(demon, now += kTickMs));
  CHECK_FALSE(scene.Holds(not_demon, now += kTickMs));
  CHECK_FALSE(scene.Holds(boss, now += kTickMs));
  CHECK(scene.brain.wanted_monster_sheet() == 0);

  RuleCondition poring = Of(RuleSubject::kTarget, RulePredicate::kMonsterJob, RuleComparison::kIs);
  poring.id = kPoringJob;
  CHECK(scene.Holds(poring, now += kTickMs));
}

TEST_CASE("Conditions : la cible incante, et qui elle attaque", "[sim][companion_rules]") {
  Scene scene;
  std::uint32_t now = kStartMs;
  scene.Monster(kFirstMonsterId, kOwnerX + 2);
  scene.Engage(kFirstMonsterId, now);
  const RuleCondition casting =
      Of(RuleSubject::kTarget, RulePredicate::kCasting, RuleComparison::kIs);
  CHECK_FALSE(scene.Holds(casting, now += kTickMs));
  REQUIRE(scene.world.RecordCast(
      kFirstMonsterId,
      ro::sim::Cast{.skill_id = kBoltId, .started_ms = now, .duration_ms = kTwoSecondsMs}));
  CHECK(scene.Holds(casting, now += kTickMs));

  RuleCondition owner = Of(RuleSubject::kTarget, RulePredicate::kAttacks, RuleComparison::kIs);
  owner.choice = static_cast<int>(ro::sim::RuleWhom::kOwner);
  RuleCondition companion = owner;
  companion.choice = static_cast<int>(ro::sim::RuleWhom::kCompanion);
  RuleCondition ally = owner;
  ally.choice = static_cast<int>(ro::sim::RuleWhom::kFriend);
  CHECK(scene.Holds(owner, now += kTickMs));
  CHECK_FALSE(scene.Holds(companion, now += kTickMs));
  CHECK_FALSE(scene.Holds(ally, now += kTickMs));
  ro::sim::Actor pal = MakeActor(kFriendId, ro::sim::ActorKind::kPlayer, kOwnerX - 2);
  pal.name = "Alice";
  scene.world.Spawn(pal);
  scene.settings.friends = {"Alice"};
  scene.Strikes(kFirstMonsterId, kFriendId, now);
  CHECK(scene.Holds(ally, now += kTickMs));
}

TEST_CASE("Conditions : les ennemis alentour, comptés par leur poids", "[sim][companion_rules]") {
  Scene scene;
  std::uint32_t now = kStartMs;
  scene.Monster(kFirstMonsterId, kOwnerX + 3);
  scene.Monster(kSecondMonsterId, kOwnerX + 4);
  scene.Monster(kThirdMonsterId, kOwnerX + 12);
  RuleCondition near =
      Of(RuleSubject::kSurroundings, RulePredicate::kEnemiesNear, RuleComparison::kIs);
  near.number = 2;
  near.cells = 3;
  near.choice = static_cast<int>(ro::sim::RuleAround::kCompanion);
  CHECK(scene.Holds(near, now));
  near.number = 3;
  CHECK_FALSE(scene.Holds(near, now += kTickMs));
  near.choice = static_cast<int>(ro::sim::RuleAround::kTarget);
  near.number = 1;
  // Sans cible : inconnu.
  CHECK_FALSE(scene.Holds(near, now += kTickMs));
}

TEST_CASE("Conditions : le vocabulaire est fermé", "[sim][companion_rules]") {
  RuleCondition condition =
      Of(RuleSubject::kTarget, RulePredicate::kSpPercent, RuleComparison::kBelow);
  CHECK_FALSE(ro::sim::ValidCondition(condition));
  condition = Of(RuleSubject::kTarget, RulePredicate::kHpPercent, RuleComparison::kAbove);
  CHECK_FALSE(ro::sim::ValidCondition(condition));
  condition.comparison = RuleComparison::kBelow;
  CHECK(ro::sim::ValidCondition(condition));
  condition.number = ro::sim::kRulePercentMost + 1;
  CHECK_FALSE(ro::sim::ValidCondition(condition));
  ro::sim::CompanionRule rule = Guard(Always());
  rule.conditions.assign(ro::sim::kRuleConditionMost + 1, Always());
  CHECK_FALSE(ro::sim::ValidRule(rule));
  rule.conditions.clear();
  CHECK_FALSE(ro::sim::ValidRule(rule));
}
