#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/survivors_state.h"
#include "ro/sim/walk_map.h"

#include "support/survivors_fixture.h"

using ro::sim::CellCenter;
using ro::sim::CellOf;
using ro::sim::Cells;
using ro::sim::DistanceSquared;
using ro::sim::FieldPoint;
using ro::sim::IntegerSqrt;
using ro::sim::SurvivorsField;
using ro::sim::SurvivorsFoe;
using ro::sim::SurvivorsInput;
using ro::sim::SurvivorsRules;
using ro::sim::SurvivorsRun;
using ro::sim::TurnVector;
using ro::sim::WalkMap;
using ro::test_support::Heading;
using ro::test_support::kTinyLunaticId;
using ro::test_support::kTinyPoringId;
using ro::test_support::PillarArena;
using ro::test_support::PlaceFoe;
using ro::test_support::QuietRules;
using ro::test_support::TinyRules;
using ro::test_support::TinyStart;

namespace {

// Le rayon des petits monstres et du héros dans les règles minuscules.
constexpr std::int32_t kSmallRadius = 358;
constexpr std::int32_t kHeroRadius = 409;
// La marche d'un Lunatic (200 ms par case, × 160 %) en sous-unités par pas.
constexpr std::int32_t kLunaticUnits = 64;

[[nodiscard]] SurvivorsRun StartOn(const SurvivorsRules& rules, WalkMap arena, std::uint64_t seed) {
  auto run = SurvivorsRun::Start(rules, std::move(arena), TinyStart(seed));
  REQUIRE(run.has_value());
  return std::move(*run);
}

[[nodiscard]] WalkMap OpenArena(std::int32_t side) {
  return WalkMap(side, side,
                 std::vector<std::uint8_t>(
                     static_cast<std::size_t>(side) * static_cast<std::size_t>(side), 1U));
}

// Strictement dans le cadre d'apparition des règles, centré sur `hero`.
[[nodiscard]] bool InsideFrame(const SurvivorsRules& rules, FieldPoint hero, FieldPoint point) {
  const std::int64_t dx = std::int64_t{point.x} - hero.x;
  const std::int64_t dy = std::int64_t{point.y} - hero.y;
  return dx > -rules.spawn_frame.half_width_units && dx < rules.spawn_frame.half_width_units &&
         dy > -rules.spawn_frame.south_units && dy < rules.spawn_frame.north_units;
}

[[nodiscard]] std::uint32_t Length(FieldPoint delta) {
  return IntegerSqrt(static_cast<std::uint64_t>(DistanceSquared(delta, FieldPoint{})));
}

// Une arène ouverte de `side` cases, sauf les cases (x, y) que `blocked` désigne.
template <typename Blocked>
[[nodiscard]] WalkMap WalledArena(std::int32_t side, Blocked blocked) {
  std::vector<std::uint8_t> passable(
      static_cast<std::size_t>(side) * static_cast<std::size_t>(side), 1U);
  for (std::int32_t y = 0; y < side; ++y) {
    for (std::int32_t x = 0; x < side; ++x) {
      if (blocked(x, y)) {
        passable[static_cast<std::size_t>(y) * static_cast<std::size_t>(side) +
                 static_cast<std::size_t>(x)] = 0U;
      }
    }
  }
  return WalkMap(side, side, std::move(passable));
}

// Le héros posé en `at`, immobile et sans skill.
void PlaceHero(SurvivorsField& field, FieldPoint at) {
  field.hero.skills.clear();
  field.hero.position = at;
  field.hero.previous_position = at;
}

// La course du monstre 0 vers le héros immobile : s'il l'a touché en au plus
// `most_steps` pas, et s'il a été rappelé en chemin (un pas qui ne part pas
// d'où le précédent l'avait laissé).
struct Pursuit {
  bool reached = false;
  bool recalled = false;
};
[[nodiscard]] Pursuit ChaseHero(SurvivorsRun& run, int most_steps) {
  const std::int64_t touch = std::int64_t{kHeroRadius} + kSmallRadius + 128;
  Pursuit pursuit;
  for (int step = 0; step < most_steps && !pursuit.reached; ++step) {
    const FieldPoint before = run.field().foes[0].position;
    run.Step({});
    const SurvivorsFoe& foe = run.field().foes[0];
    REQUIRE(run.field().ground.Passable(CellOf(foe.position.x), CellOf(foe.position.y)));
    pursuit.recalled = pursuit.recalled || foe.previous_position != before;
    pursuit.reached = DistanceSquared(foe.position, run.field().hero.position) < touch * touch;
  }
  return pursuit;
}

// Les règles silencieuses où le Lunatic est un mini-boss.
struct BossRules {
  std::array<ro::sim::FoeRule, ro::test_support::kTinyFoes.size()> foes =
      ro::test_support::kTinyFoes;
  SurvivorsRules rules = QuietRules();
  BossRules() {
    for (ro::sim::FoeRule& foe : foes) {
      if (foe.foe_id == kTinyLunaticId) foe.rank = ro::sim::FoeRank::kMiniBoss;
    }
    rules.foes = foes;
  }
  BossRules(const BossRules&) = delete;
  BossRules& operator=(const BossRules&) = delete;
};

}  // namespace

