// LE CÔNE ET LA BARRIÈRE : Fire Ball frappe ce qui est devant le héros, dans
// son cap ; Ice Wall pose des blocs que la horde ne traverse pas, qui gèlent
// qui les frappe, et qu'un boss brise.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_news.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/survivors_state.h"

#include "support/survivors_content_fixture.h"
#include "support/survivors_fixture.h"

using ro::sim::CastKind;
using ro::sim::Cells;
using ro::sim::CellTenths;
using ro::sim::FieldPoint;
using ro::sim::SkillArchetype;
using ro::sim::SurvivorsField;
using ro::sim::SurvivorsFoe;
using ro::sim::SurvivorsRun;
using ro::sim::TargetRule;
using ro::test_support::ContentTables;
using ro::test_support::kContentBaphometId;
using ro::test_support::kContentNovaId;
using ro::test_support::kContentNovaIndex;
using ro::test_support::kTinyLunaticId;
using ro::test_support::PillarArena;
using ro::test_support::PlaceFoe;
using ro::test_support::StepStill;
using ro::test_support::TinyStart;

namespace {

constexpr std::int32_t kArenaSideCells = 64;
constexpr std::int32_t kSturdyHp = 1000;
constexpr std::int32_t kBossHp = 50000;
constexpr std::size_t kBaphometIndex = 11;
constexpr std::int32_t kConeDamage = 10;
// L'est, sur les 64 caps comptés depuis le nord dans le sens trigonométrique.
constexpr std::uint8_t kEast = 48;

// Les règles de contenu, silencieuses, où le skill de nova devient `archetype`.
struct SkillContent : ContentTables {
  SkillContent(SkillArchetype archetype, TargetRule target, ro::sim::SkillLevel level) {
    rules.waves = ro::test_support::kQuietWaves;
    rules.events = {};
    rules.bosses = {};
    rules.elites = {};
    rules.plants = {};
    foes[kBaphometIndex].behaviour.traits = 0;
    nova = level;
    skills[kContentNovaIndex].archetype = archetype;
    skills[kContentNovaIndex].target = target;
  }
};

[[nodiscard]] SurvivorsRun StartRun(const ro::sim::SurvivorsRules& rules) {
  auto run = SurvivorsRun::Start(rules, PillarArena(kArenaSideCells), TinyStart(1));
  REQUIRE(run.has_value());
  return std::move(*run);
}

void GiveSkill(SurvivorsField& field) {
  field.hero.skills = {ro::sim::OwnedSkill{
      .skill_id = kContentNovaId, .level = 1, .ready_step = field.step, .phase_step = field.step}};
}

std::uint32_t Place(SurvivorsField& field, std::uint16_t foe_id, FieldPoint offset, bool walks,
                    std::int32_t hp = kSturdyHp) {
  const std::uint32_t serial = PlaceFoe(field, foe_id, field.hero.position + offset);
  SurvivorsFoe& foe = field.foes.back();
  if (!walks) foe.units_per_step = 0;
  foe.hp = hp;
  foe.max_hp = hp;
  return serial;
}

[[nodiscard]] const SurvivorsFoe& FoeOf(const SurvivorsField& field, std::uint32_t serial) {
  const auto found = std::find_if(field.foes.begin(), field.foes.end(),
                                  [&](const SurvivorsFoe& foe) { return foe.serial == serial; });
  REQUIRE(found != field.foes.end());
  return *found;
}

[[nodiscard]] std::size_t CountCasts(const ro::sim::SurvivorsNews& news, CastKind kind) {
  return static_cast<std::size_t>(
      std::count_if(news.casts.begin(), news.casts.end(),
                    [&](const ro::sim::SurvivorsCast& cast) { return cast.kind == kind; }));
}

// Un cône de quatre cases, ouvert d'un huitième de tour de chaque côté.
constexpr ro::sim::SkillLevel kConeLevel{
    .damage = kConeDamage, .cooldown_ms = 10000, .reach_units = Cells(4), .cone_half_steps = 8};

// Trois blocs d'une demi-case, posés à trois cases face à la foule.
constexpr ro::sim::SkillLevel kWallLevel{.cooldown_ms = 60000,
                                         .amount = 3,
                                         .reach_units = Cells(10),
                                         .hit_radius_units = CellTenths(5),
                                         .life_ms = 60000,
                                         .area_radius_units = Cells(2),
                                         .length_units = Cells(3),
                                         .freeze_ms = 1000,
                                         .block_hp = 1000};

}  // namespace

