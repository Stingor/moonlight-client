// Le compteur à quatre modes : ce que nous et le groupe infligeons, soignons,
// encaissons et recevons, segment par segment.

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/combat_notice.h"
#include "ro/sim/damage_meter.h"
#include "ro/sim/world.h"

using ro::sim::Actor;
using ro::sim::ActorKind;
using ro::sim::CellPosition;
using ro::sim::MeterActor;
using ro::sim::MeterMode;
using ro::sim::World;

namespace {

constexpr std::uint32_t kSelf = 7;
constexpr std::uint32_t kPriest = 8;
constexpr std::uint32_t kStranger = 9;
constexpr std::uint32_t kOrc = 1000;
constexpr std::uint32_t kStormUnit = 5000;

Actor Someone(std::uint32_t id, ActorKind kind, const char* name) {
  Actor actor;
  actor.id = id;
  actor.kind = kind;
  actor.position = CellPosition{5, 5, 0};
  actor.name = name;
  return actor;
}

// Un monde où nous jouons avec un prêtre, près d'un inconnu et d'un orc.
World Party() {
  World world;
  world.set_player_id(kSelf);
  world.Spawn(Someone(kSelf, ActorKind::kPlayer, "Moi"));
  world.Spawn(Someone(kPriest, ActorKind::kPlayer, "Prêtre"));
  world.Spawn(Someone(kStranger, ActorKind::kPlayer, "Inconnu"));
  world.Spawn(Someone(kOrc, ActorKind::kMonster, "Orc"));
  const std::vector<MeterActor> party = {MeterActor{.id = kPriest, .name = "Prêtre"}};
  world.damage_meter().Track(kSelf, "Moi", party);
  return world;
}

ro::sim::Strike Hit(std::uint32_t attacker, std::uint32_t target, std::int32_t damage,
                    std::uint32_t at_ms) {
  ro::sim::Strike strike;
  strike.attacker = attacker;
  strike.target = target;
  strike.damage = damage;
  strike.started_ms = at_ms;
  return strike;
}

ro::sim::SupportHit Heal(std::uint32_t healer, std::int32_t amount, std::uint32_t at_ms) {
  return ro::sim::SupportHit{.skill_id = ro::sim::kHealAnnounceSkill,
                             .source_id = healer,
                             .amount = amount,
                             .succeeded = true,
                             .started_ms = at_ms};
}

std::int64_t TotalOf(const ro::sim::MeterSegment& segment, MeterMode mode, std::uint32_t id) {
  const auto& by_actor = segment.of(mode);
  const auto found = by_actor.find(id);
  return found != by_actor.end() ? found->second.total.amount : 0;
}

}  // namespace

TEST_CASE("Compteur : le groupe compte, l'inconnu non", "[sim][dpsmeter]") {
  World world = Party();
  REQUIRE(world.RecordStrike(Hit(kSelf, kOrc, 300, 1000)));
  REQUIRE(world.RecordStrike(Hit(kPriest, kOrc, 120, 1100)));
  REQUIRE(world.RecordStrike(Hit(kStranger, kOrc, 999, 1200)));
  const ro::sim::MeterSegment& fight = world.damage_meter().current();
  CHECK(TotalOf(fight, MeterMode::kDamageDone, kSelf) == 300);
  CHECK(TotalOf(fight, MeterMode::kDamageDone, kPriest) == 120);
  CHECK(TotalOf(fight, MeterMode::kDamageDone, kStranger) == 0);
  CHECK(fight.of(MeterMode::kDamageDone).at(kPriest).name == "Prêtre");
}

TEST_CASE("Compteur : ce qu'on encaisse se compte à la cible, par source", "[sim][dpsmeter]") {
  World world = Party();
  ro::sim::Strike bash = Hit(kOrc, kSelf, 80, 1000);
  bash.skill_id = 5;
  REQUIRE(world.RecordStrike(bash));
  REQUIRE(world.RecordStrike(Hit(kOrc, kPriest, 40, 1100)));
  const ro::sim::MeterSegment& fight = world.damage_meter().current();
  CHECK(TotalOf(fight, MeterMode::kDamageTaken, kSelf) == 80);
  CHECK(TotalOf(fight, MeterMode::kDamageTaken, kPriest) == 40);
  // Encaisser ouvre aussi le combat, et se ventile par compétence.
  CHECK(world.damage_meter().in_combat());
  CHECK(fight.of(MeterMode::kDamageTaken).at(kSelf).skills.at(5).amount == 80);
  CHECK(world.damage_meter().records(MeterMode::kDamageTaken).front().blow.other_name == "Orc");
}