TEST_CASE("Horde survivors : la marche et l'aimant du héros", "[sim][survivors]") {
  const auto run = SurvivorsRun::Start(TinyRules(), PillarArena(64), TinyStart(1));
  REQUIRE(run.has_value());
  // 150 ms par case : 136 sous-unités par pas ; l'aimant de la classe tel quel.
  CHECK(run->field().hero.units_per_step == 136);
  CHECK(run->field().hero.magnet_units == ro::sim::Cells(2));
  CHECK(run->field().foes.empty());
}

TEST_CASE("Horde survivors : le héros glisse le long d'un mur", "[sim][survivors]") {
  // Un mur plein sur la rangée 11, juste au nord du départ (10, 10).
  constexpr std::int32_t kSide = 20;
  constexpr std::int32_t kWallRow = 11;
  std::vector<std::uint8_t> passable(static_cast<std::size_t>(kSide * kSide), 1U);
  for (std::int32_t x = 0; x < kSide; ++x) {
    passable[static_cast<std::size_t>(kWallRow * kSide + x)] = 0U;
  }
  SurvivorsRun run = StartOn(QuietRules(), WalkMap(kSide, kSide, std::move(passable)), 1);
  const FieldPoint start = run.field().hero.position;
  REQUIRE(start == CellCenter(10, 10));

  // Au nord-ouest, 136 sous-unités par pas : (−96, +96) par la table.
  constexpr std::uint8_t kNorthWest = 8;
  constexpr int kSteps = 20;
  REQUIRE(TurnVector(kNorthWest, 136) == FieldPoint{-96, 96});
  for (int step = 0; step < kSteps; ++step) run.Step(Heading(kNorthWest));
  const FieldPoint at = run.field().hero.position;
  // L'axe x avance à chaque pas ; l'axe y s'arrête au dernier pas avant le mur.
  CHECK(at.x == start.x - 96 * kSteps);
  CHECK(at.y == start.y + 96 * 5);
  CHECK(CellOf(at.y) == kWallRow - 1);
  CHECK(run.field().hero.facing == kNorthWest);
  // Un cap hors de [0, 64) se ramène modulo 64.
  run.Step(Heading(kNorthWest + ro::sim::kHeadingSteps));
  CHECK(run.field().hero.position.x == at.x - 96);
  CHECK(run.field().hero.previous_position == at);
  // À l'arrêt, le héros garde son cap et sa place.
  run.Step({});
  CHECK(run.field().hero.position == run.field().hero.previous_position);
  CHECK(run.field().hero.facing == kNorthWest);
}

TEST_CASE("Horde survivors : le bond de Back Slide l'emporte sur la marche, sans retour",
          "[sim][survivors]") {
  constexpr std::int32_t kSide = 40;
  std::vector<std::uint8_t> passable(static_cast<std::size_t>(kSide * kSide), 1U);
  SurvivorsRun run = StartOn(QuietRules(), WalkMap(kSide, kSide, std::move(passable)), 1);
  constexpr std::uint8_t kEast = 48;
  constexpr std::uint8_t kWest = 16;
  ro::sim::SurvivorsHero& hero = run.field_for_tests().hero;
  const FieldPoint start = hero.position;
  hero.facing = kWest;
  hero.leap_heading = kEast;
  hero.leap_steps_left = ro::sim::kBackSlideLeapSteps;
  // La marche demandée vers l'ouest n'a pas la main tant que le bond dure.
  for (std::uint8_t step = 0; step < ro::sim::kBackSlideLeapSteps; ++step) {
    run.Step(Heading(kWest));
  }
  const FieldPoint landing =
      start + TurnVector(kEast, ro::sim::Cells(ro::sim::kBackSlideLeapCells) /
                                    ro::sim::kBackSlideLeapSteps * ro::sim::kBackSlideLeapSteps);
  CHECK(hero.position == landing);
  CHECK(hero.facing == kWest);
  CHECK(hero.leap_steps_left == 0U);
  // Au bout du bond, il reste là, et la marche reprend.
  run.Step({});
  CHECK(hero.position == landing);
  run.Step(Heading(kWest));
  CHECK(hero.position.x < landing.x);
}

