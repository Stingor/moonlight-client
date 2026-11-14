#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <span>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/pcg32.h"
#include "ro/sim/survivors_content.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_news.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/survivors_score.h"
#include "ro/sim/survivors_state.h"
#include "ro/sim/walk_map.h"

#include "support/survivors_content_fixture.h"
#include "support/survivors_fixture.h"

using ro::sim::BossMoveKind;
using ro::sim::BossMoveRule;
using ro::sim::Cells;
using ro::sim::DistanceSquared;
using ro::sim::FieldPoint;
using ro::sim::HazardKind;
using ro::sim::kMostBossMoves;
using ro::sim::kSurvivorsStepMs;
using ro::sim::StepsOf;
using ro::sim::SurvivorsField;
using ro::sim::SurvivorsFoe;
using ro::sim::SurvivorsNews;
using ro::sim::SurvivorsOutcome;
using ro::sim::SurvivorsPhase;
using ro::sim::SurvivorsRun;
using ro::sim::WalkMap;
using ro::sim::WarningKind;
using ro::test_support::ContentTables;
using ro::test_support::kContentBaphometId;
using ro::test_support::kContentBaphometJrId;
using ro::test_support::kContentNovaId;
using ro::test_support::kTinyCardId;
using ro::test_support::kTinyLunaticId;
using ro::test_support::kTinyPoringId;
using ro::test_support::PlaceFoe;
using ro::test_support::StepStill;
using ro::test_support::TinyStart;

