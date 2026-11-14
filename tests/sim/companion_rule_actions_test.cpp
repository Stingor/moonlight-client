// LA LECTURE DES RÈGLES SI/ALORS ET LEURS ACTIONS : l'ordre, la règle
// infaisable qui laisse passer la suivante, « ne rien faire », chaque cible
// d'action et le genre du skill, la meilleure case, les attributs, la
// suspension d'une règle refusée, et les amis défendus.

#include <cstdint>
#include <optional>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/companion_ai.h"
#include "ro/sim/companion_rules.h"
#include "ro/sim/world.h"

#include "companion_rule_scene.h"

using namespace companion_rule_scene;
using ro::sim::CompanionOrder;
using ro::sim::CompanionRule;
using ro::sim::RuleAction;
using ro::sim::RuleAim;

namespace {

constexpr std::uint32_t kTraceDelayMs = 50;
// Plus que le délai d'usine entre deux skills.
constexpr std::uint32_t kAfterSkillDelayMs = 560;
constexpr std::uint32_t kRuleDelayMs = 10000;
constexpr std::uint16_t kSbr44Id = 8012;
constexpr std::uint16_t kDecreaseAgiId = 8234;
constexpr std::int32_t kHurtHealth = 30;
constexpr std::int32_t kScratchedHealth = 80;
constexpr std::int32_t kFullHealth = 100;
constexpr std::int64_t kHighReserve = 95;

[[nodiscard]] CompanionRule Cast(std::uint16_t skill_id, RuleAim aim) {
  return CompanionRule{
      .action = RuleAction::kCastSkill, .aim = aim, .skill_id = skill_id, .conditions = {Always()}};
}

[[nodiscard]] CompanionRule Never(RuleAction action) {
  ro::sim::RuleCondition never = Always();
  never.comparison = ro::sim::RuleComparison::kBelow;
  return CompanionRule{.action = action, .conditions = {never}};
}

[[nodiscard]] bool CastsOn(const std::optional<CompanionOrder>& order, std::uint16_t skill_id,
                           std::uint32_t target_id) {
  return IsKind(order, CompanionOrder::Kind::kSkill) && order->skill_id == skill_id &&
         order->target_id == target_id;
}

}  // namespace

TEST_CASE("Règles : de haut en bas, la première vraie et faisable décide",
          "[sim][companion_rules]") {
  Scene scene;
  scene.settings.rules = {
      Never(RuleAction::kDoNothing), Cast(kSelfBuffId, RuleAim::kSelf),
      CompanionRule{.action = RuleAction::kDoNothing, .conditions = {Always()}}};
  CHECK(CastsOn(scene.Tick(kStartMs), kSelfBuffId, kHomunculusId));
  REQUIRE(scene.brain.deciding_rule().has_value());
  CHECK(scene.brain.deciding_rule()->index == 1);
  CHECK(scene.brain.last_reason() == ro::sim::CompanionAiReason::kRule);
}

TEST_CASE("Règles : une règle vraie mais infaisable laisse passer la suivante",
          "[sim][companion_rules]") {
  Scene scene;
  // Sans cible de combat, le bolt n'a rien à viser.
  scene.settings.rules = {Cast(kBoltId, RuleAim::kTarget), Cast(kSelfBuffId, RuleAim::kSelf)};
  CHECK(CastsOn(scene.Tick(kStartMs), kSelfBuffId, kHomunculusId));
  CHECK(scene.brain.deciding_rule()->index == 1);
}

TEST_CASE("Règles : « ne rien faire » arrête tout, sauf un ordre direct",
          "[sim][companion_rules]") {
  Scene scene;
  scene.Monster(kFirstMonsterId, kOwnerX + 3);
  scene.Strikes(kFirstMonsterId, kOwnerId, kStartMs);
  scene.settings.rules = {
      CompanionRule{.action = RuleAction::kDoNothing, .conditions = {Always()}}};
  CHECK_FALSE(scene.Tick(kStartMs).has_value());
  CHECK(scene.brain.target_id() == 0);
  scene.brain.Command(ro::sim::DirectOrder{.kind = ro::sim::DirectOrder::Kind::kAttack,
                                           .target_id = kFirstMonsterId});
  CHECK(scene.Tick(kStartMs + kTickMs).has_value());
  CHECK(scene.brain.target_id() == kFirstMonsterId);
}