TEST_CASE("Horde survivors : la même vitesse dans tous les caps", "[sim][survivors]") {
  for (std::uint8_t heading = 0; heading < ro::sim::kHeadingSteps; ++heading) {
    SurvivorsRun run = StartOn(QuietRules(), OpenArena(64), 2);
    run.Step(Heading(heading));
    const SurvivorsField& field = run.field();
    const std::uint32_t moved = Length(field.hero.position - field.hero.previous_position);
    INFO("cap " << int{heading});
    CHECK(moved >= 135U);
    CHECK(moved <= 137U);
  }
}

TEST_CASE("Horde survivors : un monstre contourne un pilier et rejoint le héros",
          "[sim][survivors]") {
  SurvivorsRun run = StartOn(QuietRules(), PillarArena(64), 3);
  SurvivorsField& field = run.field_for_tests();
  field.hero.skills.clear();
  // La diagonale de (22, 22) vers le héros (32, 32) traverse le pilier (30, 30).
  (void)PlaceFoe(field, kTinyLunaticId, CellCenter(22, 22));
  const std::int64_t touch = std::int64_t{kHeroRadius} + kSmallRadius + 128;
  bool reached = false;
  for (int step = 0; step < 400 && !reached; ++step) {
    run.Step({});
    const SurvivorsFoe& foe = run.field().foes[0];
    REQUIRE(run.field().ground.Passable(CellOf(foe.position.x), CellOf(foe.position.y)));
    reached = DistanceSquared(foe.position, run.field().hero.position) < touch * touch;
  }
  CHECK(reached);
  CHECK(run.field().flow.floods() >= 1U);
}

TEST_CASE("Horde survivors : la poursuite suit le champ autour d'un U", "[sim][survivors]") {
  // Un U ouvert au sud : le fond sur la rangée 18 (x de 14 à 26), les bras sur
  // les colonnes 14 et 26 (y de 12 à 18). Le héros est dans le creux, le
  // monstre au nord du fond, aligné sur lui : marcher droit le collerait au
  // mur pour toujours.
  constexpr std::int32_t kSide = 40;
  constexpr std::int32_t kWest = 14;
  constexpr std::int32_t kEast = 26;
  constexpr std::int32_t kBottom = 12;
  constexpr std::int32_t kTop = 18;
  const WalkMap arena = WalledArena(kSide, [](std::int32_t x, std::int32_t y) {
    const bool top = y == kTop && x >= kWest && x <= kEast;
    const bool arm = (x == kWest || x == kEast) && y >= kBottom && y <= kTop;
    return top || arm;
  });
  SurvivorsRun run = StartOn(QuietRules(), arena, 3);
  SurvivorsField& field = run.field_for_tests();
  PlaceHero(field, CellCenter(20, 14));
  (void)PlaceFoe(field, kTinyLunaticId, CellCenter(20, 24));
  // Il fait le tour : un bras franchi par le sud, puis le creux.
  const std::int64_t touch = std::int64_t{kHeroRadius} + kSmallRadius + 128;
  bool went_around = false;
  bool reached = false;
  for (int step = 0; step < 1500 && !reached; ++step) {
    run.Step({});
    const SurvivorsFoe& foe = run.field().foes[0];
    went_around = went_around || CellOf(foe.position.y) < kBottom;
    reached = DistanceSquared(foe.position, run.field().hero.position) < touch * touch;
  }
  CHECK(went_around);
  CHECK(reached);
}

