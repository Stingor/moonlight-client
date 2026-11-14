// LES GARDES DES RÈGLES SI/ALORS : un retour au maître qui ne boucle pas, la
// trace des skills instantanés, le silence des règles sous un ordre direct, les
// cibles mortes ou « Ignorer », les cases non marchables, le quota par cible de
// combat, le refus muet, et « ne rien faire » qui garde les déplacements de
// sûreté.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/companion_ai.h"
#include "ro/sim/companion_rules.h"
#include "ro/sim/walk_map.h"
#include "ro/sim/world.h"

#include "companion_rule_scene.h"

using namespace companion_rule_scene;
using ro::sim::CompanionOrder;
using ro::sim::CompanionRule;
using ro::sim::RuleAction;
using ro::sim::RuleAim;
using ro::sim::RuleComparison;
using ro::sim::RuleCondition;
using ro::sim::RulePredicate;
using ro::sim::RuleSubject;

namespace {

constexpr std::uint32_t kTraceDelayMs = 50;
constexpr std::uint32_t kAfterSkillDelayMs = 560;
constexpr std::uint32_t kSettledMs = 1100;
constexpr std::uint32_t kWatchMs = 6000;
constexpr std::uint16_t kFarX = 108;
constexpr std::uint16_t kFabreJob = 1007;
constexpr std::int32_t kFullHealth = 100;
constexpr std::int32_t kScratchedHealth = 80;
constexpr int kMapCells = 200;

[[nodiscard]] CompanionRule Cast(std::uint16_t skill_id, RuleAim aim) {
  return CompanionRule{
      .action = RuleAction::kCastSkill, .aim = aim, .skill_id = skill_id, .conditions = {Always()}};
}

[[nodiscard]] CompanionRule Do(RuleAction action, RuleAim aim = RuleAim::kTarget) {
  return CompanionRule{.action = action, .aim = aim, .conditions = {Always()}};
}

[[nodiscard]] bool CastsOn(const std::optional<CompanionOrder>& order, std::uint16_t skill_id,
                           std::uint32_t target_id) {
  return IsKind(order, CompanionOrder::Kind::kSkill) && order->skill_id == skill_id &&
         order->target_id == target_id;
}

// LIF EN UNE AUTRE CASE, comme le serveur l'y range.
void PlaceCompanion(Scene& scene, std::uint16_t x) {
  ro::sim::Actor moved = MakeActor(kHomunculusId, ro::sim::ActorKind::kNonPlayer, x, kLifJob);
  moved.familiar = ro::sim::Familiar::kHomunculus;
  scene.world.Spawn(moved);
}

// UNE CARTE TOUTE MARCHABLE, sauf les colonnes de `blocked` : des murs nord-sud.
[[nodiscard]] ro::sim::WalkMap MapBlocking(const std::vector<std::uint16_t>& blocked) {
  std::vector<std::uint8_t> passable(static_cast<std::size_t>(kMapCells) * kMapCells, 1);
  for (const std::uint16_t x : blocked) {
    for (std::size_t y = 0; y < static_cast<std::size_t>(kMapCells); ++y) {
      passable[(y * kMapCells) + x] = 0;
    }
  }
  return ro::sim::WalkMap(kMapCells, kMapCells, std::move(passable));
}

}  // namespace

TEST_CASE("Gardes : revenir au maître ne boucle pas une fois arrivé", "[sim][companion_rules]") {
  Scene scene;
  PlaceCompanion(scene, kFarX);
  int returns = 0;
  SECTION("par une règle") { scene.settings.rules = {Do(RuleAction::kReturnToOwner)}; }
  SECTION("par le maître assis") {
    scene.settings.owner_seated = ro::sim::OwnerSeated::kStayBesidePassive;
    REQUIRE(scene.world.SetPosture(kOwnerId, ro::sim::Posture::kSitting, kStartMs));
  }
  for (std::uint32_t now = kStartMs; now < kStartMs + kWatchMs; now += kTickMs) {
    // LE SERVEUR RANGE LE COMPAGNON À DEUX CASES du maître, une seconde après.
    if (now == kStartMs + (8 * kTickMs)) {
      PlaceCompanion(scene, kOwnerX + 2);
    }
    returns += IsKind(scene.Tick(now), CompanionOrder::Kind::kReturnToOwner) ? 1 : 0;
  }
  CHECK(returns == 1);
}