namespace {

constexpr std::int32_t kArenaSideCells = 64;
constexpr std::int32_t kSturdyHeroHp = 1000000;
// Les index des espèces dans ContentTables::foes.
constexpr std::size_t kLunaticIndex = 0;
constexpr std::size_t kPoringIndex = 1;
constexpr std::size_t kBaphometIndex = 11;
// Un coup prêt un pas après la naissance du boss, au pas 0 : le pas 1.
constexpr std::uint32_t kOneStepMs = kSurvivorsStepMs;
constexpr std::uint32_t kLongPeriodMs = 60000;
// Les PV de Baphomet à la minute 0, et ses seuils de phase.
constexpr std::int32_t kBaphometHp = 50000;
constexpr std::int32_t kSixtyPctHp = 30000;
constexpr std::int32_t kQuarterHp = 12500;

// Les règles de contenu sans vague, événement, élite ni plante, avec
// Baphomet seul au pas 0, sans escorte, sans script et sans contact : un essai
// y pose le script qu'il mesure.
struct BossContent : ContentTables {
  std::array<BossMoveRule, kMostBossMoves> script{};
  BossContent() {
    rules.waves = ro::test_support::kQuietWaves;
    rules.events = {};
    rules.elites = {};
    rules.plants = {};
    bosses[0].at_ms = 0;
    bosses[0].escort_count = 0;
    bosses[0].moves = {};
    foes[kBaphometIndex].touch_damage = 0;
  }
  void Script(std::initializer_list<BossMoveRule> moves) {
    std::copy(moves.begin(), moves.end(), script.begin());
    bosses[0].moves = std::span(script).first(moves.size());
  }
};

[[nodiscard]] WalkMap OpenArena() {
  const auto side = static_cast<std::size_t>(kArenaSideCells);
  return WalkMap(kArenaSideCells, kArenaSideCells, std::vector<std::uint8_t>(side * side, 1U));
}

// Une partie sur une arène ouverte, sans skill, au héros increvable, après son
// premier pas : Baphomet est né.
[[nodiscard]] SurvivorsRun StartRun(const ro::sim::SurvivorsRules& rules) {
  auto run = SurvivorsRun::Start(rules, OpenArena(), TinyStart(1));
  REQUIRE(run.has_value());
  SurvivorsField& field = run->field_for_tests();
  field.hero.skills.clear();
  field.hero.hp = kSturdyHeroHp;
  run->Step({});
  return std::move(*run);
}

[[nodiscard]] SurvivorsFoe* BossOf(SurvivorsField& field) {
  for (SurvivorsFoe& foe : field.foes) {
    if (foe.foe_id == kContentBaphometId) return &foe;
  }
  return nullptr;
}

// Baphomet posé à `offset` du héros.
SurvivorsFoe& PlaceBoss(SurvivorsRun& run, FieldPoint offset) {
  SurvivorsField& field = run.field_for_tests();
  SurvivorsFoe* boss = BossOf(field);
  REQUIRE(boss != nullptr);
  boss->position = field.hero.position + offset;
  boss->previous_position = boss->position;
  return *boss;
}

[[nodiscard]] std::size_t CountOf(const SurvivorsField& field, std::uint16_t foe_id) {
  return static_cast<std::size_t>(
      std::count_if(field.foes.begin(), field.foes.end(),
                    [&](const SurvivorsFoe& foe) { return foe.foe_id == foe_id; }));
}

[[nodiscard]] std::vector<BossMoveKind> MovesOf(const SurvivorsNews& news) {
  std::vector<BossMoveKind> kinds;
  for (const auto& move : news.boss_moves) kinds.push_back(move.kind);
  return kinds;
}

[[nodiscard]] std::int64_t Distance(FieldPoint a, FieldPoint b) {
  return static_cast<std::int64_t>(
      ro::sim::IntegerSqrt(static_cast<std::uint64_t>(DistanceSquared(a, b))));
}

// Un coup par phase de Baphomet, prêt au pas 1 : un hurlement de 100 à 60 %,
// une invocation de 60 à 25 %, un séisme sous 25 %.
[[nodiscard]] std::vector<BossMoveKind> MovesAtHp(std::int32_t hp) {
  BossContent tables;
  tables.Script({
      {.kind = BossMoveKind::kHowl,
       .from_hp_pct = 100,
       .to_hp_pct = 60,
       .first_ms = kOneStepMs,
       .period_ms = kLongPeriodMs,
       .effect_ms = 1000,
       .foe_id = kTinyLunaticId,
       .boost_pct = 10},
      {.kind = BossMoveKind::kSummon,
       .from_hp_pct = 60,
       .to_hp_pct = 25,
       .first_ms = kOneStepMs,
       .period_ms = kLongPeriodMs,
       .foe_id = kContentBaphometJrId,
       .count = 1},
      {.kind = BossMoveKind::kQuake,
       .from_hp_pct = 25,
       .to_hp_pct = 0,
       .first_ms = kOneStepMs,
       .period_ms = kLongPeriodMs,
       .count = 1,
       .radius_units = Cells(1),
       .damage = 1},
  });
  SurvivorsRun run = StartRun(tables.rules);
  SurvivorsFoe* boss = BossOf(run.field_for_tests());
  REQUIRE(boss != nullptr);
  REQUIRE(boss->max_hp == kBaphometHp);
  boss->hp = hp;
  (void)run.TakeNews();
  run.Step({});
  return MovesOf(run.TakeNews());
}

}  // namespace

TEST_CASE("Boss survivors : les phases tombent aux seuils exacts, bornes comprises",
          "[sim][survivors]") {
  using Kinds = std::vector<BossMoveKind>;
  CHECK(MovesAtHp(kBaphometHp) == Kinds{BossMoveKind::kHowl});
  // Un PV au-dessus de 60 % reste en phase 1 ; 60 % pile passe en phase 2 (la
  // borne haute d'un coup est comprise, la basse exclue).
  CHECK(MovesAtHp(kSixtyPctHp + 1) == Kinds{BossMoveKind::kHowl});
  CHECK(MovesAtHp(kSixtyPctHp) == Kinds{BossMoveKind::kSummon});
  // Un PV au-dessus de 25 % reste en phase 2 ; 25 % pile passe en phase 3.
  CHECK(MovesAtHp(kQuarterHp + 1) == Kinds{BossMoveKind::kSummon});
  CHECK(MovesAtHp(kQuarterHp) == Kinds{BossMoveKind::kQuake});
  CHECK(MovesAtHp(1) == Kinds{BossMoveKind::kQuake});
}