TEST_CASE("Horde survivors : un pilier entre le monstre et le héros", "[sim][survivors]") {
  // Le pilier de PillarArena(160) sur (78..79, 78..79). Le héros contre sa face
  // est, le monstre derrière sa face ouest, à moins de trois cases : marcher
  // droit le figerait, aligné sur le héros.
  SECTION("un monstre ordinaire, de près, contourne") {
    SurvivorsRun run = StartOn(QuietRules(), PillarArena(160), 21);
    SurvivorsField& field = run.field_for_tests();
    PlaceHero(field, FieldPoint{Cells(80) + 450, Cells(79)});
    (void)PlaceFoe(field, kTinyLunaticId, FieldPoint{Cells(77) + 600, Cells(79) + 300});
    REQUIRE(DistanceSquared(field.foes[0].position, field.hero.position) <
            std::int64_t{Cells(3)} * Cells(3));
    const Pursuit pursuit = ChaseHero(run, 300);
    CHECK(pursuit.reached);
    CHECK_FALSE(pursuit.recalled);
  }
  SECTION("un boss hors du champ, arrêté par un pilier, est rappelé") {
    // À 30 cases à l'ouest, sur la rangée des piliers : hors de la fenêtre du
    // champ (20 cases), il marche droit et bute sur le pilier (54, 79).
    const BossRules boss;
    SurvivorsRun run = StartOn(boss.rules, PillarArena(160), 22);
    SurvivorsField& field = run.field_for_tests();
    PlaceHero(field, CellCenter(80, 79));
    (void)PlaceFoe(field, kTinyLunaticId, CellCenter(50, 79));
    const Pursuit pursuit = ChaseHero(run, 1200);
    CHECK(pursuit.recalled);
    CHECK(pursuit.reached);
  }
}

TEST_CASE("Horde survivors : une impasse ne reçoit ni naissance ni rappel", "[sim][survivors]") {
  // Un enclos praticable mais fermé sur le côté nord du cadre d'apparition :
  // l'intérieur x de 20 à 44, y de 40 à 42 ; le héros au centre (32, 32).
  constexpr std::int32_t kSide = 64;
  const auto inside = [](std::int32_t x, std::int32_t y) {
    return x >= 20 && x <= 44 && y >= 40 && y <= 42;
  };
  const WalkMap arena = WalledArena(kSide, [](std::int32_t x, std::int32_t y) {
    const bool rows = (y == 39 || y == 43) && x >= 19 && x <= 45;
    const bool columns = (x == 19 || x == 45) && y >= 39 && y <= 43;
    return rows || columns;
  });
  SECTION("aucune naissance dans l'enclos") {
    SurvivorsRun run = StartOn(TinyRules(), arena, 23);
    run.field_for_tests().hero.skills.clear();
    for (int step = 0; step < 200; ++step) {
      run.Step({});
      for (const SurvivorsFoe& foe : run.field().foes) {
        CHECK_FALSE(inside(CellOf(foe.position.x), CellOf(foe.position.y)));
      }
    }
    CHECK(run.field().foes.size() >= TinyRules().waves[0].floor);
  }
  SECTION("un monstre dans l'enclos est rappelé, même à moins de 48 cases") {
    SurvivorsRun run = StartOn(QuietRules(), arena, 24);
    SurvivorsField& field = run.field_for_tests();
    field.hero.skills.clear();
    (void)PlaceFoe(field, kTinyLunaticId, CellCenter(30, 41));
    run.Step({});
    const SurvivorsFoe& foe = run.field().foes[0];
    CHECK_FALSE(inside(CellOf(foe.previous_position.x), CellOf(foe.previous_position.y)));
    CHECK_FALSE(InsideFrame(QuietRules(), run.field().hero.position, foe.previous_position));
    CHECK(run.field().ground.Passable(CellOf(foe.previous_position.x),
                                      CellOf(foe.previous_position.y)));
  }
}

TEST_CASE("Horde survivors : deux monstres superposés s'écartent", "[sim][survivors]") {
  // À deux cases du héros, en deçà de la poursuite directe : les deux marchent
  // droit sur lui, d'un pas de Lunatic.
  const FieldPoint offset{Cells(2), 0};
  const auto play = [offset] {
    SurvivorsRun run = StartOn(QuietRules(), PillarArena(64), 4);
    SurvivorsField& field = run.field_for_tests();
    field.hero.skills.clear();
    const FieldPoint spot = field.hero.position + offset;
    (void)PlaceFoe(field, kTinyLunaticId, spot);
    (void)PlaceFoe(field, kTinyLunaticId, spot);
    run.Step({});
    return std::pair{run.field().foes[0].position, run.field().foes[1].position};
  };
  const auto [first, second] = play();
  const SurvivorsRun fresh = StartOn(QuietRules(), PillarArena(64), 4);
  const FieldPoint chased = fresh.field().hero.position + offset - FieldPoint{kLunaticUnits, 0};
  // Puis 716 de recouvrement : la moitié chacun, plafonnée à 128, le second au
  // cap (2 × 23) % 64, le premier à l'opposé.
  constexpr std::uint8_t kSecondHeading = (2 * 23) % 64;
  constexpr std::uint8_t kFirstHeading = (kSecondHeading + 32) % 64;
  CHECK(second == chased + TurnVector(kSecondHeading, 128));
  CHECK(first == chased + TurnVector(kFirstHeading, 128));
  // Le plafond, mesuré sur les places réelles.
  CHECK(Length(second - chased) <= 128U);
  CHECK(Length(first - chased) <= 128U);
  // Déterministe : la même partie rend les mêmes places.
  CHECK(play() == std::pair{first, second});
}