TEST_CASE("Gardes : un soin ou une case visée laissent leur trace", "[sim][companion_rules]") {
  Scene scene;
  std::uint32_t now = kStartMs;
  RuleCondition healed = RuleCondition{.subject = RuleSubject::kCompanion,
                                       .predicate = RulePredicate::kCastWithin,
                                       .comparison = RuleComparison::kIs,
                                       .seconds = 3,
                                       .id = kHealId};
  RuleCondition not_healed = healed;
  not_healed.comparison = RuleComparison::kIsNot;
  // Jamais lancé, et rien d'envoyé : « n'a pas lancé » est vrai.
  CHECK(scene.Holds(not_healed, now));
  scene.settings.rules = {Cast(kHealId, RuleAim::kOwner)};
  REQUIRE(CastsOn(scene.Tick(now += kTickMs), kHealId, kOwnerId));
  // Envoyé, sans trace encore : inconnu, donc faux des deux côtés.
  CHECK_FALSE(scene.Holds(not_healed, now += kTickMs));
  CHECK_FALSE(scene.Holds(healed, now += kTickMs));
  // LA TRACE TELLE QU'ELLE ARRIVE : l'emploi d'un skill de soutien.
  REQUIRE(scene.world.RecordSupport(
      kOwnerId,
      ro::sim::SupportHit{
          .skill_id = kHealId, .source_id = kHomunculusId, .succeeded = true, .started_ms = now}));
  CHECK(scene.Holds(healed, now += kTickMs));
  CHECK_FALSE(scene.Holds(not_healed, now += kTickMs));
  // Plus de trois secondes après, sans nouvel envoi : vrai.
  CHECK(scene.Holds(not_healed, now += 3000));

  // ET LA CASE VISÉE d'un skill au sol.
  RuleCondition grounded = healed;
  grounded.id = kGroundId;
  CHECK_FALSE(scene.Holds(grounded, now += kTickMs));
  REQUIRE(scene.world.RecordGroundSkill(
      kHomunculusId,
      ro::sim::GroundSkillUse{.skill_id = kGroundId, .level = 1, .cell = {}, .started_ms = now}));
  CHECK(scene.Holds(grounded, now += kTickMs));
}

TEST_CASE("Gardes : la trace d'un soin n'est pas un refus muet", "[sim][companion_rules]") {
  Scene scene;
  std::uint32_t now = kStartMs;
  CompanionRule heal = Cast(kHealId, RuleAim::kOwner);
  heal.own_delay_ms = 10000;
  scene.settings.rules = {heal, Cast(kSelfBuffId, RuleAim::kSelf)};
  REQUIRE(CastsOn(scene.Tick(now), kHealId, kOwnerId));
  REQUIRE(scene.world.RecordSupport(kOwnerId, ro::sim::SupportHit{.skill_id = kHealId,
                                                                  .source_id = kHomunculusId,
                                                                  .succeeded = true,
                                                                  .started_ms = now + 1}));
  // Le soin compte dans son délai : la règle suivante a la main…
  CHECK(CastsOn(scene.Tick(now += kAfterSkillDelayMs), kSelfBuffId, kHomunculusId));
  // … et le soin repart à l'échéance de son délai, pas avant.
  bool healed_early = false;
  for (now += kTickMs; now < kStartMs + 10000; now += kTickMs) {
    healed_early = healed_early || CastsOn(scene.Tick(now), kHealId, kOwnerId);
  }
  CHECK_FALSE(healed_early);
}

TEST_CASE("Gardes : un refus muet suspend la règle", "[sim][companion_rules]") {
  Scene scene;
  std::uint32_t now = kStartMs;
  scene.settings.rules = {Cast(kSelfBuffId, RuleAim::kSelf)};
  REQUIRE(CastsOn(scene.Tick(now), kSelfBuffId, kHomunculusId));
  // Aucune trace en deux secondes : tenu pour refusé, la règle se tait.
  now += ro::sim::kSkillTraceMs + kTickMs;
  CHECK_FALSE(CastsOn(scene.Tick(now), kSelfBuffId, kHomunculusId));
  CHECK(CastsOn(scene.Tick(now + ro::sim::kSkillRefusalSuspendMs), kSelfBuffId, kHomunculusId));
}

