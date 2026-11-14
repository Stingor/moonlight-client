#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/survivors_content.h"
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
using ro::sim::StepsOf;
using ro::sim::SurvivorsElement;
using ro::sim::SurvivorsField;
using ro::sim::SurvivorsFoe;
using ro::sim::SurvivorsRun;
using ro::sim::SurvivorsStart;
using ro::sim::SurvivorsStat;
using ro::sim::TurnVector;
using ro::test_support::ContentTables;
using ro::test_support::kContentBaphometId;
using ro::test_support::kContentBlastId;
using ro::test_support::kContentBlastIndex;
using ro::test_support::kContentBoltId;
using ro::test_support::kContentLinearId;
using ro::test_support::kContentNovaId;
using ro::test_support::kContentPupaId;
using ro::test_support::kContentRingId;
using ro::test_support::kContentSavageId;
using ro::test_support::kContentWallId;
using ro::test_support::kTinyLunaticId;
using ro::test_support::PillarArena;
using ro::test_support::PlaceFoe;
using ro::test_support::SetBonus;
using ro::test_support::StepStill;
using ro::test_support::TinyStart;

namespace {

constexpr std::int32_t kArenaSideCells = 64;
constexpr std::int32_t kSturdyHp = 1000;
// La majoration de dégâts d'un héros de niveau 1 des règles minuscules.
constexpr std::int64_t kLevelOneBonusPct = 1;
constexpr std::int64_t kWholePct = 100;
constexpr std::size_t kPupaIndex = 2;
constexpr std::size_t kBaphometIndex = 11;

// Les règles de contenu, silencieuses : ni vague, ni événement, ni boss, ni
// élite, ni plante. Un essai y pose ses monstres et son skill lui-même.
struct QuietContent : ContentTables {
  QuietContent() {
    rules.waves = ro::test_support::kQuietWaves;
    rules.events = {};
    rules.bosses = {};
    rules.elites = {};
    rules.plants = {};
  }
};

// Les dégâts qu'un héros de niveau 1 inflige pour `base`, à `efficiency_pct`.
[[nodiscard]] std::int32_t Expected(std::int64_t base, std::int64_t efficiency_pct = kWholePct) {
  const std::int64_t scaled = base * (kWholePct + kLevelOneBonusPct) / kWholePct;
  return static_cast<std::int32_t>(std::max<std::int64_t>(scaled * efficiency_pct / kWholePct, 1));
}

[[nodiscard]] SurvivorsRun StartRun(const ro::sim::SurvivorsRules& rules,
                                    SurvivorsStart start = TinyStart(1)) {
  auto run = SurvivorsRun::Start(rules, PillarArena(kArenaSideCells), start);
  REQUIRE(run.has_value());
  return std::move(*run);
}

// Le héros ne garde que le skill `skill_id`, au niveau 1, prêt à ce pas.
void Give(SurvivorsField& field, std::uint16_t skill_id) {
  field.hero.skills = {ro::sim::OwnedSkill{
      .skill_id = skill_id, .level = 1, .ready_step = field.step, .phase_step = field.step}};
}

// Un monstre immobile de `hp` PV, à `offset` du héros.
std::uint32_t PlaceStill(SurvivorsField& field, std::uint16_t foe_id, FieldPoint offset,
                         std::int32_t hp = kSturdyHp) {
  const std::uint32_t serial = PlaceFoe(field, foe_id, field.hero.position + offset);
  SurvivorsFoe& foe = field.foes.back();
  foe.units_per_step = 0;
  foe.hp = hp;
  foe.max_hp = hp;
  return serial;
}

[[nodiscard]] FieldPoint East(std::int32_t units) { return FieldPoint{units, 0}; }
[[nodiscard]] FieldPoint North(std::int32_t units) { return FieldPoint{0, units}; }

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

[[nodiscard]] std::int64_t Dot(FieldPoint a, FieldPoint b) {
  return std::int64_t{a.x} * b.x + std::int64_t{a.y} * b.y;
}

}  // namespace

// ── L'éclat ─────────────────────────────────────────────────────────────────