TEST_CASE("Règles : chaque cible d'action, et le genre du skill", "[sim][companion_rules]") {
  Scene scene;
  std::uint32_t now = kStartMs;
  scene.Monster(kFirstMonsterId, kOwnerX + 2);
  SECTION("la cible du combat") {
    scene.Engage(kFirstMonsterId, now);
    scene.settings.rules = {Cast(kBoltId, RuleAim::kTarget)};
    CHECK(CastsOn(scene.Tick(now += kAfterSkillDelayMs), kBoltId, kFirstMonsterId));
  }
  SECTION("soi, et le maître") {
    scene.settings.rules = {Cast(kHealId, RuleAim::kOwner)};
    CHECK(CastsOn(scene.Tick(now), kHealId, kOwnerId));
  }
  SECTION("ce que le maître attaque, ce qui attaque le maître") {
    scene.Strikes(kOwnerId, kFirstMonsterId, now);
    scene.settings.rules = {Cast(kBoltId, RuleAim::kOwnerTarget)};
    CHECK(CastsOn(scene.Tick(now), kBoltId, kFirstMonsterId));
    scene.Monster(kSecondMonsterId, kOwnerX - 3);
    scene.Strikes(kSecondMonsterId, kOwnerId, now);
    scene.brain = ro::sim::CompanionBrain();
    scene.settings.rules = {Cast(kBoltId, RuleAim::kOwnerAttacker)};
    CHECK(CastsOn(scene.Tick(now += kTickMs), kBoltId, kSecondMonsterId));
  }
  SECTION("l'ami le plus blessé, PV connus seulement") {
    ro::sim::Actor hurt = MakeActor(kFriendId, ro::sim::ActorKind::kPlayer, kOwnerX - 2);
    hurt.name = "Alice";
    scene.world.Spawn(hurt);
    REQUIRE(scene.world.SetHealth(kFriendId, kScratchedHealth, kFullHealth));
    ro::sim::Actor worse = MakeActor(kFriendId + 1, ro::sim::ActorKind::kPlayer, kOwnerX - 3);
    worse.name = "Bob";
    scene.world.Spawn(worse);
    REQUIRE(scene.world.SetHealth(kFriendId + 1, kHurtHealth, kFullHealth));
    ro::sim::Actor unknown = MakeActor(kFriendId + 2, ro::sim::ActorKind::kPlayer, kOwnerX - 4);
    unknown.name = "Chloe";
    scene.world.Spawn(unknown);
    scene.settings.friends = {"Alice", "Bob", "Chloe"};
    scene.settings.rules = {Cast(kHealId, RuleAim::kFriend)};
    CHECK(CastsOn(scene.Tick(now), kHealId, kFriendId + 1));
  }
  SECTION("la case sous la cible, la case sous le maître") {
    scene.Engage(kFirstMonsterId, now);
    scene.settings.rules = {Cast(kGroundId, RuleAim::kCellUnderTarget)};
    std::optional<CompanionOrder> order = scene.Tick(now += kAfterSkillDelayMs);
    REQUIRE(IsKind(order, CompanionOrder::Kind::kSkill));
    CHECK(order->cell.x == kOwnerX + 2);
    scene.settings.rules = {Cast(kGroundId, RuleAim::kCellUnderOwner)};
    // Le skill précédent attend sa trace : il part, et la règle neuve repart de zéro.
    REQUIRE(scene.world.RecordCast(
        kHomunculusId, ro::sim::Cast{.skill_id = kGroundId, .started_ms = now + kTraceDelayMs}));
    order = scene.Tick(now += kAfterSkillDelayMs);
    REQUIRE(IsKind(order, CompanionOrder::Kind::kSkill));
    CHECK(order->cell.x == kOwnerX);
    CHECK(order->target_id == 0);
  }
  SECTION("un skill d'un autre genre ne se vise pas là") {
    scene.Engage(kFirstMonsterId, now);
    scene.settings.rules = {Cast(kBoltId, RuleAim::kSelf), Cast(kBoltId, RuleAim::kOwner),
                            Cast(kHealId, RuleAim::kCellUnderOwner),
                            Cast(kSelfBuffId, RuleAim::kTarget)};
    CHECK_FALSE(IsKind(scene.Tick(now += kAfterSkillDelayMs), CompanionOrder::Kind::kSkill));
    CHECK_FALSE(scene.brain.deciding_rule().has_value());
  }
}

TEST_CASE("Règles : la meilleure case couvre le plus d'ennemis", "[sim][companion_rules]") {
  Scene scene;
  scene.facts.skills[3].area_radius_cells = 1;
  scene.Monster(kFirstMonsterId, kOwnerX + 4);
  scene.Monster(kSecondMonsterId, kOwnerX + 5);
  scene.Monster(kThirdMonsterId, kOwnerX + 9);
  scene.settings.rules = {Cast(kGroundId, RuleAim::kBestCell)};
  const std::optional<CompanionOrder> order = scene.Tick(kStartMs);
  REQUIRE(IsKind(order, CompanionOrder::Kind::kSkill));
  // Deux monstres sous la zone, et la case la plus proche qui les couvre.
  CHECK(order->cell.x == kOwnerX + 4);
  CHECK(order->cell.y == kRowY);
}