TEST_CASE("Gardes : la file réservée et la case tenue font taire les règles",
          "[sim][companion_rules]") {
  Scene scene;
  std::uint32_t now = kStartMs;
  SECTION("un long « aller ici » se poursuit par sa file") {
    scene.settings.rules = {Do(RuleAction::kReturnToOwner)};
    constexpr std::uint16_t kFarAwayX = 140;
    scene.brain.Command(ro::sim::DirectOrder{.kind = ro::sim::DirectOrder::Kind::kMove,
                                             .cell = ro::sim::CellPosition{kFarAwayX, kRowY, 0}});
    const std::optional<CompanionOrder> first = scene.Tick(now);
    REQUIRE(IsKind(first, CompanionOrder::Kind::kMove));
    REQUIRE(scene.brain.reserved_count() > 0);
    PlaceCompanion(scene, first->cell.x);
    const std::optional<CompanionOrder> next = scene.Tick(now += kSettledMs);
    const std::optional<CompanionOrder> after =
        next.has_value() ? next : scene.Tick(now += kTickMs);
    CHECK(IsKind(after, CompanionOrder::Kind::kMove));
    CHECK_FALSE(scene.brain.deciding_rule().has_value());
  }
  SECTION("un combat parti de la case tenue") {
    scene.settings.move_order_holds_cell = true;
    scene.settings.move_order_hold_fights = true;
    constexpr std::uint16_t kHeldX = kOwnerX + 3;
    scene.brain.Command(ro::sim::DirectOrder{.kind = ro::sim::DirectOrder::Kind::kMove,
                                             .cell = ro::sim::CellPosition{kHeldX, kRowY, 0}});
    REQUIRE(IsKind(scene.Tick(now), CompanionOrder::Kind::kMove));
    // EN CHEMIN, il ne garde encore rien ; arrivé, sa case est celle qu'il
    // garde, et elle le reste pendant le combat qui en part.
    CHECK_FALSE(scene.brain.held_cell().has_value());
    PlaceCompanion(scene, kHeldX);
    static_cast<void>(scene.Tick(now += kSettledMs));
    const ro::sim::CellPosition held{kHeldX, kRowY, 0};
    CHECK(scene.brain.held_cell() == std::optional(held));
    scene.Monster(kFirstMonsterId, kHeldX + 1);
    scene.Strikes(kFirstMonsterId, kOwnerId, now);
    scene.settings.rules = {Do(RuleAction::kReturnToOwner)};
    bool returned = false;
    bool attacked = false;
    for (int step = 0; step < 4; ++step) {
      const std::optional<CompanionOrder> order = scene.Tick(now += kTickMs);
      returned = returned || IsKind(order, CompanionOrder::Kind::kReturnToOwner);
      attacked = attacked || IsKind(order, CompanionOrder::Kind::kAttack);
    }
    CHECK(attacked);
    CHECK_FALSE(returned);
    CHECK(scene.brain.held_cell() == std::optional(held));
  }
}

TEST_CASE("Gardes : un ami mort n'est pas le plus blessé, le maître mort non plus",
          "[sim][companion_rules]") {
  Scene scene;
  ro::sim::Actor dead = MakeActor(kFriendId, ro::sim::ActorKind::kPlayer, kOwnerX - 2);
  dead.name = "Alice";
  dead.posture = ro::sim::Posture::kDead;
  scene.world.Spawn(dead);
  REQUIRE(scene.world.SetHealth(kFriendId, 0, kFullHealth));
  ro::sim::Actor hurt = MakeActor(kFriendId + 1, ro::sim::ActorKind::kPlayer, kOwnerX - 3);
  hurt.name = "Bob";
  scene.world.Spawn(hurt);
  REQUIRE(scene.world.SetHealth(kFriendId + 1, kScratchedHealth, kFullHealth));
  scene.settings.friends = {"Alice", "Bob"};
  scene.settings.rules = {Cast(kHealId, RuleAim::kFriend)};
  CHECK(CastsOn(scene.Tick(kStartMs), kHealId, kFriendId + 1));

  Scene alone;
  REQUIRE(alone.world.SetPosture(kOwnerId, ro::sim::Posture::kDead, kStartMs));
  alone.settings.rules = {Cast(kHealId, RuleAim::kOwner)};
  CHECK_FALSE(CastsOn(alone.Tick(kStartMs), kHealId, kOwnerId));
}