TEST_CASE("Horde survivors : un monstre immobile n'est pas poussé", "[sim][survivors]") {
  SECTION("par un autre monstre") {
    SurvivorsRun run = StartOn(QuietRules(), PillarArena(64), 5);
    SurvivorsField& field = run.field_for_tests();
    field.hero.skills.clear();
    const FieldPoint spot = field.hero.position + FieldPoint{Cells(6), 0};
    (void)PlaceFoe(field, kTinyLunaticId, spot);
    field.foes[0].units_per_step = 0;
    (void)PlaceFoe(field, kTinyLunaticId, spot);
    run.Step({});
    const SurvivorsFoe& still = run.field().foes[0];
    const SurvivorsFoe& mobile = run.field().foes[1];
    CHECK(still.position == spot);
    // Le mobile a marché d'un pas, puis pris toute la poussée, plafonnée.
    const std::uint32_t apart = Length(mobile.position - still.position);
    CHECK(apart >= static_cast<std::uint32_t>(kLunaticUnits + 128 - 2));
    CHECK(apart <= static_cast<std::uint32_t>(kLunaticUnits + 128));
  }
  SECTION("par le héros, qui prend tout le recouvrement") {
    SurvivorsRun run = StartOn(QuietRules(), PillarArena(64), 6);
    SurvivorsField& field = run.field_for_tests();
    field.hero.skills.clear();
    const FieldPoint hero = field.hero.position;
    const FieldPoint spot = hero + FieldPoint{300, 0};
    (void)PlaceFoe(field, kTinyLunaticId, spot);
    field.foes[0].units_per_step = 0;
    run.Step({});
    CHECK(run.field().foes[0].position == spot);
    const std::int32_t overlap = kHeroRadius + kSmallRadius - 300;
    CHECK(run.field().hero.position == hero - FieldPoint{overlap, 0});
  }
}

TEST_CASE("Horde survivors : le héros n'est repoussé que de 20 %", "[sim][survivors]") {
  SurvivorsRun run = StartOn(QuietRules(), PillarArena(64), 7);
  SurvivorsField& field = run.field_for_tests();
  field.hero.skills.clear();
  const FieldPoint hero = field.hero.position;
  (void)PlaceFoe(field, kTinyLunaticId, hero + FieldPoint{300, 0});
  run.Step({});
  // Le monstre marche d'un pas vers le héros, puis le recouvrement se partage.
  const std::int32_t gap = 300 - kLunaticUnits;
  const std::int32_t overlap = kHeroRadius + kSmallRadius - gap;
  const std::int32_t hero_push = overlap * 20 / 100;
  const std::int32_t foe_push = overlap * 80 / 100;
  CHECK(run.field().hero.position == hero - FieldPoint{hero_push, 0});
  CHECK(run.field().foes[0].position == hero + FieldPoint{gap + foe_push, 0});
  CHECK(run.field().hero.previous_position == hero);
}