TEST_CASE("Règles : les attributs — délai propre, quota par cible, réserve ignorée",
          "[sim][companion_rules]") {
  Scene scene;
  std::uint32_t now = kStartMs;
  SECTION("le délai propre se compte à la trace") {
    CompanionRule buff = Cast(kSelfBuffId, RuleAim::kSelf);
    buff.own_delay_ms = kRuleDelayMs;
    scene.settings.rules = {buff};
    REQUIRE(CastsOn(scene.Tick(now), kSelfBuffId, kHomunculusId));
    REQUIRE(scene.world.RecordCast(
        kHomunculusId, ro::sim::Cast{.skill_id = kSelfBuffId, .started_ms = now + kTraceDelayMs}));
    CHECK_FALSE(scene.Tick(now += kAfterSkillDelayMs).has_value());
    CHECK(CastsOn(scene.Tick(now += kRuleDelayMs), kSelfBuffId, kHomunculusId));
  }
  SECTION("au plus N fois par cible") {
    scene.Monster(kFirstMonsterId, kOwnerX + 2);
    scene.Engage(kFirstMonsterId, now);
    CompanionRule bolt = Cast(kBoltId, RuleAim::kTarget);
    bolt.per_target_most = 1;
    scene.settings.rules = {bolt};
    REQUIRE(CastsOn(scene.Tick(now += kAfterSkillDelayMs), kBoltId, kFirstMonsterId));
    static_cast<void>(scene.world.RecordStrike(ro::sim::Strike{.attacker = kHomunculusId,
                                                               .target = kFirstMonsterId,
                                                               .started_ms = now + kTraceDelayMs,
                                                               .skill_id = kBoltId}));
    CHECK_FALSE(CastsOn(scene.Tick(now += kAfterSkillDelayMs), kBoltId, kFirstMonsterId));
  }
  SECTION("la SP à garder, ignorée ou non") {
    scene.settings.skills.sp_reserve_points = kHighReserve;
    CompanionRule buff = Cast(kSelfBuffId, RuleAim::kSelf);
    scene.settings.rules = {buff};
    CHECK_FALSE(scene.Tick(now).has_value());
    buff.ignores_sp_reserve = true;
    scene.settings.rules = {buff};
    CHECK(CastsOn(scene.Tick(now += kTickMs), kSelfBuffId, kHomunculusId));
  }
}

TEST_CASE("Règles : un refus suspend la règle, pas le skill", "[sim][companion_rules]") {
  Scene scene;
  std::uint32_t now = kStartMs;
  scene.settings.rules = {Cast(kSelfBuffId, RuleAim::kSelf), Cast(kSelfBuffId, RuleAim::kSelf)};
  // Les deux règles diffèrent par un attribut, pour qu'elles soient deux.
  scene.settings.rules[1].own_delay_ms = kTickMs;
  REQUIRE(CastsOn(scene.Tick(now), kSelfBuffId, kHomunculusId));
  REQUIRE(scene.brain.deciding_rule()->index == 0);
  scene.brain.NoteSkillRefused(kSelfBuffId, now + kTraceDelayMs);
  CHECK(CastsOn(scene.Tick(now += kAfterSkillDelayMs), kSelfBuffId, kHomunculusId));
  CHECK(scene.brain.deciding_rule()->index == 1);
  scene.brain.NoteSkillRefused(kSelfBuffId, now + kTraceDelayMs);
  // Les deux suspendues : rien ne part avant la fin de la suspension.
  CHECK_FALSE(scene.Tick(now += kAfterSkillDelayMs).has_value());
  CHECK(CastsOn(scene.Tick(now += ro::sim::kSkillRefusalSuspendMs), kSelfBuffId, kHomunculusId));
}

TEST_CASE("Règles : un skill qui ne part jamais seul est refusé aussi dans une règle",
          "[sim][companion_rules]") {
  Scene scene;
  scene.facts.skills.push_back(Skill(kSbr44Id, ro::sim::kSkillOnEnemy));
  scene.Monster(kFirstMonsterId, kOwnerX + 2);
  scene.Engage(kFirstMonsterId, kStartMs);
  const CompanionRule sbr = Cast(kSbr44Id, RuleAim::kTarget);
  CHECK_FALSE(ro::sim::ValidRule(sbr));
  scene.settings.rules = {sbr};
  CHECK_FALSE(CastsOn(scene.Tick(kStartMs + kAfterSkillDelayMs), kSbr44Id, kFirstMonsterId));
  // Un debuff, lui, y a sa place.
  CHECK(ro::sim::ValidRule(Cast(kDecreaseAgiId, RuleAim::kTarget)));
}