TEST_CASE("Gardes : la meilleure case est marchable, et ne compte ni « Ignorer » ni l'interdit",
          "[sim][companion_rules]") {
  Scene scene;
  scene.facts.skills[3].area_radius_cells = 1;
  scene.Monster(kFirstMonsterId, kOwnerX + 4);
  scene.Monster(kSecondMonsterId, kOwnerX + 5);
  scene.settings.rules = {Cast(kGroundId, RuleAim::kBestCell)};
  SECTION("une case bloquée ne se vise pas") {
    const ro::sim::WalkMap map = MapBlocking({kOwnerX + 4});
    scene.facts.walk_map = &map;
    const std::optional<CompanionOrder> order = scene.Tick(kStartMs);
    REQUIRE(IsKind(order, CompanionOrder::Kind::kSkill));
    CHECK(order->cell.y == kRowY);
    CHECK(order->cell.x == kOwnerX + 5);
    scene.facts.walk_map = nullptr;
  }
  SECTION("les monstres « Ignorer » ne comptent pas") {
    scene.Monster(kThirdMonsterId, kOwnerX + 8, kFabreJob);
    REQUIRE(scene.settings.tactics.Set(ro::sim::CompanionTactic{
        .key = ro::sim::TacticKey{.kind = ro::sim::TacticKeyKind::kMonster, .job = kPoringJob},
        .rank = ro::sim::TacticRank::kIgnore}));
    const std::optional<CompanionOrder> order = scene.Tick(kStartMs);
    REQUIRE(IsKind(order, CompanionOrder::Kind::kSkill));
    CHECK(order->cell.x == kOwnerX + 7);
  }
}

TEST_CASE("Gardes : une règle ne vise pas un monstre « Ignorer »", "[sim][companion_rules]") {
  Scene scene;
  scene.Monster(kFirstMonsterId, kOwnerX + 2);
  scene.Strikes(kFirstMonsterId, kOwnerId, kStartMs);
  REQUIRE(scene.settings.tactics.Set(ro::sim::CompanionTactic{
      .key = ro::sim::TacticKey{.kind = ro::sim::TacticKeyKind::kMonster, .job = kPoringJob},
      .rank = ro::sim::TacticRank::kIgnore}));
  scene.settings.rules = {Do(RuleAction::kAttack, RuleAim::kOwnerAttacker),
                          Cast(kBoltId, RuleAim::kOwnerAttacker)};
  const std::optional<CompanionOrder> order = scene.Tick(kStartMs);
  CHECK_FALSE(IsKind(order, CompanionOrder::Kind::kAttack));
  CHECK_FALSE(IsKind(order, CompanionOrder::Kind::kSkill));
}

TEST_CASE("Gardes : s'écarter contre un mur est infaisable", "[sim][companion_rules]") {
  Scene scene;
  scene.Monster(kFirstMonsterId, kOwnerX + 2);
  scene.Strikes(kOwnerId, kFirstMonsterId, kStartMs);
  // Lif en 101, le monstre en 102 : s'écarter de quatre mène en 98, bloquée.
  const ro::sim::WalkMap map = MapBlocking({kOwnerX - 2});
  scene.facts.walk_map = &map;
  scene.settings.rules = {CompanionRule{.action = RuleAction::kStepAway,
                                        .aim = RuleAim::kOwnerTarget,
                                        .step_cells = 4,
                                        .conditions = {Always()}},
                          Cast(kSelfBuffId, RuleAim::kSelf)};
  CHECK(CastsOn(scene.Tick(kStartMs), kSelfBuffId, kHomunculusId));
  CHECK(scene.brain.deciding_rule()->index == 1);
  scene.facts.walk_map = nullptr;
}

TEST_CASE("Gardes : « attaquer » lâche une cible qui ne reçoit aucun coup",
          "[sim][companion_rules]") {
  Scene scene;
  scene.Monster(kFirstMonsterId, kOwnerX + 2);
  scene.settings.rules = {Do(RuleAction::kAttack, RuleAim::kOwnerAttacker)};
  std::optional<std::uint32_t> last_attack_ms;
  for (std::uint32_t now = kStartMs; now < kStartMs + 15000; now += kTickMs) {
    scene.Strikes(kFirstMonsterId, kOwnerId, now);
    if (IsKind(scene.Tick(now), CompanionOrder::Kind::kAttack)) {
      last_attack_ms = now;
    }
  }
  REQUIRE(last_attack_ms.has_value());
  CHECK(*last_attack_ms < kStartMs + 11000);
}