TEST_CASE("Cône survivors : devant le héros, dans son cap, à sa portée", "[sim][survivors]") {
  SkillContent tables(SkillArchetype::kCone, TargetRule::kNearest, kConeLevel);
  SurvivorsRun run = StartRun(tables.rules);
  SurvivorsField& field = run.field_for_tests();
  GiveSkill(field);
  field.hero.facing = kEast;
  const std::uint32_t ahead = Place(field, kTinyLunaticId, {Cells(2), 0}, false);
  const std::uint32_t aside = Place(field, kTinyLunaticId, {Cells(3), Cells(1)}, false);
  const std::uint32_t wide = Place(field, kTinyLunaticId, {Cells(1), Cells(3)}, false);
  const std::uint32_t behind = Place(field, kTinyLunaticId, {-Cells(2), 0}, false);
  const std::uint32_t far = Place(field, kTinyLunaticId, {Cells(7), 0}, false);

  StepStill(run, 1);
  CHECK(CountCasts(run.TakeNews(), CastKind::kConeOn) == 1);
  CHECK(FoeOf(field, ahead).hp < kSturdyHp);
  CHECK(FoeOf(field, aside).hp < kSturdyHp);
  // Plus d'un huitième de tour hors de l'axe, derrière, ou trop loin.
  CHECK(FoeOf(field, wide).hp == kSturdyHp);
  CHECK(FoeOf(field, behind).hp == kSturdyHp);
  CHECK(FoeOf(field, far).hp == kSturdyHp);
}

TEST_CASE("Armure survivors : un monstre blindé retranche sa part du coup", "[sim][survivors]") {
  // Deux Lunatics côte à côte dans le même cône, l'un portant une armure de
  // 50 % : il perd la moitié de ce que perd l'autre.
  constexpr std::int32_t kHalfArmorPct = 50;
  constexpr std::int32_t kWholePct = 100;
  SkillContent tables(SkillArchetype::kCone, TargetRule::kNearest, kConeLevel);
  SurvivorsRun run = StartRun(tables.rules);
  SurvivorsField& field = run.field_for_tests();
  GiveSkill(field);
  field.hero.facing = kEast;
  const std::uint32_t bare = Place(field, kTinyLunaticId, {Cells(2), 0}, false);
  const std::uint32_t armored = Place(field, kTinyLunaticId, {Cells(2), Cells(1)}, false);
  field.foes.back().armor_pct = kHalfArmorPct;

  StepStill(run, 1);
  const std::int32_t bare_loss = kSturdyHp - FoeOf(field, bare).hp;
  const std::int32_t armored_loss = kSturdyHp - FoeOf(field, armored).hp;
  REQUIRE(bare_loss > 1);
  CHECK(armored_loss == bare_loss * (kWholePct - kHalfArmorPct) / kWholePct);
}

TEST_CASE("Cône survivors : sans monstre devant, il attend", "[sim][survivors]") {
  SkillContent tables(SkillArchetype::kCone, TargetRule::kNearest, kConeLevel);
  SurvivorsRun run = StartRun(tables.rules);
  SurvivorsField& field = run.field_for_tests();
  GiveSkill(field);
  field.hero.facing = kEast;
  Place(field, kTinyLunaticId, {-Cells(2), 0}, false);
  StepStill(run, 1);
  CHECK(CountCasts(run.TakeNews(), CastKind::kConeOn) == 0);
  CHECK(field.hero.skills[0].ready_step <= field.step);
}

TEST_CASE("Barrière survivors : des blocs face à la foule, que la horde ne traverse pas",
          "[sim][survivors]") {
  SkillContent tables(SkillArchetype::kBarrier, TargetRule::kDensest, kWallLevel);
  SurvivorsRun run = StartRun(tables.rules);
  SurvivorsField& field = run.field_for_tests();
  GiveSkill(field);
  // La foule, immobile, à sept cases à l'est ; un marcheur part de six.
  for (const std::int32_t row : {-1, 0, 1}) {
    Place(field, kTinyLunaticId, {Cells(7), Cells(row)}, false);
  }
  const std::uint32_t walker = Place(field, kTinyLunaticId, {Cells(6), 0}, true);

  StepStill(run, 1);
  CHECK(CountCasts(run.TakeNews(), CastKind::kBlockOn) == 3);
  REQUIRE(field.zones.size() == 3);
  // Le bloc du milieu, à trois cases à l'est du héros.
  const FieldPoint middle = field.zones[1].from;
  CHECK(middle.x - field.hero.position.x == Cells(3));

  // Le marcheur bute sur la rangée : il ne passe pas, frappe un bloc et gèle.
  StepStill(run, 150);
  const SurvivorsFoe& held = FoeOf(field, walker);
  CHECK(held.position.x - field.hero.position.x > Cells(3));
  CHECK(std::any_of(field.zones.begin(), field.zones.end(), [](const ro::sim::SurvivorsZone& zone) {
    return zone.block_hp < kWallLevel.block_hp;
  }));
  CHECK(field.recap.freezes > 0);
}