TEST_CASE("Compteur : un soin crédite le soigneur et le soigné", "[sim][dpsmeter]") {
  World world = Party();
  REQUIRE(world.RecordStrike(Hit(kSelf, kOrc, 10, 1000)));
  static_cast<void>(world.RecordSupport(kSelf, Heal(kPriest, 500, 1100)));
  const ro::sim::MeterSegment& fight = world.damage_meter().current();
  CHECK(TotalOf(fight, MeterMode::kHealingDone, kPriest) == 500);
  CHECK(TotalOf(fight, MeterMode::kHealingTaken, kSelf) == 500);
  CHECK(world.damage_meter().total(MeterMode::kHealingTaken) == 500);

  // ⚠ UNE COMPÉTENCE SANS DÉGÂTS QUI N'EST PAS UN SOIN porte son NIVEAU dans
  // le même champ : la compter ferait de chaque Blessing un soin de dix.
  ro::sim::SupportHit blessing = Heal(kPriest, 10, 1200);
  blessing.skill_id = 34;
  static_cast<void>(world.RecordSupport(kSelf, blessing));
  CHECK(TotalOf(world.damage_meter().current(), MeterMode::kHealingDone, kPriest) == 500);
}

TEST_CASE("Compteur : une unité au sol est créditée à son lanceur", "[sim][dpsmeter]") {
  World world = Party();
  ro::sim::SkillUnit storm;
  storm.id = kStormUnit;
  storm.creator_id = kPriest;
  world.PlaceSkillUnit(storm);
  REQUIRE(world.RecordStrike(Hit(kStormUnit, kOrc, 250, 1000)));
  CHECK(TotalOf(world.damage_meter().current(), MeterMode::kDamageDone, kPriest) == 250);

  // ⚠ LA NÔTRE, NON : le cumul privé du serveur la compte déjà.
  ro::sim::SkillUnit mine;
  mine.id = kStormUnit + 1;
  mine.creator_id = kSelf;
  world.PlaceSkillUnit(mine);
  REQUIRE(world.RecordStrike(Hit(kStormUnit + 1, kOrc, 250, 1100)));
  CHECK(TotalOf(world.damage_meter().current(), MeterMode::kDamageDone, kSelf) == 0);
}

TEST_CASE("Compteur : un soin rendu deux fois ne compte qu'une", "[sim][dpsmeter]") {
  World world = Party();
  static_cast<void>(world.RecordSupport(kSelf, Heal(0, 300, 1000)));
  // Le même soin, rendu à nous seuls dans la même trame.
  world.damage_meter().NoteRecovery(300, 1000);
  CHECK(TotalOf(world.damage_meter().session(), MeterMode::kHealingTaken, kSelf) == 300);
  // Une potion bue plus tard, elle, compte.
  world.damage_meter().NoteRecovery(300, 2000);
  CHECK(TotalOf(world.damage_meter().session(), MeterMode::kHealingTaken, kSelf) == 600);
}

TEST_CASE("Compteur : Highness Heal compte, un monstre soigné non", "[sim][dpsmeter]") {
  World world = Party();
  REQUIRE(world.RecordStrike(Hit(kSelf, kOrc, 10, 1000)));
  // Annoncé sous son propre numéro.
  ro::sim::SupportHit highness = Heal(kPriest, 900, 1100);
  highness.skill_id = 2051;
  static_cast<void>(world.RecordSupport(kSelf, highness));
  CHECK(TotalOf(world.damage_meter().current(), MeterMode::kHealingDone, kPriest) == 900);
  CHECK(
      world.damage_meter().current().of(MeterMode::kHealingDone).at(kPriest).skills.contains(2051));
  // ⚠ SANCTUARY SOIGNE AUSSI L'ORC : ce n'est pas un soin du groupe.
  static_cast<void>(world.RecordSupport(kOrc, Heal(kPriest, 400, 1200)));
  CHECK(TotalOf(world.damage_meter().current(), MeterMode::kHealingDone, kPriest) == 900);
}