TEST_CASE("Gardes : le quota se compte par cible de combat", "[sim][companion_rules]") {
  Scene scene;
  std::uint32_t now = kStartMs;
  CompanionRule buff = Cast(kSelfBuffId, RuleAim::kSelf);
  buff.per_target_most = 1;
  buff.conditions = {
      RuleCondition{.subject = RuleSubject::kCompanion,
                    .predicate = RulePredicate::kActivity,
                    .comparison = RuleComparison::kIs,
                    .choice = static_cast<int>(ro::sim::CompanionActivity::kFighting)}};
  scene.settings.rules = {buff};
  const auto buffs_within = [&scene, &now](std::uint32_t span_ms) {
    int count = 0;
    for (const std::uint32_t end = now + span_ms; now < end; now += kTickMs) {
      if (CastsOn(scene.Tick(now), kSelfBuffId, kHomunculusId)) {
        ++count;
        REQUIRE(
            scene.world.RecordSupport(kHomunculusId, ro::sim::SupportHit{.skill_id = kSelfBuffId,
                                                                         .source_id = kHomunculusId,
                                                                         .succeeded = true,
                                                                         .started_ms = now + 1}));
      }
    }
    return count;
  };
  scene.Monster(kFirstMonsterId, kOwnerX + 2);
  scene.Strikes(kFirstMonsterId, kOwnerId, now);
  CHECK(buffs_within(3000) == 1);
  // UN AUTRE COMBAT : le quota repart.
  REQUIRE(scene.world.SetPosture(kFirstMonsterId, ro::sim::Posture::kDead, now));
  scene.Monster(kSecondMonsterId, kOwnerX + 2);
  scene.Strikes(kSecondMonsterId, kOwnerId, now);
  CHECK(buffs_within(3000) == 1);
}

TEST_CASE("Gardes : un skill de soutien se vise sur soi", "[sim][companion_rules]") {
  Scene scene;
  scene.settings.rules = {Cast(kHealId, RuleAim::kSelf)};
  CHECK(CastsOn(scene.Tick(kStartMs), kHealId, kHomunculusId));
}

TEST_CASE("Gardes : la SP à garder de la tactique vaut pour une règle", "[sim][companion_rules]") {
  Scene scene;
  scene.Monster(kFirstMonsterId, kOwnerX + 2);
  scene.Engage(kFirstMonsterId, kStartMs);
  REQUIRE(scene.settings.tactics.Set(ro::sim::CompanionTactic{
      .key = ro::sim::TacticKey{.kind = ro::sim::TacticKeyKind::kMonster, .job = kPoringJob},
      .sp_reserve_points = 95}));
  CompanionRule bolt = Cast(kBoltId, RuleAim::kTarget);
  scene.settings.rules = {bolt};
  CHECK_FALSE(CastsOn(scene.Tick(kStartMs + kAfterSkillDelayMs), kBoltId, kFirstMonsterId));
  bolt.ignores_sp_reserve = true;
  scene.settings.rules = {bolt};
  CHECK(CastsOn(scene.Tick(kStartMs + (2 * kAfterSkillDelayMs)), kBoltId, kFirstMonsterId));
}

TEST_CASE("Gardes : éviter « s'il attaque » compte les attaques sur un ami",
          "[sim][companion_rules]") {
  Scene scene;
  ro::sim::Actor pal = MakeActor(kFriendId, ro::sim::ActorKind::kPlayer, kOwnerX + 5);
  pal.name = "Alice";
  scene.world.Spawn(pal);
  scene.Monster(kFirstMonsterId, kOwnerX + 3);
  REQUIRE(scene.settings.tactics.Set(ro::sim::CompanionTactic{
      .key = ro::sim::TacticKey{.kind = ro::sim::TacticKeyKind::kMonster, .job = kPoringJob},
      .rank = ro::sim::TacticRank::kIgnore,
      .avoid = ro::sim::TacticAvoid::kIfAttacks}));
  scene.Strikes(kFirstMonsterId, kFriendId, kStartMs);
  SECTION("un inconnu frappé : rien") {
    CHECK_FALSE(IsKind(scene.Tick(kStartMs), CompanionOrder::Kind::kMove));
  }
  SECTION("un ami frappé : il s'écarte") {
    scene.settings.friends = {"Alice"};
    CHECK(IsKind(scene.Tick(kStartMs), CompanionOrder::Kind::kMove));
    CHECK(scene.brain.last_reason() == ro::sim::CompanionAiReason::kAvoid);
  }
}