TEST_CASE("Archétypes survivors : l'éclat frappe son centre et son pourtour, sans promettre",
          "[sim][survivors]") {
  QuietContent tables;
  tables.skills[kContentBlastIndex].element = SurvivorsElement::kNeutral;
  SECTION("le centre à plein, le pourtour à splash_pct, au-delà rien") {
    SurvivorsRun run = StartRun(tables.rules);
    SurvivorsField& field = run.field_for_tests();
    Give(field, kContentBlastId);
    const std::uint32_t center = PlaceStill(field, kTinyLunaticId, East(Cells(4)));
    const std::uint32_t rim =
        PlaceStill(field, kTinyLunaticId, East(Cells(4)) + North(CellTenths(10)));
    const std::uint32_t outside =
        PlaceStill(field, kTinyLunaticId, East(Cells(4)) + North(Cells(3)));

    StepStill(run);
    REQUIRE(field.blasts.size() == 1);
    // Le centre et son voisin comptent deux monstres chacun : le premier né.
    CHECK(field.blasts[0].center_serial == center);
    CHECK(field.blasts[0].land_step == 1U);
    // Une aire ne promet rien.
    for (const SurvivorsFoe& foe : field.foes) CHECK(foe.pending_damage == 0);
    for (const SurvivorsFoe& foe : field.foes) CHECK(foe.hp == kSturdyHp);

    StepStill(run);
    CHECK(field.blasts.empty());
    CHECK(FoeOf(field, center).hp == kSturdyHp - Expected(24));
    CHECK(FoeOf(field, rim).hp == kSturdyHp - Expected(24 * 60 / 100));
    CHECK(FoeOf(field, outside).hp == kSturdyHp);
    const ro::sim::SurvivorsNews news = run.TakeNews();
    CHECK(CountCasts(news, CastKind::kBlast) == 1);
    CHECK(news.hits.size() == 2);
  }
  SECTION("un pourtour à 0 % ne touche que le centre") {
    tables.blast.splash_pct = 0;
    SurvivorsRun run = StartRun(tables.rules);
    SurvivorsField& field = run.field_for_tests();
    Give(field, kContentBlastId);
    const std::uint32_t center = PlaceStill(field, kTinyLunaticId, East(Cells(4)));
    const std::uint32_t rim =
        PlaceStill(field, kTinyLunaticId, East(Cells(4)) + North(CellTenths(10)));
    StepStill(run, 2);
    CHECK(FoeOf(field, center).hp == kSturdyHp - Expected(24));
    CHECK(FoeOf(field, rim).hp == kSturdyHp);
  }
  SECTION("l'aire du héros élargit l'éclat") {
    for (const std::int32_t area_pct : {0, 100}) {
      SurvivorsStart start = TinyStart(1);
      SetBonus(start, SurvivorsStat::kAreaPct, area_pct);
      SurvivorsRun run = StartRun(tables.rules, start);
      SurvivorsField& field = run.field_for_tests();
      Give(field, kContentBlastId);
      PlaceStill(field, kTinyLunaticId, East(Cells(4)));
      // À deux cases : hors de 1,5 case plus le rayon d'un petit, dans 3 cases.
      const std::uint32_t far = PlaceStill(field, kTinyLunaticId, East(Cells(4)) + North(Cells(2)));
      StepStill(run, 2);
      CHECK((FoeOf(field, far).hp < kSturdyHp) == (area_pct > 0));
    }
  }
}