TEST_CASE("Horde survivors : le rappel au-delà de 48 cases", "[sim][survivors]") {
  const SurvivorsRules& rules = QuietRules();
  const auto recalled = [&rules](std::uint8_t heading) {
    SurvivorsRun run = StartOn(rules, PillarArena(160), 8);
    SurvivorsField& field = run.field_for_tests();
    field.hero.skills.clear();
    // 60 cases au sud du héros.
    (void)PlaceFoe(field, kTinyLunaticId, CellCenter(80, 20));
    run.Step(Heading(heading));
    const SurvivorsFoe& foe = run.field().foes[0];
    const FieldPoint hero = run.field().hero.position;
    CHECK_FALSE(InsideFrame(rules, hero, foe.previous_position));
    CHECK(run.field().ground.Passable(CellOf(foe.previous_position.x),
                                      CellOf(foe.previous_position.y)));
    CHECK(Length(foe.position - foe.previous_position) <=
          static_cast<std::uint32_t>(kLunaticUnits));
    return foe.previous_position - hero;
  };
  SECTION("le héros marche au nord : le monstre revient au nord") {
    const FieldPoint offset = recalled(0);
    CHECK(offset.y >= rules.spawn_frame.north_units);
    CHECK(offset.y <= rules.spawn_frame.north_units + rules.spawn_frame.margin_units + Cells(1));
    CHECK(offset.x >= -Cells(1));
    CHECK(offset.x <= Cells(1));
  }
  SECTION("le héros marche à l'est : le monstre revient à l'est") {
    const FieldPoint offset = recalled(48);
    CHECK(offset.x >= rules.spawn_frame.half_width_units);
    CHECK(offset.y <= -rules.spawn_frame.south_units + Cells(1));
  }
  SECTION("en deçà de 48 cases, pas de rappel") {
    SurvivorsRun run = StartOn(rules, PillarArena(160), 8);
    run.field_for_tests().hero.skills.clear();
    const FieldPoint spot = run.field().hero.position - FieldPoint{0, Cells(40)};
    (void)PlaceFoe(run.field_for_tests(), kTinyLunaticId, spot);
    run.Step({});
    CHECK(run.field().foes[0].previous_position == spot);
  }
  SECTION("le héros collé au bord ouest et tourné vers lui : le monstre revient à l'est") {
    SurvivorsRun run = StartOn(rules, PillarArena(160), 8);
    SurvivorsField& field = run.field_for_tests();
    field.hero.skills.clear();
    field.hero.position = CellCenter(2, 80);
    field.hero.previous_position = field.hero.position;
    (void)PlaceFoe(field, kTinyLunaticId, CellCenter(70, 80));
    constexpr std::uint8_t kWest = 16;
    run.Step(Heading(kWest));
    const SurvivorsFoe& foe = run.field().foes[0];
    const FieldPoint hero = run.field().hero.position;
    REQUIRE(CellOf(hero.x) == 2);
    // L'ouest tombe hors de l'arène : le côté opposé le remplace.
    const FieldPoint offset = foe.previous_position - hero;
    CHECK(offset.x >= rules.spawn_frame.half_width_units);
    CHECK(offset.x <=
          rules.spawn_frame.half_width_units + rules.spawn_frame.margin_units + Cells(1));
    CHECK_FALSE(InsideFrame(rules, hero, foe.previous_position));
    CHECK(run.field().ground.Passable(CellOf(foe.previous_position.x),
                                      CellOf(foe.previous_position.y)));
  }
  SECTION("un boss est rappelé comme les autres") {
    const BossRules boss;
    SurvivorsRun run = StartOn(boss.rules, PillarArena(160), 8);
    run.field_for_tests().hero.skills.clear();
    const FieldPoint spot = CellCenter(80, 20);
    (void)PlaceFoe(run.field_for_tests(), kTinyLunaticId, spot);
    run.Step({});
    const SurvivorsFoe& foe = run.field().foes[0];
    CHECK_FALSE(foe.previous_position == spot);
    CHECK_FALSE(InsideFrame(rules, run.field().hero.position, foe.previous_position));
  }
  SECTION("un rappel nul est éteint") {
    SurvivorsRules still_rules = rules;
    still_rules.tuning.recall_units = 0;
    SurvivorsRun run = StartOn(still_rules, PillarArena(160), 8);
    run.field_for_tests().hero.skills.clear();
    const FieldPoint spot = CellCenter(80, 20);
    (void)PlaceFoe(run.field_for_tests(), kTinyLunaticId, spot);
    run.Step({});
    CHECK(run.field().foes[0].previous_position == spot);
  }
}

TEST_CASE("Horde survivors : le glaçage ralentit, moins un boss", "[sim][survivors]") {
  // À deux cases à l'est du héros : la poursuite directe, droit à l'ouest.
  const auto chilled_step = [](const SurvivorsRules& rules) {
    SurvivorsRun run = StartOn(rules, PillarArena(64), 20);
    SurvivorsField& field = run.field_for_tests();
    field.hero.skills.clear();
    const FieldPoint spot = field.hero.position + FieldPoint{Cells(2), 0};
    (void)PlaceFoe(field, kTinyLunaticId, spot);
    constexpr std::uint32_t kChillSteps = 10;
    field.foes[0].chill_until_step = field.step + kChillSteps;
    run.Step({});
    return spot.x - run.field().foes[0].position.x;
  };
  // 30 % de ralentissement : 64 × 70 / 100.
  CHECK(chilled_step(QuietRules()) == kLunaticUnits * 70 / 100);

  // Un boss n'en subit que la moitié, 15 % : 64 × 85 / 100.
  std::array<ro::sim::FoeRule, ro::test_support::kTinyFoes.size()> foes =
      ro::test_support::kTinyFoes;
  for (ro::sim::FoeRule& foe : foes) {
    if (foe.foe_id == kTinyLunaticId) foe.rank = ro::sim::FoeRank::kBoss;
  }
  SurvivorsRules boss_rules = QuietRules();
  boss_rules.foes = foes;
  CHECK(chilled_step(boss_rules) == kLunaticUnits * 85 / 100);
}