TEST_CASE("Compteur : un soin annoncé puis rendu plafonné ne compte qu'une fois",
          "[sim][dpsmeter]") {
  // ⚠ L'ANNONCE PORTE LE SOIN DEMANDÉ, LE RENDU LE SOIN EFFECTIF : il nous
  // manquait trois cents PV, Epiclesis en annonce deux mille.
  World world = Party();
  static_cast<void>(world.RecordSupport(kSelf, Heal(kPriest, 2000, 1000)));
  world.damage_meter().NoteRecovery(300, 1000);
  CHECK(TotalOf(world.damage_meter().session(), MeterMode::kHealingTaken, kSelf) == 2000);
}

TEST_CASE("Compteur : un combat oublié garde son temps dans la session", "[sim][dpsmeter]") {
  World world = Party();
  ro::sim::DamageMeter& meter = world.damage_meter();
  REQUIRE(world.RecordStrike(Hit(kSelf, kOrc, 100, 1000)));
  REQUIRE(world.RecordStrike(Hit(kSelf, kOrc, 100, 4000)));
  meter.Forget();
  // Les montants restent dans la session : leur temps aussi.
  CHECK(TotalOf(meter.session(), MeterMode::kDamageDone, kSelf) == 200);
  CHECK(meter.active_ms(meter.session(), 5000) == 3000);
}

TEST_CASE("Compteur : un soin hors combat a sa courbe et son bilan", "[sim][dpsmeter]") {
  // 🔴 AUCUN COMBAT : rien n'a frappé. Le soin a pourtant son activité.
  World world = Party();
  ro::sim::DamageMeter& meter = world.damage_meter();
  meter.settings().slot_ms = 200;
  static_cast<void>(world.RecordSupport(kPriest, Heal(kSelf, 400, 1000)));
  REQUIRE_FALSE(meter.in_combat());
  CHECK(meter.active(MeterMode::kHealingDone));
  CHECK(meter.total(MeterMode::kHealingDone) == 400);
  meter.Tick(1200);
  REQUIRE(meter.plot_offset(MeterMode::kHealingDone) == 1);
  CHECK(meter.plot(MeterMode::kHealingDone)[0] > 0.0F);
  CHECK(meter.peak(MeterMode::kHealingDone) > 0.0F);
  // Les dégâts, eux, n'ont pas bougé : chaque mode a son activité.
  CHECK_FALSE(meter.active(MeterMode::kDamageDone));
  CHECK(meter.plot_offset(MeterMode::kDamageDone) == 0);
  // Le silence la ferme, et le bilan reste celui de la dernière.
  meter.Tick(10000);
  CHECK_FALSE(meter.active(MeterMode::kHealingDone));
  CHECK(meter.total(MeterMode::kHealingDone) == 400);
  CHECK(meter.activity_span_ms(MeterMode::kHealingDone, 10000) == 0);
}

TEST_CASE("Compteur : les segments — en cours, précédent, session", "[sim][dpsmeter]") {
  World world = Party();
  ro::sim::DamageMeter& meter = world.damage_meter();
  meter.settings().combat_timeout_secs = 5;
  REQUIRE(world.RecordStrike(Hit(kSelf, kOrc, 100, 1000)));
  REQUIRE(world.RecordStrike(Hit(kSelf, kOrc, 100, 3000)));
  meter.Tick(9000);
  REQUIRE_FALSE(meter.in_combat());
  // Le combat clos garde son temps actif : du premier au dernier coup.
  CHECK(meter.active_ms(meter.current(), 9000) == 2000);

  // Un second combat : le premier devient le précédent.
  REQUIRE(world.RecordStrike(Hit(kSelf, kOrc, 50, 20000)));
  CHECK(TotalOf(meter.current(), MeterMode::kDamageDone, kSelf) == 50);
  CHECK(TotalOf(meter.previous(), MeterMode::kDamageDone, kSelf) == 200);
  CHECK(TotalOf(meter.session(), MeterMode::kDamageDone, kSelf) == 250);
  // La session ne compte que le temps de combat, pas les silences.
  CHECK(meter.active_ms(meter.session(), 21000) == 2000 + 1000);

  // ⚠ UN SOIN HORS COMBAT VA À LA SESSION SEULE.
  meter.Tick(40000);
  static_cast<void>(world.RecordSupport(kSelf, Heal(kPriest, 70, 40000)));
  CHECK(TotalOf(meter.session(), MeterMode::kHealingDone, kPriest) == 70);
  CHECK(TotalOf(meter.current(), MeterMode::kHealingDone, kPriest) == 0);
}