TEST_CASE("Archétypes survivors : le plus dense, contre la force brute", "[sim][survivors]") {
  QuietContent tables;
  tables.blast.amount = 3;
  SurvivorsRun run = StartRun(tables.rules);
  SurvivorsField& field = run.field_for_tests();
  Give(field, kContentBlastId);
  // Des grappes de tailles diverses, en dixièmes de case du héros.
  constexpr std::array<std::array<std::int32_t, 2>, 14> kOffsetsTenths{{
      {30, 0},
      {32, 4},
      {35, -3},
      {-40, 10},
      {-42, 14},
      {-38, 8},
      {-41, 9},
      {0, 50},
      {5, 52},
      {0, -45},
      {60, 30},
      {-20, -60},
      {25, 25},
      {27, 22},
  }};
  for (const auto& [x, y] : kOffsetsTenths) {
    PlaceStill(field, kTinyLunaticId, FieldPoint{CellTenths(x), CellTenths(y)});
  }

  // La force brute : pour chaque monstre à portée, le compte des monstres à
  // moins du rayon d'éclat ; le plus grand, puis le premier né.
  const std::int64_t radius = tables.blast.area_radius_units;
  const std::int64_t reach = tables.blast.reach_units;
  struct Ranked {
    std::uint32_t serial = 0;
    std::uint32_t crowd = 0;
  };
  std::vector<Ranked> ranked;
  for (const SurvivorsFoe& candidate : field.foes) {
    if (ro::sim::DistanceSquared(candidate.position, field.hero.position) > reach * reach) continue;
    std::uint32_t crowd = 0;
    for (const SurvivorsFoe& other : field.foes) {
      if (ro::sim::DistanceSquared(candidate.position, other.position) < radius * radius) ++crowd;
    }
    ranked.push_back({candidate.serial, crowd});
  }
  std::stable_sort(ranked.begin(), ranked.end(),
                   [](const Ranked& a, const Ranked& b) { return a.crowd > b.crowd; });
  REQUIRE(ranked.size() >= 3);
  REQUIRE(ranked[0].crowd == 4U);

  StepStill(run);
  REQUIRE(field.blasts.size() == 3);
  for (std::size_t blast = 0; blast < 3; ++blast) {
    CHECK(field.blasts[blast].center_serial == ranked[blast].serial);
    CHECK(field.blasts[blast].land_step == 1U + static_cast<std::uint32_t>(blast) * StepsOf(150));
  }
}

TEST_CASE("Archétypes survivors : deux grappes égales, la première née l'emporte",
          "[sim][survivors]") {
  QuietContent tables;
  SurvivorsRun run = StartRun(tables.rules);
  SurvivorsField& field = run.field_for_tests();
  Give(field, kContentBlastId);
  const std::uint32_t first = PlaceStill(field, kTinyLunaticId, East(Cells(3)));
  PlaceStill(field, kTinyLunaticId, East(Cells(3)) + North(CellTenths(5)));
  const std::uint32_t second = PlaceStill(field, kTinyLunaticId, East(-Cells(3)));
  PlaceStill(field, kTinyLunaticId, East(-Cells(3)) + North(CellTenths(5)));
  StepStill(run);
  REQUIRE(field.blasts.size() == 1);
  CHECK(field.blasts[0].center_serial == first);
  CHECK(field.blasts[0].center_serial != second);
}

// ── La nova ─────────────────────────────────────────────────────────────────

TEST_CASE("Archétypes survivors : la nova frappe autour du héros et gèle", "[sim][survivors]") {
  QuietContent tables;
  SurvivorsRun run = StartRun(tables.rules);
  SurvivorsField& field = run.field_for_tests();
  Give(field, kContentNovaId);
  SECTION("sans monstre dans l'aire, elle attend") {
    PlaceStill(field, kTinyLunaticId, East(Cells(6)));
    StepStill(run, 3);
    CHECK(field.blasts.empty());
    CHECK(field.hero.skills[0].ready_step == 0U);
  }
  SECTION("dans l'aire, frappé et gelé au pas suivant") {
    const std::uint32_t near = PlaceStill(field, kTinyLunaticId, East(Cells(3)));
    const std::uint32_t far = PlaceStill(field, kTinyLunaticId, East(Cells(6)));
    StepStill(run);
    REQUIRE(field.blasts.size() == 1);
    CHECK(field.blasts[0].center == field.hero.position);
    CHECK(FoeOf(field, near).hp == kSturdyHp);
    StepStill(run);
    // L'eau contre le neutre : 100 %.
    CHECK(FoeOf(field, near).hp == kSturdyHp - Expected(40));
    CHECK(FoeOf(field, near).freeze_until_step == 1U + StepsOf(2500));
    CHECK(FoeOf(field, far).hp == kSturdyHp);
    CHECK(FoeOf(field, far).freeze_until_step == 0U);
    const ro::sim::SurvivorsNews news = run.TakeNews();
    CHECK(CountCasts(news, CastKind::kNova) == 1);
    REQUIRE(news.hits.size() == 1);
    CHECK(news.hits[0].froze);
    CHECK_FALSE(news.hits[0].knocked);
  }
}