TEST_CASE("Boss survivors : l'escorte naît à l'arrivée, sous le plafond, une fois",
          "[sim][survivors]") {
  BossContent tables;
  tables.bosses[0].escort_foe_id = kContentBaphometJrId;
  tables.bosses[0].escort_count = 2;
  {
    SurvivorsRun run = StartRun(tables.rules);
    SurvivorsField& field = run.field_for_tests();
    REQUIRE(CountOf(field, kContentBaphometJrId) == 2);
    const SurvivorsFoe* boss = BossOf(field);
    REQUIRE(boss != nullptr);
    // En cercle à deux cases du boss, ramenés au centre d'une case.
    for (const SurvivorsFoe& foe : field.foes) {
      if (foe.foe_id == kContentBaphometJrId) {
        CHECK(Distance(foe.position, boss->position) <= Cells(3));
      }
    }
  }
  // Le boss compte dans le plafond : une seule place, une seule escorte, et
  // une escorte tuée n'est jamais rattrapée.
  tables.rules.caps.most_foes = 2;
  SurvivorsRun run = StartRun(tables.rules);
  SurvivorsField& field = run.field_for_tests();
  REQUIRE(CountOf(field, kContentBaphometJrId) == 1);
  for (SurvivorsFoe& foe : field.foes) {
    if (foe.foe_id == kContentBaphometJrId) foe.hp = 0;
  }
  StepStill(run, 10);
  CHECK(CountOf(run.field(), kContentBaphometJrId) == 0);
}

TEST_CASE("Boss survivors : une invocation respecte le plafond et réessaie au pas suivant",
          "[sim][survivors]") {
  BossContent tables;
  constexpr std::uint32_t kSummonPeriodMs = 2000;
  tables.Script({{.kind = BossMoveKind::kSummon,
                  .first_ms = kOneStepMs,
                  .period_ms = kSummonPeriodMs,
                  .foe_id = kContentBaphometJrId,
                  .count = 4,
                  .radius_units = Cells(2)}});
  tables.rules.caps.most_foes = 3;
  SurvivorsRun run = StartRun(tables.rules);
  (void)run.TakeNews();
  run.Step({});
  // Quatre demandés, deux places à côté du boss.
  CHECK(MovesOf(run.TakeNews()) == std::vector{BossMoveKind::kSummon});
  REQUIRE(CountOf(run.field(), kContentBaphometJrId) == 2);
  const SurvivorsFoe* boss = BossOf(run.field_for_tests());
  REQUIRE(boss != nullptr);
  for (const SurvivorsFoe& foe : run.field().foes) {
    if (foe.foe_id == kContentBaphometJrId) {
      CHECK(Distance(foe.position, boss->position) <= Cells(3));
    }
  }
  // Au plafond, à l'heure : rien, et le coup reste prêt.
  const std::uint32_t due_step = 1 + StepsOf(kSummonPeriodMs);
  StepStill(run, due_step - run.field().step + 5);
  CHECK(run.TakeNews().boss_moves.empty());
  REQUIRE(run.field().boss_tracks.size() == 1);
  CHECK(run.field().boss_tracks[0].ready_step[0] == due_step);
  // Une place libérée : l'invocation part au pas qui suit la chute.
  for (SurvivorsFoe& foe : run.field_for_tests().foes) {
    if (foe.foe_id == kContentBaphometJrId) {
      foe.hp = 0;
      break;
    }
  }
  run.Step({});
  CHECK(run.TakeNews().boss_moves.empty());
  run.Step({});
  CHECK(MovesOf(run.TakeNews()) == std::vector{BossMoveKind::kSummon});
  CHECK(CountOf(run.field(), kContentBaphometJrId) == 2);
  CHECK(run.field().boss_tracks[0].ready_step[0] ==
        run.field().step - 1 + StepsOf(kSummonPeriodMs));
}