TEST_CASE("Barrière survivors : les blocs d'un héros tombé retiennent encore la horde",
          "[sim][survivors]") {
  SkillContent tables(SkillArchetype::kBarrier, TargetRule::kDensest, kWallLevel);
  ro::sim::SurvivorsStart start = TinyStart(1);
  start.allies.push_back(ro::sim::SurvivorsAllyStart{.class_id = start.class_id,
                                                     .first_skill_id = start.first_skill_id});
  auto started = SurvivorsRun::Start(tables.rules, PillarArena(kArenaSideCells), start);
  REQUIRE(started.has_value());
  SurvivorsRun& run = *started;
  SurvivorsField& field = run.field_for_tests();
  // L'allié ne fait que servir de cible : seul le premier héros pose des blocs.
  run.PutHeroOnStage(1);
  field.hero.skills.clear();
  run.PutHeroOnStage(0);
  GiveSkill(field);
  for (const std::int32_t row : {-1, 0, 1}) {
    Place(field, kTinyLunaticId, {Cells(7), Cells(row)}, false);
  }
  const std::uint32_t walker = Place(field, kTinyLunaticId, {Cells(6), 0}, true);
  StepStill(run, 1);
  REQUIRE(field.zones.size() == 3);

  field.hero.down = true;
  StepStill(run, 150);
  REQUIRE(field.zones.size() == 3);
  CHECK(FoeOf(field, walker).position.x - field.hero.position.x > Cells(3));
}

TEST_CASE("Barrière survivors : un boss brise le bloc qu'il touche", "[sim][survivors]") {
  SkillContent tables(SkillArchetype::kBarrier, TargetRule::kDensest, kWallLevel);
  SurvivorsRun run = StartRun(tables.rules);
  SurvivorsField& field = run.field_for_tests();
  GiveSkill(field);
  Place(field, kContentBaphometId, {Cells(5), 0}, true, kBossHp);
  StepStill(run, 1);
  REQUIRE(field.zones.size() == 3);
  StepStill(run, 300);
  CHECK(field.zones.size() < 3);
  CHECK(CountCasts(run.TakeNews(), CastKind::kZoneOff) > 0);
}

TEST_CASE("Cône et barrière survivors : le contrôle des règles", "[sim][survivors]") {
  const auto refused = [](SkillArchetype archetype, TargetRule target, ro::sim::SkillLevel level) {
    SkillContent tables(archetype, target, level);
    return !ro::sim::CheckSurvivorsRules(tables.rules).has_value();
  };
  CHECK_FALSE(refused(SkillArchetype::kCone, TargetRule::kNearest, kConeLevel));
  CHECK_FALSE(refused(SkillArchetype::kBarrier, TargetRule::kDensest, kWallLevel));
  ro::sim::SkillLevel shut = kConeLevel;
  shut.cone_half_steps = 0;
  CHECK(refused(SkillArchetype::kCone, TargetRule::kNearest, shut));
  ro::sim::SkillLevel overturned = kConeLevel;
  overturned.cone_half_steps = ro::sim::kHeadingSteps / 2 + 1;
  CHECK(refused(SkillArchetype::kCone, TargetRule::kNearest, overturned));
  ro::sim::SkillLevel hollow = kWallLevel;
  hollow.block_hp = 0;
  CHECK(refused(SkillArchetype::kBarrier, TargetRule::kDensest, hollow));
  ro::sim::SkillLevel lifeless = kWallLevel;
  lifeless.life_ms = 0;
  CHECK(refused(SkillArchetype::kBarrier, TargetRule::kDensest, lifeless));
}