TEST_CASE("Archétypes survivors : le gel épargne les boss et les insensibles", "[sim][survivors]") {
  QuietContent tables;
  // Le boss sans aucun trait : son rang seul le protège.
  tables.foes[kBaphometIndex].behaviour.traits = 0;
  tables.foes[kPupaIndex].behaviour.traits = ro::sim::kFoeFreezeImmune;
  SurvivorsRun run = StartRun(tables.rules);
  SurvivorsField& field = run.field_for_tests();
  Give(field, kContentNovaId);
  const std::uint32_t boss = PlaceStill(field, kContentBaphometId, East(Cells(3)), 50000);
  const std::uint32_t immune = PlaceStill(field, kContentPupaId, East(-Cells(3)));
  const std::uint32_t common = PlaceStill(field, kTinyLunaticId, North(Cells(3)));
  StepStill(run, 2);
  CHECK(FoeOf(field, boss).hp < 50000);
  CHECK(FoeOf(field, boss).freeze_until_step == 0U);
  CHECK(FoeOf(field, immune).hp < kSturdyHp);
  CHECK(FoeOf(field, immune).freeze_until_step == 0U);
  CHECK(FoeOf(field, common).freeze_until_step > field.step);
}

// ── Le gel ──────────────────────────────────────────────────────────────────

TEST_CASE("Archétypes survivors : un gelé prend l'élément Water", "[sim][survivors]") {
  QuietContent tables;
  for (const bool frozen : {false, true}) {
    SurvivorsRun run = StartRun(tables.rules);
    SurvivorsField& field = run.field_for_tests();
    Give(field, kContentBoltId);
    const std::uint32_t serial = PlaceStill(field, kTinyLunaticId, East(Cells(4)));
    if (frozen) field.foes.back().freeze_until_step = 1000;
    StepStill(run, 2);
    const ro::sim::SurvivorsNews news = run.TakeNews();
    REQUIRE(news.hits.size() == 1);
    // Le vent contre l'eau : 175 % ; contre le neutre : 100 %.
    const std::int64_t efficiency = frozen ? 175 : 100;
    CHECK(news.hits[0].efficiency_pct == efficiency);
    CHECK(news.hits[0].amount == Expected(12, efficiency));
    CHECK(FoeOf(field, serial).hp == kSturdyHp - Expected(12, efficiency));
  }
}

TEST_CASE("Archétypes survivors : un gelé ne bouge pas et ne touche pas", "[sim][survivors]") {
  QuietContent tables;
  SurvivorsRun run = StartRun(tables.rules);
  SurvivorsField& field = run.field_for_tests();
  field.hero.skills.clear();
  PlaceFoe(field, kTinyLunaticId, field.hero.position + East(CellTenths(8)));
  SurvivorsFoe& foe = field.foes.back();
  foe.hp = kSturdyHp;
  constexpr std::uint32_t kFrozenSteps = 30;
  foe.freeze_until_step = kFrozenSteps;
  const FieldPoint frozen_at = foe.position;
  const std::int32_t full_hp = field.hero.hp;
  StepStill(run, kFrozenSteps);
  CHECK(field.foes[0].position == frozen_at);
  CHECK(field.hero.hp == full_hp);
  // Dégelé, il touche aussitôt.
  StepStill(run);
  CHECK(field.hero.hp < full_hp);
}

// ── Le projectile droit ─────────────────────────────────────────────────────