TEST_CASE("Boss survivors : le séisme annonce ses cercles et ne blesse qu'au pas armé",
          "[sim][survivors]") {
  BossContent tables;
  constexpr std::uint32_t kTelegraphMs = 1000;
  constexpr std::int32_t kQuakeDamage = 40;
  constexpr std::int32_t kScatter = Cells(3);
  constexpr std::uint8_t kCircles = 3;
  tables.Script({{.kind = BossMoveKind::kQuake,
                  .first_ms = kOneStepMs,
                  .period_ms = kLongPeriodMs,
                  .telegraph_ms = kTelegraphMs,
                  .count = kCircles,
                  .reach_units = kScatter,
                  .radius_units = ro::sim::CellTenths(25),
                  .damage = kQuakeDamage}});
  SurvivorsRun run = StartRun(tables.rules);
  SurvivorsField& field = run.field_for_tests();
  // Le boss au loin : rien d'autre ne touche le héros.
  (void)PlaceBoss(run, {Cells(20), 0});
  ro::core::Pcg32 draws = field.combat_random;
  const FieldPoint hero = field.hero.position;
  (void)run.TakeNews();
  run.Step({});
  const SurvivorsNews news = run.TakeNews();
  CHECK(MovesOf(news) == std::vector{BossMoveKind::kQuake});
  REQUIRE(news.warnings.size() == kCircles);
  const std::uint32_t strike_step = 1 + StepsOf(kTelegraphMs);
  // Le premier sur le héros, les autres décalés de deux Between chacun, et
  // pas un tirage de plus.
  std::vector<FieldPoint> centers{hero};
  for (std::uint8_t circle = 1; circle < kCircles; ++circle) {
    const std::int32_t dx = draws.Between(-kScatter, kScatter);
    const std::int32_t dy = draws.Between(-kScatter, kScatter);
    centers.push_back(hero + FieldPoint{dx, dy});
  }
  CHECK(field.combat_random == draws);
  std::int32_t expected_hurt = 0;
  for (std::size_t circle = 0; circle < kCircles; ++circle) {
    CHECK(news.warnings[circle].kind == WarningKind::kQuake);
    CHECK(news.warnings[circle].to == centers[circle]);
    CHECK(news.warnings[circle].strike_step == strike_step);
    CHECK(field.hazards[circle].kind == HazardKind::kQuake);
    const std::int64_t reach =
        std::int64_t{news.warnings[circle].radius_units} + tables.rules.tuning.hero_radius_units;
    if (DistanceSquared(hero, centers[circle]) < reach * reach) expected_hurt += kQuakeDamage;
  }
  REQUIRE(expected_hurt >= kQuakeDamage);
  // Rien avant le pas armé, tout à ce pas, plus rien ensuite.
  StepStill(run, strike_step - field.step);
  CHECK(field.hero.hp == kSturdyHeroHp);
  run.Step({});
  CHECK(field.hero.hp == kSturdyHeroHp - expected_hurt);
  StepStill(run, StepsOf(kTelegraphMs));
  CHECK(field.hero.hp == kSturdyHeroHp - expected_hurt);
  CHECK(field.hazards.empty());
}

TEST_CASE("Boss survivors : la charge s'annonce, attend immobile, puis file", "[sim][survivors]") {
  BossContent tables;
  constexpr std::uint32_t kTelegraphMs = 800;
  constexpr std::uint32_t kDashMs = 400;
  tables.Script({{.kind = BossMoveKind::kCharge,
                  .first_ms = kOneStepMs,
                  .period_ms = 8000,
                  .telegraph_ms = kTelegraphMs,
                  .effect_ms = kDashMs,
                  .reach_units = Cells(6)}});
  SurvivorsRun run = StartRun(tables.rules);
  SurvivorsField& field = run.field_for_tests();
  const FieldPoint start = PlaceBoss(run, {Cells(10), 0}).position;
  (void)run.TakeNews();
  run.Step({});
  const SurvivorsNews news = run.TakeNews();
  REQUIRE(news.warnings.size() == 1);
  const auto& warning = news.warnings[0];
  CHECK(warning.kind == WarningKind::kCharge);
  CHECK(warning.from == start);
  CHECK(warning.strike_step == 1 + StepsOf(kTelegraphMs));
  // Six cases droit vers le héros, à l'ouest, réparties sur les pas de la
  // charge.
  const std::int32_t per_step = Cells(6) / static_cast<std::int32_t>(StepsOf(kDashMs));
  CHECK(warning.to ==
        start + FieldPoint{-per_step * static_cast<std::int32_t>(StepsOf(kDashMs)), 0});
  StepStill(run, warning.strike_step - field.step);
  CHECK(BossOf(field)->position == start);
  run.Step({});
  CHECK(BossOf(field)->position == start + FieldPoint{-per_step, 0});
  StepStill(run, StepsOf(kDashMs) - 1);
  CHECK(BossOf(field)->position == warning.to);
  // La charge finie, le boss reprend la poursuite.
  run.Step({});
  CHECK(Distance(BossOf(field)->position, field.hero.position) <
        Distance(warning.to, field.hero.position));
  CHECK_FALSE(BossOf(field)->scripted);
}