TEST_CASE("Horde survivors : les apparitions", "[sim][survivors]") {
  SECTION("jamais dans le cadre ni sur un mur, plancher et lot") {
    const SurvivorsRules& rules = TinyRules();
    SurvivorsRun run = StartOn(rules, PillarArena(128), 9);
    run.field_for_tests().hero.skills.clear();
    const FieldPoint hero = run.field().hero.position;
    for (std::uint32_t step = 0; step < 80; ++step) {
      run.Step({});
      const SurvivorsField& field = run.field();
      REQUIRE(field.hero.position == hero);
      for (const SurvivorsFoe& foe : field.foes) {
        if (foe.born_step != step) continue;
        CHECK_FALSE(InsideFrame(rules, hero, foe.previous_position));
        CHECK(field.ground.Passable(CellOf(foe.previous_position.x),
                                    CellOf(foe.previous_position.y)));
      }
      // Le plancher de 4 dès le premier pas ; un lot d'un monstre toutes les
      // 1 000 ms (50 pas).
      const std::size_t expected = step < 50 ? 4 : 5;
      CHECK(field.foes.size() == expected);
    }
    // 200 ms par case × 160 % : un Lunatic plus LENT, 64 sous-unités par pas ;
    // un Poring, 400 × 160 %, 32.
    STATIC_REQUIRE(ro::sim::UnitsPerStep(320) == kLunaticUnits);
    for (const SurvivorsFoe& foe : run.field().foes) {
      CHECK(foe.units_per_step == (foe.foe_id == kTinyLunaticId ? kLunaticUnits : 32));
      CHECK(foe.max_hp == ro::sim::FindFoe(rules, foe.foe_id)->hp);
      CHECK(foe.touch_ready_step == foe.born_step);
    }
  }
  SECTION("au plus 10 naissances de plancher par pas") {
    std::array<ro::sim::WaveRule, 1> waves{ro::test_support::kQuietWaves[0]};
    waves[0].floor = 30;
    SurvivorsRules rules = TinyRules();
    rules.waves = waves;
    SurvivorsRun run = StartOn(rules, PillarArena(128), 10);
    run.field_for_tests().hero.skills.clear();
    // Dix par pas jusqu'au plancher, puis plus rien : le lot n'arrive qu'à 50 pas.
    REQUIRE(rules.tuning.most_floor_spawns_per_step == 10);
    const std::array<std::size_t, 6> expected{10, 20, 30, 30, 30, 30};
    for (const std::size_t alive : expected) {
      run.Step({});
      CHECK(run.field().foes.size() == alive);
    }
  }
  SECTION("le plafond borne le plancher") {
    std::array<ro::sim::WaveRule, 1> waves{ro::test_support::kQuietWaves[0]};
    waves[0].floor = 50;
    SurvivorsRules rules = TinyRules();
    rules.waves = waves;
    SurvivorsRun run = StartOn(rules, PillarArena(128), 11);
    run.field_for_tests().hero.skills.clear();
    for (int step = 0; step < 6; ++step) run.Step({});
    CHECK(run.field().foes.size() == rules.caps.most_foes);
  }
  SECTION("un lot interrompu ne reprend que son reste") {
    // Le héros près du coin sud-ouest : les côtés ouest et sud du cadre tombent
    // hors de l'arène, et un essai unique par monstre échoue une fois sur deux.
    // Un lot de huit, sans plancher.
    std::array<ro::sim::WaveRule, 1> waves{ro::test_support::kQuietWaves[0]};
    waves[0].batch = 8;
    SurvivorsRules rules = TinyRules();
    rules.waves = waves;
    rules.tuning.spawn_tries_per_foe = 1;
    SurvivorsRun run = StartOn(rules, OpenArena(64), 25);
    PlaceHero(run.field_for_tests(), CellCenter(3, 3));
    const std::uint32_t due_step = ro::sim::StepsOf(waves[0].interval_ms);
    while (run.field().wave_spawn_step == 0) {
      REQUIRE(run.field().step < due_step + 200);
      run.Step({});
      // Jamais plus que le lot, même repris plusieurs fois.
      REQUIRE(run.field().foes.size() <= waves[0].batch);
    }
    // Le lot s'est bien interrompu au moins une fois, puis s'est accompli.
    CHECK(run.field().wave_spawn_step > due_step);
    CHECK(run.field().foes.size() == waves[0].batch);
    CHECK(run.field().wave_batch_born == 0);
    // Chaque interruption est une naissance sans place, d'un essai unique.
    CHECK(run.field().spawn_draws_failed > 0);
    CHECK(run.field().spawn_tries_failed == run.field().spawn_draws_failed);
  }
  SECTION("les PV grandissent avec les minutes") {
    const SurvivorsRules& rules = TinyRules();
    SurvivorsRun run = StartOn(rules, PillarArena(128), 12);
    run.field_for_tests().hero.skills.clear();
    run.field_for_tests().step = ro::sim::kStepsPerMinute;
    run.Step({});
    REQUIRE_FALSE(run.field().foes.empty());
    for (const SurvivorsFoe& foe : run.field().foes) {
      const std::int32_t base = ro::sim::FindFoe(rules, foe.foe_id)->hp;
      CHECK(foe.max_hp == base * 115 / 100);
      CHECK(foe.hp == foe.max_hp);
      CHECK(foe.born_step == ro::sim::kStepsPerMinute);
    }
  }
  SECTION("même graine, mêmes naissances") {
    const auto born = [](std::uint64_t seed) {
      SurvivorsRun run = StartOn(TinyRules(), PillarArena(128), seed);
      run.field_for_tests().hero.skills.clear();
      for (int step = 0; step < 60; ++step) run.Step({});
      std::vector<std::pair<std::uint16_t, FieldPoint>> foes;
      for (const SurvivorsFoe& foe : run.field().foes) foes.emplace_back(foe.foe_id, foe.position);
      return foes;
    };
    CHECK(born(13) == born(13));
    CHECK(born(13) != born(14));
  }
}