TEST_CASE("Archétypes survivors : le projectile droit ne se recale pas", "[sim][survivors]") {
  QuietContent tables;
  SurvivorsRun run = StartRun(tables.rules);
  SurvivorsField& field = run.field_for_tests();
  Give(field, kContentLinearId);
  const std::uint32_t serial = PlaceStill(field, kTinyLunaticId, East(Cells(5)));
  StepStill(run);
  REQUIRE(field.shots.size() == 1);
  CHECK(field.shots[0].straight);
  const FieldPoint velocity = field.shots[0].velocity;
  CHECK(velocity == FieldPoint{ro::sim::ShotUnitsPerStep(tables.linear), 0});
  // La vie le mène à sa portée sans la dépasser.
  const std::uint32_t life = static_cast<std::uint32_t>(tables.linear.reach_units /
                                                        ro::sim::ShotUnitsPerStep(tables.linear));
  CHECK(field.shots[0].expires_step == life);
  // Le monstre s'écarte : un chercheur le suivrait, le projectile file droit.
  field.foes[0].position = field.foes[0].position + North(Cells(3));
  StepStill(run, 5);
  REQUIRE(field.shots.size() == 1);
  CHECK(field.shots[0].velocity == velocity);
  StepStill(run, life);
  CHECK(field.shots.empty());
  CHECK(FoeOf(field, serial).hp == kSturdyHp);
}

TEST_CASE("Archétypes survivors : le projectile droit perce et gèle", "[sim][survivors]") {
  QuietContent tables;
  SurvivorsRun run = StartRun(tables.rules);
  SurvivorsField& field = run.field_for_tests();
  Give(field, kContentLinearId);
  const std::uint32_t first = PlaceStill(field, kTinyLunaticId, East(Cells(2)));
  const std::uint32_t second = PlaceStill(field, kTinyLunaticId, East(Cells(4)));
  const std::uint32_t third = PlaceStill(field, kTinyLunaticId, East(Cells(6)));
  StepStill(run, 40);
  // Un perçage : deux monstres, puis le projectile s'éteint.
  CHECK(FoeOf(field, first).hp == kSturdyHp - Expected(18));
  CHECK(FoeOf(field, second).hp == kSturdyHp - Expected(18));
  CHECK(FoeOf(field, third).hp == kSturdyHp);
  CHECK(FoeOf(field, first).freeze_until_step > 0U);
  CHECK(FoeOf(field, second).freeze_until_step > FoeOf(field, first).freeze_until_step);
  CHECK(FoeOf(field, third).freeze_until_step == 0U);
  CHECK(field.shots.empty());
}

// ── Le mur ──────────────────────────────────────────────────────────────────

TEST_CASE("Archétypes survivors : les murs, perpendiculaires au cap", "[sim][survivors]") {
  QuietContent tables;
  for (const std::uint8_t facing :
       {std::uint8_t{0}, std::uint8_t{16}, std::uint8_t{32}, std::uint8_t{48}}) {
    SurvivorsStart start = TinyStart(1);
    SetBonus(start, SurvivorsStat::kAmount, 3);
    SurvivorsRun run = StartRun(tables.rules, start);
    SurvivorsField& field = run.field_for_tests();
    Give(field, kContentWallId);
    field.hero.facing = facing;
    StepStill(run);
    REQUIRE(field.zones.size() == 4);
    // Devant, derrière, à gauche, à droite.
    constexpr std::array<std::uint8_t, 4> kTurns = {0, 32, 16, 48};
    for (std::size_t wall = 0; wall < 4; ++wall) {
      const ro::sim::SurvivorsZone& zone = field.zones[wall];
      const auto heading = static_cast<std::uint8_t>((facing + kTurns[wall]) % 64);
      const FieldPoint middle = field.hero.position + TurnVector(heading, Cells(2));
      CHECK(zone.from + zone.to == middle + middle);
      CHECK(Dot(zone.to - zone.from, TurnVector(heading, Cells(1))) == 0);
      CHECK(ro::sim::DistanceSquared(zone.from, zone.to) ==
            std::int64_t{Cells(3)} * std::int64_t{Cells(3)});
      CHECK(zone.push_heading == heading);
    }
    CHECK(CountCasts(run.TakeNews(), CastKind::kWallOn) == 4);
  }
}