TEST_CASE("Règles : attaquer, s'écarter, revenir au maître", "[sim][companion_rules]") {
  Scene scene;
  std::uint32_t now = kStartMs;
  scene.Monster(kFirstMonsterId, kOwnerX + 2);
  scene.Strikes(kOwnerId, kFirstMonsterId, now);
  SECTION("attaquer ce que le maître attaque") {
    scene.settings.rules = {CompanionRule{
        .action = RuleAction::kAttack, .aim = RuleAim::kOwnerTarget, .conditions = {Always()}}};
    const std::optional<CompanionOrder> order = scene.Tick(now);
    REQUIRE(IsKind(order, CompanionOrder::Kind::kAttack));
    CHECK(order->target_id == kFirstMonsterId);
    CHECK(scene.brain.target_id() == kFirstMonsterId);
  }
  SECTION("s'écarter de la cible") {
    scene.settings.rules = {CompanionRule{.action = RuleAction::kStepAway,
                                          .aim = RuleAim::kOwnerTarget,
                                          .step_cells = 4,
                                          .conditions = {Always()}}};
    const std::optional<CompanionOrder> order = scene.Tick(now);
    REQUIRE(IsKind(order, CompanionOrder::Kind::kMove));
    CHECK(order->cell.x < kOwnerX + 1);
  }
  SECTION("revenir au maître") {
    ro::sim::Actor far =
        MakeActor(kHomunculusId, ro::sim::ActorKind::kNonPlayer, kOwnerX + 8, kLifJob);
    far.familiar = ro::sim::Familiar::kHomunculus;
    scene.world.Spawn(far);
    scene.settings.rules = {
        Never(RuleAction::kDoNothing),
        CompanionRule{.action = RuleAction::kReturnToOwner, .conditions = {Always()}}};
    CHECK(IsKind(scene.Tick(now), CompanionOrder::Kind::kReturnToOwner));
  }
}

TEST_CASE("Règles : « revenir au maître » garde la main pendant le retour",
          "[sim][companion_rules]") {
  constexpr std::uint16_t kFarDx = 8;
  Scene scene;
  ro::sim::Actor far =
      MakeActor(kHomunculusId, ro::sim::ActorKind::kNonPlayer, kOwnerX + kFarDx, kLifJob);
  far.familiar = ro::sim::Familiar::kHomunculus;
  scene.world.Spawn(far);
  // Un monstre qui frappe le maître : la base voudrait le défendre.
  scene.Monster(kFirstMonsterId, kOwnerX + kFarDx + 1);
  scene.Strikes(kFirstMonsterId, kOwnerId, kStartMs);
  scene.settings.rules = {
      CompanionRule{.action = RuleAction::kReturnToOwner, .conditions = {Always()}}};
  CHECK(IsKind(scene.Tick(kStartMs), CompanionOrder::Kind::kReturnToOwner));
  // 🔴 LE RETOUR EST EN COURS : la base ne reprend pas la poursuite.
  CHECK_FALSE(scene.Tick(kStartMs + kTickMs).has_value());
  CHECK(scene.brain.target_id() == 0);
}

TEST_CASE("Amis : défendus comme le maître", "[sim][companion_rules]") {
  Scene scene;
  ro::sim::Actor pal = MakeActor(kFriendId, ro::sim::ActorKind::kPlayer, kOwnerX - 2);
  pal.name = "Alice";
  scene.world.Spawn(pal);
  scene.Monster(kFirstMonsterId, kOwnerX - 3);
  scene.Strikes(kFirstMonsterId, kFriendId, kStartMs);
  SECTION("un inconnu ne l'est pas") {
    static_cast<void>(scene.Tick(kStartMs));
    CHECK(scene.brain.target_id() == 0);
  }
  SECTION("un ami l'est") {
    scene.settings.friends = {"Alice"};
    static_cast<void>(scene.Tick(kStartMs));
    CHECK(scene.brain.target_id() == kFirstMonsterId);
  }
  SECTION("sous la case « défendre le maître »") {
    scene.settings.friends = {"Alice"};
    scene.settings.defend_owner = false;
    static_cast<void>(scene.Tick(kStartMs));
    CHECK(scene.brain.target_id() == 0);
  }
}