TEST_CASE("Horde survivors : un boss naît à son heure, hors plafond", "[sim][survivors]") {
  constexpr std::uint32_t kBossMs = 100;
  const std::array<ro::sim::BossRule, 1> bosses{{{.at_ms = kBossMs, .foe_id = kTinyPoringId}}};
  SurvivorsRules rules = QuietRules();
  rules.bosses = bosses;
  SurvivorsRun run = StartOn(rules, PillarArena(128), 15);
  SurvivorsField& field = run.field_for_tests();
  field.hero.skills.clear();
  for (std::uint16_t placed = 0; placed < rules.caps.most_foes; ++placed) {
    (void)PlaceFoe(field, kTinyLunaticId, field.hero.position + FieldPoint{Cells(6), 0});
  }
  const std::uint32_t boss_step = kBossMs / ro::sim::kSurvivorsStepMs;
  for (std::uint32_t step = 0; step < boss_step; ++step) run.Step({});
  CHECK(run.field().foes.size() == rules.caps.most_foes);
  CHECK(run.TakeNews().boss_arrivals.empty());
  run.Step({});
  REQUIRE(run.field().foes.size() == rules.caps.most_foes + 1U);
  const SurvivorsFoe& boss = run.field().foes.back();
  CHECK(boss.foe_id == kTinyPoringId);
  CHECK(boss.born_step == boss_step);
  CHECK(run.TakeNews().boss_arrivals == std::vector<std::uint32_t>{boss.serial});
  CHECK_FALSE(InsideFrame(rules, run.field().hero.position, boss.previous_position));
  run.Step({});
  CHECK(run.field().foes.size() == rules.caps.most_foes + 1U);
}

TEST_CASE("Horde survivors : le plancher d'une équipe n'a pas la borne d'un lot",
          "[sim][survivors]") {
  // Le plancher de deux héros vaut 110 % : 330 ici, sous le plafond de la horde.
  constexpr std::size_t kMostBatch = 255;
  std::array<ro::sim::WaveRule, 1> waves{ro::test_support::kQuietWaves[0]};
  waves[0].floor = 300;
  SurvivorsRules rules = TinyRules();
  rules.waves = waves;
  rules.caps.most_foes = 400;
  rules.tuning.most_floor_spawns_per_step = 100;
  ro::sim::SurvivorsStart start = TinyStart(10);
  start.allies.push_back(ro::sim::SurvivorsAllyStart{.class_id = start.class_id,
                                                     .first_skill_id = start.first_skill_id});
  auto run = SurvivorsRun::Start(rules, PillarArena(128), start);
  REQUIRE(run.has_value());
  for (int step = 0; step < 5; ++step) run->Step({});
  CHECK(run->field().foes.size() > kMostBatch);
}