TEST_CASE("Boss survivors : le bond s'annonce et se pose à son pas", "[sim][survivors]") {
  BossContent tables;
  constexpr std::uint32_t kTelegraphMs = 400;
  tables.Script({{.kind = BossMoveKind::kLeap,
                  .first_ms = kOneStepMs,
                  .period_ms = 5000,
                  .telegraph_ms = kTelegraphMs,
                  .reach_units = Cells(4)}});
  SurvivorsRun run = StartRun(tables.rules);
  SurvivorsField& field = run.field_for_tests();
  const FieldPoint start = PlaceBoss(run, {Cells(10), 0}).position;
  (void)run.TakeNews();
  run.Step({});
  const SurvivorsNews news = run.TakeNews();
  REQUIRE(news.warnings.size() == 1);
  CHECK(news.warnings[0].kind == WarningKind::kLeap);
  CHECK(news.warnings[0].to == start + FieldPoint{-Cells(4), 0});
  StepStill(run, news.warnings[0].strike_step - field.step);
  CHECK(BossOf(field)->position == start);
  run.Step({});
  CHECK(BossOf(field)->position == start + FieldPoint{-Cells(4), 0});
}

TEST_CASE("Boss survivors : le hurlement ne presse que l'espèce visée", "[sim][survivors]") {
  BossContent tables;
  constexpr std::uint32_t kHowlMs = 4000;
  constexpr std::int32_t kBoostPct = 30;
  tables.Script({{.kind = BossMoveKind::kHowl,
                  .first_ms = kOneStepMs,
                  .period_ms = 10000,
                  .effect_ms = kHowlMs,
                  .foe_id = kTinyLunaticId,
                  .boost_pct = kBoostPct}});
  SurvivorsRun run = StartRun(tables.rules);
  SurvivorsField& field = run.field_for_tests();
  (void)PlaceBoss(run, {Cells(20), 0});
  const FieldPoint hero = field.hero.position;
  const std::uint32_t lunatic = PlaceFoe(field, kTinyLunaticId, hero + FieldPoint{0, Cells(2)});
  const std::uint32_t poring = PlaceFoe(field, kTinyPoringId, hero - FieldPoint{0, Cells(2)});
  run.Step({});
  const auto by_serial = [&](std::uint32_t serial) -> const SurvivorsFoe& {
    return *std::find_if(field.foes.begin(), field.foes.end(),
                         [&](const SurvivorsFoe& foe) { return foe.serial == serial; });
  };
  const SurvivorsFoe& pressed = by_serial(lunatic);
  const SurvivorsFoe& calm = by_serial(poring);
  CHECK(pressed.boost_until_step == 1 + StepsOf(kHowlMs));
  CHECK(pressed.boost_pct == kBoostPct);
  CHECK(calm.boost_until_step == 0);
  CHECK(BossOf(field)->boost_until_step == 0);
  // Pressé dès ce pas : sa marche majorée de 30 %, celle du Poring intacte.
  const std::int32_t lunatic_pace = pressed.units_per_step;
  CHECK(pressed.previous_position.y - pressed.position.y == lunatic_pace * (100 + kBoostPct) / 100);
  CHECK(calm.position.y - calm.previous_position.y == calm.units_per_step);
  CHECK(tables.foes[kLunaticIndex].foe_id == kTinyLunaticId);
  CHECK(tables.foes[kPoringIndex].foe_id == kTinyPoringId);
}