TEST_CASE("Archétypes survivors : un mur compte ses coups", "[sim][survivors]") {
  QuietContent tables;
  tables.wall.knockback_units = 0;
  SurvivorsRun run = StartRun(tables.rules);
  SurvivorsField& field = run.field_for_tests();
  Give(field, kContentWallId);
  const std::uint32_t serial = PlaceStill(field, kTinyLunaticId, North(Cells(2)));
  const std::uint32_t rehit = StepsOf(tables.wall.rehit_ms);
  StepStill(run);
  CHECK(FoeOf(field, serial).hp == kSturdyHp - Expected(12));
  REQUIRE(field.zones.size() == 1);
  CHECK(field.zones[0].hits_left == 4);
  // Un coup par retouche, jusqu'au cinquième, puis le mur s'éteint.
  StepStill(run, rehit * 4);
  CHECK(FoeOf(field, serial).hp == kSturdyHp - 5 * Expected(12));
  CHECK(field.zones.empty());
  CHECK(CountCasts(run.TakeNews(), CastKind::kZoneOff) == 1);
  StepStill(run, rehit * 2);
  CHECK(FoeOf(field, serial).hp == kSturdyHp - 5 * Expected(12));
}

TEST_CASE("Archétypes survivors : le recul d'un mur", "[sim][survivors]") {
  QuietContent tables;
  constexpr std::uint32_t kKnockSteps = 15;
  SECTION("en terrain ouvert, de toute sa longueur dans le sens du cap") {
    SurvivorsRun run = StartRun(tables.rules);
    SurvivorsField& field = run.field_for_tests();
    Give(field, kContentWallId);
    const std::uint32_t pushed = PlaceStill(field, kTinyLunaticId, North(Cells(2)));
    const std::uint32_t immune =
        PlaceStill(field, kContentSavageId, North(Cells(2)) + East(Cells(1)));
    const FieldPoint pushed_from = FoeOf(field, pushed).position;
    const FieldPoint immune_from = FoeOf(field, immune).position;
    StepStill(run);
    CHECK(FoeOf(field, pushed).knock_left_units == tables.wall.knockback_units);
    CHECK(FoeOf(field, immune).knock_left_units == 0);
    const ro::sim::SurvivorsNews news = run.TakeNews();
    REQUIRE(news.hits.size() == 2);
    CHECK(news.hits[0].knocked);
    CHECK_FALSE(news.hits[1].knocked);
    StepStill(run, kKnockSteps);
    CHECK(FoeOf(field, pushed).position == pushed_from + North(tables.wall.knockback_units));
    CHECK(FoeOf(field, pushed).knock_left_units == 0);
    CHECK(FoeOf(field, immune).position == immune_from);
  }
  SECTION("arrêté par un pilier") {
    SurvivorsRun run = StartRun(tables.rules);
    SurvivorsField& field = run.field_for_tests();
    Give(field, kContentWallId);
    // Le héros à l'est de sa case de départ, sur une rangée de piliers : le mur
    // pousse vers l'est, sur le pilier des cases 42 et 43.
    field.hero.position = ro::sim::CellCenter(38, 30);
    field.hero.previous_position = field.hero.position;
    field.hero.facing = 48;
    const std::uint32_t serial = PlaceStill(field, kTinyLunaticId, East(Cells(2)));
    StepStill(run, 1 + kKnockSteps);
    const SurvivorsFoe& foe = FoeOf(field, serial);
    CHECK(foe.knock_left_units == 0);
    CHECK(ro::sim::CellOf(foe.position.x) == 41);
    CHECK(field.ground.Passable(ro::sim::CellOf(foe.position.x), ro::sim::CellOf(foe.position.y)));
  }
}

// ── Les piliers ─────────────────────────────────────────────────────────────

TEST_CASE("Archétypes survivors : le pilier s'arme, se déclenche, se réarme", "[sim][survivors]") {
  QuietContent tables;
  SurvivorsRun run = StartRun(tables.rules);
  SurvivorsField& field = run.field_for_tests();
  Give(field, kContentRingId);
  const std::uint32_t serial = PlaceStill(field, kTinyLunaticId, North(Cells(3)));
  StepStill(run);
  REQUIRE(field.zones.size() == 8);
  for (std::size_t pillar = 0; pillar < 8; ++pillar) {
    const ro::sim::SurvivorsZone& zone = field.zones[pillar];
    CHECK(zone.from == zone.to);
    CHECK(zone.from ==
          field.hero.position + TurnVector(static_cast<std::uint8_t>(pillar * 8), Cells(3)));
  }
  const std::uint32_t rearm = StepsOf(tables.ring.rehit_ms);
  // Le feu contre le neutre : 100 %.
  CHECK(FoeOf(field, serial).hp == kSturdyHp - Expected(40));
  CHECK(field.zones[0].armed_step == rearm);
  CHECK(field.zones[1].armed_step == 0U);
  StepStill(run, rearm - 1);
  CHECK(FoeOf(field, serial).hp == kSturdyHp - Expected(40));
  StepStill(run);
  CHECK(FoeOf(field, serial).hp == kSturdyHp - 2 * Expected(40));
  CHECK(field.zones[0].armed_step == 2 * rearm);
  CHECK(CountCasts(run.TakeNews(), CastKind::kRingOn) == 8);
}