TEST_CASE("Gardes : « ne rien faire » garde les déplacements de sûreté", "[sim][companion_rules]") {
  Scene scene;
  std::uint32_t now = kStartMs;
  SECTION("SP basses : il suit encore le maître qui s'éloigne") {
    scene.facts.sp = 5;
    scene.settings.rules = {Guard(RuleCondition{.subject = RuleSubject::kCompanion,
                                                .predicate = RulePredicate::kSpPercent,
                                                .comparison = RuleComparison::kBelow,
                                                .number = 10})};
    ro::sim::Actor away = MakeActor(kOwnerId, ro::sim::ActorKind::kPlayer, kOwnerX + 10);
    scene.world.Spawn(away);
    CHECK(IsKind(scene.Tick(now), CompanionOrder::Kind::kReturnToOwner));
    CHECK(scene.brain.deciding_rule().has_value());
  }
  SECTION("PV bas, fuite réglée : il fuit") {
    scene.facts.hp = 15;
    scene.settings.flee_below_hp_percent = 30;
    scene.settings.rules = {Guard(RuleCondition{.subject = RuleSubject::kCompanion,
                                                .predicate = RulePredicate::kHpPercent,
                                                .comparison = RuleComparison::kBelow,
                                                .number = 20})};
    scene.Monster(kFirstMonsterId, kOwnerX + 2);
    scene.Strikes(kFirstMonsterId, kHomunculusId, now);
    CHECK(IsKind(scene.Tick(now), CompanionOrder::Kind::kMove));
    CHECK(scene.brain.last_reason() == ro::sim::CompanionAiReason::kFlee);
  }
  SECTION("ni attaque ni skill") {
    scene.settings.skills.offensive = ro::sim::OffensiveSkills::kInCombat;
    scene.settings.attack_on_own = true;
    scene.settings.rules = {Do(RuleAction::kDoNothing)};
    scene.Monster(kFirstMonsterId, kOwnerX + 2);
    bool offended = false;
    for (int step = 0; step < 20; ++step) {
      scene.Strikes(kFirstMonsterId, kOwnerId, now);
      const std::optional<CompanionOrder> order = scene.Tick(now += kTickMs);
      offended = offended || IsKind(order, CompanionOrder::Kind::kAttack) ||
                 IsKind(order, CompanionOrder::Kind::kSkill);
    }
    CHECK_FALSE(offended);
  }
}

TEST_CASE("Gardes : l'état d'une règle la suit quand la liste change", "[sim][companion_rules]") {
  Scene scene;
  std::uint32_t now = kStartMs;
  constexpr std::uint32_t kHealDelayMs = 10000;
  constexpr std::uint32_t kLookMs = 5000;
  CompanionRule heal = Cast(kHealId, RuleAim::kOwner);
  heal.own_delay_ms = kHealDelayMs;
  // Une règle jamais vraie : les PV du compagnon sous zéro.
  const CompanionRule idle = Guard(RuleCondition{.subject = RuleSubject::kCompanion,
                                                 .predicate = RulePredicate::kHpPercent,
                                                 .comparison = RuleComparison::kBelow,
                                                 .number = 0});
  scene.settings.rules = {heal, idle};
  REQUIRE(CastsOn(scene.Tick(now), kHealId, kOwnerId));
  REQUIRE(scene.world.RecordSupport(kOwnerId, ro::sim::SupportHit{.skill_id = kHealId,
                                                                  .source_id = kHomunculusId,
                                                                  .succeeded = true,
                                                                  .started_ms = now + 1}));
  // L'AUTRE RÈGLE PASSE DEVANT : le soin garde son délai à son nouveau rang.
  scene.settings.rules = {idle, heal};
  bool healed_early = false;
  for (now += kAfterSkillDelayMs; now < kStartMs + kLookMs; now += kTickMs) {
    healed_early = healed_early || CastsOn(scene.Tick(now), kHealId, kOwnerId);
  }
  CHECK_FALSE(healed_early);
  // UNE COPIE PART À NEUF, l'original garde son délai.
  scene.settings.rules = {idle, heal, heal};
  REQUIRE(CastsOn(scene.Tick(now += kTickMs), kHealId, kOwnerId));
  REQUIRE(scene.brain.deciding_rule().has_value());
  CHECK(scene.brain.deciding_rule()->index == 2);
  REQUIRE(scene.world.RecordSupport(kOwnerId, ro::sim::SupportHit{.skill_id = kHealId,
                                                                  .source_id = kHomunculusId,
                                                                  .succeeded = true,
                                                                  .started_ms = now + 1}));
  // LA RÈGLE MODIFIÉE PART À NEUF, elle seule.
  CompanionRule changed = heal;
  changed.own_delay_ms = kHealDelayMs * 2;
  scene.settings.rules = {idle, changed, heal};
  REQUIRE(CastsOn(scene.Tick(now += kAfterSkillDelayMs), kHealId, kOwnerId));
  CHECK(scene.brain.deciding_rule()->index == 1);
}