TEST_CASE("Boss survivors : touché, le boss fuit le temps de son coup, puis revient",
          "[sim][survivors]") {
  BossContent tables;
  constexpr std::uint32_t kFleeMs = 1000;
  constexpr std::uint32_t kFleePeriodMs = 3000;
  tables.Script({{.kind = BossMoveKind::kFlee, .period_ms = kFleePeriodMs, .effect_ms = kFleeMs}});
  SurvivorsRun run = StartRun(tables.rules);
  SurvivorsField& field = run.field_for_tests();
  (void)PlaceBoss(run, {Cells(6), 0});
  const FieldPoint hero = field.hero.position;
  // Intact, il approche.
  std::int64_t distance = Distance(BossOf(field)->position, hero);
  run.Step({});
  CHECK(Distance(BossOf(field)->position, hero) < distance);
  CHECK(run.TakeNews().boss_moves.empty());
  // Touché : il s'éloigne à chaque pas de sa fuite, et un coup de plus ne la
  // relance pas.
  BossOf(field)->hp -= 1;
  for (std::uint32_t step = 0; step < StepsOf(kFleeMs); ++step) {
    distance = Distance(BossOf(field)->position, hero);
    if (step == 1) BossOf(field)->hp -= 1;
    run.Step({});
    CHECK(Distance(BossOf(field)->position, hero) > distance);
  }
  CHECK(MovesOf(run.TakeNews()) == std::vector{BossMoveKind::kFlee});
  // Sa fuite finie, il revient ; touché pendant sa recharge, il ne fuit pas.
  distance = Distance(BossOf(field)->position, hero);
  BossOf(field)->hp -= 1;
  run.Step({});
  CHECK(Distance(BossOf(field)->position, hero) < distance);
  CHECK(run.TakeNews().boss_moves.empty());
}

TEST_CASE("Boss survivors : Baphomet n'est ni gelé ni repoussé", "[sim][survivors]") {
  BossContent tables;
  tables.nova.knockback_units = Cells(1);
  tables.foes[kPoringIndex].hp = kBaphometHp;
  SurvivorsRun run = StartRun(tables.rules);
  SurvivorsField& field = run.field_for_tests();
  SurvivorsFoe& boss = PlaceBoss(run, {Cells(2), 0});
  const std::uint32_t boss_serial = boss.serial;
  const std::uint32_t poring =
      PlaceFoe(field, kTinyPoringId, field.hero.position - FieldPoint{Cells(2), 0});
  field.hero.skills.push_back({.skill_id = kContentNovaId, .level = 1});
  constexpr std::uint32_t kMostSteps = 100;
  bool landed = false;
  for (std::uint32_t step = 0; step < kMostSteps && !landed; ++step) {
    run.Step({});
    for (const auto& hit : run.TakeNews().hits) {
      if (hit.foe_serial != boss_serial) continue;
      landed = true;
      CHECK_FALSE(hit.froze);
      CHECK_FALSE(hit.knocked);
    }
  }
  REQUIRE(landed);
  CHECK(BossOf(field)->freeze_until_step == 0);
  CHECK(BossOf(field)->knock_left_units == 0);
  // Le Poring, de l'autre côté, gèle sous la même nova.
  const auto calm = std::find_if(field.foes.begin(), field.foes.end(),
                                 [&](const SurvivorsFoe& foe) { return foe.serial == poring; });
  REQUIRE(calm != field.foes.end());
  CHECK(calm->freeze_until_step > field.step);
}

TEST_CASE("Boss survivors : la chute du boss final est la victoire", "[sim][survivors]") {
  BossContent tables;
  SurvivorsRun run = StartRun(tables.rules);
  BossOf(run.field_for_tests())->hp = 0;
  run.Step({});
  const auto& tally = run.tally();
  CHECK(run.field().phase == SurvivorsPhase::kOver);
  CHECK(tally.outcome == SurvivorsOutcome::kVictory);
  CHECK(tally.boss_kills == 1);
  constexpr std::uint32_t kBaphometPoints = 81 * ro::sim::kBossKillFactor;
  CHECK(tally.kill_points == kBaphometPoints);
  CHECK(ro::sim::SurvivorsScore(tally) == std::uint64_t{kBaphometPoints} +
                                              ro::sim::kScorePerLevel * tally.level +
                                              ro::sim::kVictoryScore);
  // Un boss qui n'est pas final : la partie continue.
  tables.bosses[0].final = false;
  SurvivorsRun plain = StartRun(tables.rules);
  BossOf(plain.field_for_tests())->hp = 0;
  plain.Step({});
  CHECK(plain.field().phase == SurvivorsPhase::kRunning);
  CHECK(plain.tally().boss_kills == 1);
}