// ── Le rebond ───────────────────────────────────────────────────────────────

TEST_CASE("Archétypes survivors : le rebond va au plus proche non touché", "[sim][survivors]") {
  QuietContent tables;
  SurvivorsRun run = StartRun(tables.rules);
  SurvivorsField& field = run.field_for_tests();
  Give(field, kContentBoltId);
  // Le plus robuste prend la frappe ; de lui, B est le plus proche ; de B, D.
  const FieldPoint struck_at = East(Cells(5));
  const std::uint32_t struck = PlaceStill(field, kTinyLunaticId, struck_at);
  const std::uint32_t second = PlaceStill(field, kTinyLunaticId, struck_at + North(Cells(2)), 100);
  const std::uint32_t skipped =
      PlaceStill(field, kTinyLunaticId, struck_at + East(CellTenths(25)), 100);
  const std::uint32_t third = PlaceStill(field, kTinyLunaticId, struck_at + North(Cells(3)), 100);
  StepStill(run, 2);
  const std::int32_t bounced = Expected(12 * 60 / 100);
  CHECK(FoeOf(field, struck).hp == kSturdyHp - Expected(12));
  CHECK(FoeOf(field, second).hp == 100 - bounced);
  CHECK(FoeOf(field, third).hp == 100 - bounced);
  CHECK(FoeOf(field, skipped).hp == 100);
  const ro::sim::SurvivorsNews news = run.TakeNews();
  REQUIRE(news.hits.size() == 3);
  CHECK(news.hits[0].foe_serial == struck);
  CHECK(news.hits[1].foe_serial == second);
  CHECK(news.hits[2].foe_serial == third);
  CHECK(CountCasts(news, CastKind::kStrike) == 3);
}

// ── Les plafonds ────────────────────────────────────────────────────────────

TEST_CASE("Archétypes survivors : sous un plafond plein, le skill attend", "[sim][survivors]") {
  QuietContent tables;
  SECTION("les éclats") {
    tables.rules.caps.most_blasts = 1;
    tables.blast.amount = 2;
    SurvivorsRun run = StartRun(tables.rules);
    SurvivorsField& field = run.field_for_tests();
    Give(field, kContentBlastId);
    PlaceStill(field, kTinyLunaticId, East(Cells(3)));
    PlaceStill(field, kTinyLunaticId, East(-Cells(3)));
    field.blasts.push_back(ro::sim::SurvivorsBlast{.land_step = 1000000});
    StepStill(run, 3);
    CHECK(field.blasts.size() == 1);
    CHECK(field.hero.skills[0].ready_step == 0U);
    field.blasts.clear();
    StepStill(run);
    // Une place : un seul des deux éclats part, et la recharge court.
    CHECK(field.blasts.size() == 1);
    CHECK(field.hero.skills[0].ready_step > field.step);
  }
  SECTION("les zones") {
    tables.rules.caps.most_zones = 1;
    SurvivorsRun run = StartRun(tables.rules);
    SurvivorsField& field = run.field_for_tests();
    Give(field, kContentWallId);
    field.zones.push_back(ro::sim::SurvivorsZone{.expires_step = 1000000});
    StepStill(run, 3);
    CHECK(field.zones.size() == 1);
    CHECK(field.hero.skills[0].ready_step == 0U);
    field.zones.clear();
    StepStill(run);
    CHECK(field.zones.size() == 1);
    CHECK(field.zones[0].skill_id == kContentWallId);
  }
}