TEST_CASE("Boss survivors : la carte du boss final va droit au héros", "[sim][survivors]") {
  // La partie s'arrête à sa chute : posée au sol, sa carte serait perdue.
  BossContent tables;
  tables.foes[kBaphometIndex].card_item_id = kTinyCardId;
  SurvivorsRun run = StartRun(tables.rules);
  SurvivorsField& field = run.field_for_tests();
  BossOf(field)->hp = 0;
  run.Step({});
  REQUIRE(run.tally().outcome == SurvivorsOutcome::kVictory);
  CHECK(field.recap.cards_found == std::vector<std::uint16_t>{kTinyCardId});
  CHECK(std::ranges::none_of(field.pickups, [](const ro::sim::SurvivorsPickup& pickup) {
    return pickup.kind == ro::sim::PickupKind::kCard;
  }));
  const SurvivorsNews news = run.TakeNews();
  CHECK(news.cards_dropped.size() == 1U);
  CHECK(news.cards_found.size() == 1U);
}

TEST_CASE("Boss survivors : une victoire et une chute du même pas donnent la victoire",
          "[sim][survivors]") {
  BossContent tables;
  // Un héros à un PV, une menace armée sous lui et un Lunatic à son contact :
  // ce pas le tue, sauf si le boss final tombe d'abord.
  const auto doomed = [&](bool boss_falls) {
    SurvivorsRun run = StartRun(tables.rules);
    SurvivorsField& field = run.field_for_tests();
    (void)PlaceBoss(run, {Cells(20), 0});
    field.hero.hp = 1;
    (void)PlaceFoe(field, kTinyLunaticId, field.hero.position);
    field.hazards.push_back({.kind = HazardKind::kQuake,
                             .center = field.hero.position,
                             .radius_units = Cells(1),
                             .damage = 40,
                             .armed_step = field.step,
                             .until_step = field.step + 1});
    if (boss_falls) BossOf(field)->hp = 0;
    run.Step({});
    return std::pair{run.tally().outcome, field.hero.hp};
  };
  CHECK(doomed(false) == std::pair{SurvivorsOutcome::kFell, 0});
  CHECK(doomed(true) == std::pair{SurvivorsOutcome::kVictory, 1});
}

TEST_CASE("Boss survivors : avec un boss final, la partie va jusqu'au dépassement",
          "[sim][survivors]") {
  BossContent tables;
  constexpr std::uint32_t kRunMs = 1000;
  constexpr std::uint32_t kOvertimeMs = 1000;
  tables.rules.run_ms = kRunMs;
  tables.rules.tuning.final_overtime_ms = kOvertimeMs;
  {
    SurvivorsRun run = StartRun(tables.rules);
    StepStill(run, StepsOf(kRunMs) - run.field().step);
    CHECK(run.field().phase == SurvivorsPhase::kRunning);
    StepStill(run, StepsOf(kOvertimeMs) - 1);
    CHECK(run.field().phase == SurvivorsPhase::kRunning);
    run.Step({});
    CHECK(run.field().phase == SurvivorsPhase::kOver);
    CHECK(run.tally().outcome == SurvivorsOutcome::kSurvived);
    CHECK(run.field().step == StepsOf(kRunMs + kOvertimeMs));
  }
  // Sans boss final, la partie finit en kSurvived à run_ms.
  tables.bosses[0].final = false;
  SurvivorsRun run = StartRun(tables.rules);
  StepStill(run, StepsOf(kRunMs) - run.field().step);
  CHECK(run.field().phase == SurvivorsPhase::kOver);
  CHECK(run.tally().outcome == SurvivorsOutcome::kSurvived);
}
