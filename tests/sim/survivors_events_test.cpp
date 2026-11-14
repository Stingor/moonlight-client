#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/pcg32.h"
#include "ro/sim/survivors_content.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/survivors_score.h"
#include "ro/sim/survivors_state.h"
#include "ro/sim/walk_map.h"

#include "support/survivors_content_fixture.h"
#include "support/survivors_fixture.h"

using ro::sim::CellOf;
using ro::sim::Cells;
using ro::sim::EventKind;
using ro::sim::EventRule;
using ro::sim::FieldPoint;
using ro::sim::FindFoe;
using ro::sim::FoeRule;
using ro::sim::HeadingOf;
using ro::sim::kNoHeading;
using ro::sim::SurvivorsField;
using ro::sim::SurvivorsFoe;
using ro::sim::SurvivorsRun;
using ro::sim::SurvivorsStart;
using ro::sim::TurnVector;
using ro::sim::WalkMap;
using ro::test_support::ContentTables;
using ro::test_support::kContentBaphometId;
using ro::test_support::kContentFamiliarId;
using ro::test_support::kContentPupaId;
using ro::test_support::kContentThiefBugId;
using ro::test_support::kContentWolfId;
using ro::test_support::kTinyLunaticId;
using ro::test_support::kTinyPoringId;
using ro::test_support::PillarArena;
using ro::test_support::PlaceFoe;
using ro::test_support::StepStill;
using ro::test_support::TinyStart;

namespace {

constexpr std::int32_t kArenaSideCells = 64;
// Le cadre des règles minuscules, en cases : 10 de part et d'autre, 8 au nord,
// 7 au sud.
constexpr std::int32_t kFrameHalfWidthCells = 10;
constexpr std::int32_t kFrameNorthCells = 8;
constexpr std::int32_t kFrameSouthCells = 7;
constexpr std::uint32_t kFrameSides = 4;
constexpr std::uint32_t kEventMs = 10000;
constexpr std::uint32_t kEventStep = kEventMs / ro::sim::kSurvivorsStepMs;
constexpr std::int32_t kSturdyHeroHp = 1000000;
// Le premier lot d'une vague tombe un intervalle (1 000 ms) après le départ.
constexpr std::uint32_t kFirstBatchStep = 1000 / ro::sim::kSurvivorsStepMs;

// Les règles de contenu sans aucune naissance de vague, de boss, d'élite ni de
// plante : un essai n'y garde que ce qu'il mesure.
struct QuietContent : ContentTables {
  QuietContent() {
    rules.waves = ro::test_support::kQuietWaves;
    rules.events = {};
    rules.bosses = {};
    rules.elites = {};
    rules.plants = {};
  }
};

// Une arène toute praticable : une naissance tombe sur la case voulue.
[[nodiscard]] WalkMap OpenArena() {
  const auto side = static_cast<std::size_t>(kArenaSideCells);
  return WalkMap(kArenaSideCells, kArenaSideCells, std::vector<std::uint8_t>(side * side, 1U));
}

[[nodiscard]] SurvivorsRun StartRun(const ro::sim::SurvivorsRules& rules,
                                    WalkMap arena = OpenArena(),
                                    SurvivorsStart start = TinyStart(1)) {
  auto run = SurvivorsRun::Start(rules, std::move(arena), start);
  REQUIRE(run.has_value());
  run->field_for_tests().hero.skills.clear();
  return std::move(*run);
}

// Le milieu du côté `side` (nord, est, sud, ouest) du cadre autour de `hero`.
[[nodiscard]] FieldPoint SideMiddle(FieldPoint hero, std::uint32_t side) {
  constexpr std::array<FieldPoint, kFrameSides> kOffsets{{
      {0, Cells(kFrameNorthCells)},
      {Cells(kFrameHalfWidthCells), 0},
      {0, -Cells(kFrameSouthCells)},
      {-Cells(kFrameHalfWidthCells), 0},
  }};
  return hero + kOffsets[side];
}

// Les cases de naissance des monstres, lues dans `previous_position` : le pas
// qui les fait naître les fait aussi marcher.
struct Cell {
  std::int32_t x = 0;
  std::int32_t y = 0;
  [[nodiscard]] bool operator==(const Cell&) const = default;
};
[[nodiscard]] std::vector<Cell> BirthCells(const SurvivorsField& field) {
  std::vector<Cell> cells;
  for (const SurvivorsFoe& foe : field.foes) {
    cells.push_back({CellOf(foe.previous_position.x), CellOf(foe.previous_position.y)});
  }
  return cells;
}

// Vrai si les cases tiennent sur une ligne de l'axe `along_x` (x varie, y
// fixe) ou de l'autre, contiguës, sans doublon.
[[nodiscard]] bool OnContiguousLine(std::vector<Cell> cells, bool along_x) {
  if (cells.empty()) return false;
  const auto fixed = [&](const Cell& cell) { return along_x ? cell.y : cell.x; };
  const auto moving = [&](const Cell& cell) { return along_x ? cell.x : cell.y; };
  for (const Cell& cell : cells) {
    if (fixed(cell) != fixed(cells.front())) return false;
  }
  std::sort(cells.begin(), cells.end(),
            [&](const Cell& a, const Cell& b) { return moving(a) < moving(b); });
  for (std::size_t index = 1; index < cells.size(); ++index) {
    if (moving(cells[index]) != moving(cells[index - 1]) + 1) return false;
  }
  return true;
}

// Les règles silencieuses avec le seul événement `event`.
struct OneEvent : QuietContent {
  std::array<EventRule, 1> one{};
  explicit OneEvent(const EventRule& event) {
    one[0] = event;
    rules.events = one;
  }
};

}  // namespace

TEST_CASE("Événements survivors : l'essaim naît en file sur un côté tiré, et file droit",
          "[sim][survivors]") {
  constexpr std::uint8_t kCount = 8;
  const OneEvent tables({.at_ms = kEventMs,
                         .kind = EventKind::kSwarm,
                         .foe_id = kContentFamiliarId,
                         .count = kCount});
  SurvivorsRun run = StartRun(tables.rules);
  StepStill(run, kEventStep);
  CHECK(run.field().foes.empty());
  // Le flux des apparitions avance EXACTEMENT d'un Below(4) : le côté.
  ro::core::Pcg32 expected = run.field().spawn_random;
  const std::uint32_t side = expected.Below(kFrameSides);
  const FieldPoint hero = run.field().hero.position;
  StepStill(run);

  const SurvivorsField& field = run.field();
  CHECK(field.spawn_random == expected);
  REQUIRE(field.foes.size() == kCount);
  // Le nord et le sud sont des côtés en x, l'est et l'ouest en y.
  const bool along_x = side % 2 == 0;
  CHECK(OnContiguousLine(BirthCells(field), along_x));
  const FieldPoint middle = SideMiddle(hero, side);
  const Cell middle_cell{CellOf(middle.x), CellOf(middle.y)};
  for (const Cell& cell : BirthCells(field)) {
    CHECK((along_x ? cell.y == middle_cell.y : cell.x == middle_cell.x));
  }
  const std::uint8_t drift = HeadingOf(hero - middle, 0);
  for (const SurvivorsFoe& foe : field.foes) {
    CHECK(foe.foe_id == kContentFamiliarId);
    CHECK(foe.drift_heading == drift);
    // Le pas de sa naissance l'a déjà fait filer à ce cap.
    CHECK(foe.position != foe.previous_position);
  }
  // Une fois, pas deux.
  StepStill(run);
  CHECK(run.field().foes.size() == kCount);
  CHECK(run.field().next_event == 1U);
}

TEST_CASE("Événements survivors : le raid naît en colonne, du côté tiré vers l'extérieur",
          "[sim][survivors]") {
  constexpr std::uint8_t kCount = 6;
  const OneEvent tables(
      {.at_ms = kEventMs, .kind = EventKind::kRaid, .foe_id = kTinyLunaticId, .count = kCount});
  SurvivorsRun run = StartRun(tables.rules);
  StepStill(run, kEventStep);
  ro::core::Pcg32 expected = run.field().spawn_random;
  const std::uint32_t side = expected.Below(kFrameSides);
  const FieldPoint hero = run.field().hero.position;
  StepStill(run);

  const SurvivorsField& field = run.field();
  CHECK(field.spawn_random == expected);
  REQUIRE(field.foes.size() == kCount);
  // Une colonne perpendiculaire au côté : en y pour le nord et le sud.
  const bool along_x = side % 2 != 0;
  CHECK(OnContiguousLine(BirthCells(field), along_x));
  // Le premier né est au milieu du côté.
  const FieldPoint middle = SideMiddle(hero, side);
  CHECK(BirthCells(field).front() == Cell{CellOf(middle.x), CellOf(middle.y)});
  for (const SurvivorsFoe& foe : field.foes) {
    CHECK(foe.drift_heading == HeadingOf(hero - middle, 0));
  }
}

TEST_CASE("Événements survivors : l'anneau entoure le héros, sans tirage", "[sim][survivors]") {
  constexpr std::uint8_t kCount = 12;
  constexpr std::int32_t kRadius = Cells(6);
  const OneEvent tables({.at_ms = kEventMs,
                         .kind = EventKind::kRing,
                         .foe_id = kContentPupaId,
                         .count = kCount,
                         .radius_units = kRadius});

  SECTION("sur une arène ouverte, chacun à son cap") {
    SurvivorsRun run = StartRun(tables.rules);
    StepStill(run, kEventStep);
    const ro::core::Pcg32 before = run.field().spawn_random;
    const FieldPoint hero = run.field().hero.position;
    StepStill(run);
    const SurvivorsField& field = run.field();
    CHECK(field.spawn_random == before);
    REQUIRE(field.foes.size() == kCount);
    for (std::size_t member = 0; member < kCount; ++member) {
      const auto heading = static_cast<std::uint8_t>(member * ro::sim::kHeadingSteps / kCount);
      const FieldPoint wanted = hero + TurnVector(heading, kRadius);
      const SurvivorsFoe& foe = field.foes[member];
      CHECK(CellOf(foe.position.x) == CellOf(wanted.x));
      CHECK(CellOf(foe.position.y) == CellOf(wanted.y));
      CHECK(foe.drift_heading == kNoHeading);
    }
  }

  SECTION("une case murée sans voisine praticable est sautée") {
    // Un bloc de 3 × 3 cases murées autour du point du cap 0 (au nord).
    const auto side = static_cast<std::size_t>(kArenaSideCells);
    std::vector<std::uint8_t> passable(side * side, 1U);
    const std::int32_t hero_cell = kArenaSideCells / 2;
    const std::int32_t wall_y = hero_cell + kRadius / ro::sim::kSubunitsPerCell;
    for (std::int32_t y = wall_y - 1; y <= wall_y + 1; ++y) {
      for (std::int32_t x = hero_cell - 1; x <= hero_cell + 1; ++x) {
        passable[static_cast<std::size_t>(y) * side + static_cast<std::size_t>(x)] = 0U;
      }
    }
    SurvivorsRun run =
        StartRun(tables.rules, WalkMap(kArenaSideCells, kArenaSideCells, std::move(passable)));
    REQUIRE(CellOf(run.field().hero.position.x) == hero_cell);
    StepStill(run, kEventStep + 1);
    CHECK(run.field().foes.size() == kCount - 1U);
  }
}

TEST_CASE("Événements survivors : le groupe naît devant le héros, sans tirage",
          "[sim][survivors]") {
  constexpr std::uint8_t kCount = 3;
  const OneEvent tables({.at_ms = kEventMs,
                         .kind = EventKind::kGroup,
                         .foe_id = kContentThiefBugId,
                         .count = kCount});
  SurvivorsRun run = StartRun(tables.rules);
  StepStill(run, kEventStep);
  const ro::core::Pcg32 before = run.field().spawn_random;
  // Le héros regarde le nord (cap 0) depuis le départ.
  REQUIRE(run.field().hero.facing == 0);
  const FieldPoint middle = SideMiddle(run.field().hero.position, 0);
  StepStill(run);
  const SurvivorsField& field = run.field();
  CHECK(field.spawn_random == before);
  REQUIRE(field.foes.size() == kCount);
  for (const SurvivorsFoe& foe : field.foes) {
    CHECK(ro::sim::DistanceSquared(foe.previous_position, middle) <=
          std::int64_t{Cells(2)} * Cells(2));
    CHECK(foe.drift_heading == kNoHeading);
  }
}

TEST_CASE("Événements survivors : un événement que le plafond arrête reprend son reste",
          "[sim][survivors]") {
  constexpr std::uint16_t kMostFoes = 5;
  OneEvent tables(
      {.at_ms = kEventMs, .kind = EventKind::kSwarm, .foe_id = kContentFamiliarId, .count = 8});
  tables.rules.caps.most_foes = kMostFoes;
  SurvivorsRun run = StartRun(tables.rules);
  StepStill(run, kEventStep);
  ro::core::Pcg32 expected = run.field().spawn_random;
  (void)expected.Below(kFrameSides);
  StepStill(run);
  CHECK(run.field().foes.size() == kMostFoes);
  CHECK(run.field().spawn_random == expected);
  // Au plafond, l'événement attend sans tirer et sans être soldé.
  StepStill(run);
  CHECK(run.field().foes.size() == kMostFoes);
  CHECK(run.field().spawn_random == expected);
  CHECK(run.field().next_event == 0U);
  CHECK(run.field().event_members_placed == kMostFoes);

  // Trois places se libèrent : le reste naît, d'un côté tiré à nouveau, et
  // l'événement est soldé.
  constexpr std::size_t kFreed = 3;
  std::vector<SurvivorsFoe>& foes = run.field_for_tests().foes;
  foes.erase(foes.begin(), foes.begin() + kFreed);
  (void)expected.Below(kFrameSides);
  StepStill(run);
  CHECK(run.field().foes.size() == kMostFoes);
  CHECK(run.field().spawn_random == expected);
  CHECK(run.field().next_event == 1U);
  CHECK(run.field().event_members_placed == 0U);
  const std::size_t familiars = static_cast<std::size_t>(
      std::ranges::count(run.field().foes, kContentFamiliarId, &SurvivorsFoe::foe_id));
  CHECK(familiars == kMostFoes);

  // Soldé : une place libre ne fait plus rien naître.
  foes.erase(foes.begin());
  StepStill(run);
  CHECK(run.field().foes.size() == kMostFoes - 1U);
}

TEST_CASE("Événements survivors : un anneau arrêté reprend à son rang, le suivant attend",
          "[sim][survivors]") {
  constexpr std::uint16_t kMostFoes = 5;
  constexpr std::uint8_t kRingCount = 8;
  constexpr std::uint8_t kGroupCount = 3;
  constexpr std::int32_t kRadius = Cells(6);
  // Le groupe est dû une seconde après l'anneau, pendant que celui-ci attend.
  constexpr std::uint32_t kGroupDelayMs = 1000;
  QuietContent tables;
  const std::array<EventRule, 2> events{{
      {.at_ms = kEventMs,
       .kind = EventKind::kRing,
       .foe_id = kContentPupaId,
       .count = kRingCount,
       .radius_units = kRadius},
      {.at_ms = kEventMs + kGroupDelayMs,
       .kind = EventKind::kGroup,
       .foe_id = kContentThiefBugId,
       .count = kGroupCount},
  }};
  tables.rules.events = events;
  tables.rules.caps.most_foes = kMostFoes;
  SurvivorsRun run = StartRun(tables.rules);
  StepStill(run, kEventStep);
  const ro::core::Pcg32 before = run.field().spawn_random;
  const FieldPoint hero = run.field().hero.position;
  const auto count_of = [&](std::uint16_t foe_id) {
    return static_cast<std::size_t>(
        std::ranges::count(run.field().foes, foe_id, &SurvivorsFoe::foe_id));
  };

  StepStill(run);
  CHECK(count_of(kContentPupaId) == kMostFoes);
  CHECK(run.field().event_members_placed == kMostFoes);

  // Le groupe devient dû : il attend que l'anneau soit soldé.
  StepStill(run, kGroupDelayMs / ro::sim::kSurvivorsStepMs + 1);
  CHECK(count_of(kContentThiefBugId) == 0U);
  CHECK(run.field().next_event == 0U);
  CHECK(run.field().event_members_placed == kMostFoes);

  // Trois places : les rangs 5 à 7 de l'anneau naissent à leurs caps, sans
  // reprendre les premiers ; le groupe, dû, trouve la horde au plafond.
  constexpr std::size_t kFreed = kRingCount - kMostFoes;
  std::vector<SurvivorsFoe>& foes = run.field_for_tests().foes;
  foes.erase(foes.begin(), foes.begin() + kFreed);
  StepStill(run);
  const SurvivorsField& field = run.field();
  CHECK(field.spawn_random == before);
  REQUIRE(field.foes.size() == kMostFoes);
  CHECK(count_of(kContentPupaId) == kMostFoes);
  for (std::size_t member = kMostFoes; member < kRingCount; ++member) {
    const auto heading = static_cast<std::uint8_t>(member * ro::sim::kHeadingSteps / kRingCount);
    const FieldPoint wanted = hero + TurnVector(heading, kRadius);
    const SurvivorsFoe& foe = field.foes[member - kFreed];
    CHECK(CellOf(foe.position.x) == CellOf(wanted.x));
    CHECK(CellOf(foe.position.y) == CellOf(wanted.y));
  }
  CHECK(field.next_event == 1U);
  CHECK(field.event_members_placed == 0U);
  CHECK(count_of(kContentThiefBugId) == 0U);

  // Des places encore : le groupe naît, entier, une seule fois.
  foes.erase(foes.begin(), foes.begin() + kGroupCount);
  StepStill(run);
  CHECK(count_of(kContentThiefBugId) == kGroupCount);
  CHECK(run.field().next_event == 2U);
  CHECK(run.field().spawn_random == before);
}

TEST_CASE("Événements survivors : les élites naissent à l'heure, de la vague, majorées",
          "[sim][survivors]") {
  QuietContent tables;
  tables.rules.elites = {.first_ms = 20000,
                         .interval_ms = 10000,
                         .last_ms = 50000,
                         .hp_pct = 1000,
                         .touch_pct = 150,
                         .exp_pct = 500};
  SurvivorsRun run = StartRun(tables.rules, PillarArena(kArenaSideCells));
  // Un héros que les élites ne peuvent pas abattre en une minute.
  run.field_for_tests().hero.max_hp = kSturdyHeroHp;
  run.field_for_tests().hero.hp = kSturdyHeroHp;
  constexpr std::uint32_t kFirstStep = 20000 / ro::sim::kSurvivorsStepMs;
  constexpr std::uint32_t kIntervalSteps = 10000 / ro::sim::kSurvivorsStepMs;
  StepStill(run, kFirstStep);
  CHECK(run.field().foes.empty());
  StepStill(run);
  REQUIRE(run.field().foes.size() == 1U);

  // Une à chaque intervalle jusqu'à last_ms compris : 20, 30, 40, 50 s.
  StepStill(run, 3 * kIntervalSteps);
  REQUIRE(run.field().foes.size() == 4U);
  StepStill(run, 2 * kIntervalSteps);
  CHECK(run.field().foes.size() == 4U);

  for (const SurvivorsFoe& foe : run.field().foes) {
    CHECK(foe.elite);
    // L'espèce vient de la vague en vigueur.
    CHECK((foe.foe_id == kTinyLunaticId || foe.foe_id == kTinyPoringId));
    // Minute 0 : aucune croissance, PV × 10.
    const FoeRule* rule = FindFoe(tables.rules, foe.foe_id);
    REQUIRE(rule != nullptr);
    CHECK(foe.max_hp == rule->hp * 10);
  }
}

TEST_CASE("Événements survivors : une élite touche plus fort, rapporte cinq fois plus",
          "[sim][survivors]") {
  QuietContent tables;
  tables.rules.elites = {.first_ms = 0,
                         .interval_ms = 0,
                         .last_ms = 0,
                         .hp_pct = 1000,
                         .touch_pct = 150,
                         .exp_pct = 500};
  SurvivorsRun run = StartRun(tables.rules, PillarArena(kArenaSideCells));
  SurvivorsField& field = run.field_for_tests();
  const FoeRule* lunatic = FindFoe(tables.rules, kTinyLunaticId);
  REQUIRE(lunatic != nullptr);

  SECTION("contact à 150 %") {
    (void)PlaceFoe(field, kTinyLunaticId, field.hero.position);
    field.foes.back().elite = true;
    const std::int32_t hp = field.hero.hp;
    StepStill(run);
    // 3 × 150 / 100 = 4, tronqué.
    CHECK(hp - run.field().hero.hp == lunatic->touch_damage * 150 / 100);
  }

  SECTION("points × 5, XP × 5, compté parmi les élites tuées") {
    (void)PlaceFoe(field, kTinyLunaticId, field.hero.position + FieldPoint{Cells(6), 0});
    field.foes.back().elite = true;
    field.foes.back().hp = 0;
    StepStill(run);
    CHECK(run.tally().kills == 1U);
    CHECK(run.tally().elite_kills == 1U);
    CHECK(run.tally().kill_points == ro::sim::KillPoints(*lunatic, true));
    CHECK(ro::sim::KillPoints(*lunatic, true) == lunatic->ro_level * ro::sim::kEliteKillFactor);
    REQUIRE_FALSE(run.field().gems.empty());
    CHECK(run.field().gems.front().exp == lunatic->exp * 5);
  }

  SECTION("un commun reste au barème de base") {
    (void)PlaceFoe(field, kTinyLunaticId, field.hero.position + FieldPoint{Cells(6), 0});
    field.foes.back().hp = 0;
    StepStill(run);
    CHECK(run.tally().elite_kills == 0U);
    CHECK(run.tally().kill_points == lunatic->ro_level);
  }
}

TEST_CASE("Événements survivors : une meute naît d'un tirage, et compte six au plafond",
          "[sim][survivors]") {
  static constexpr std::array<std::uint16_t, 1> kWolves{kContentWolfId};
  static constexpr std::array<std::uint32_t, 1> kWeights{1};
  constexpr std::array<ro::sim::WaveRule, 1> kWolfWave{{
      {.from_ms = 0,
       .foe_ids = kWolves,
       .weights = kWeights,
       .floor = 0,
       .interval_ms = 1000,
       .batch = 1},
  }};
  QuietContent tables;
  tables.rules.waves = kWolfWave;

  SECTION("un lot d'un tirage fait six loups, au même point") {
    SurvivorsRun run = StartRun(tables.rules, PillarArena(kArenaSideCells));
    StepStill(run, kFirstBatchStep);
    CHECK(run.field().foes.empty());
    StepStill(run);
    const SurvivorsField& field = run.field();
    REQUIRE(field.foes.size() == 6U);
    for (const SurvivorsFoe& foe : field.foes) {
      CHECK(foe.foe_id == kContentWolfId);
      CHECK(foe.previous_position == field.foes.front().previous_position);
    }
    // Le lot est accompli : rien avant l'intervalle suivant.
    StepStill(run);
    CHECK(run.field().foes.size() == 6U);
  }

  SECTION("le plafond tronque la meute") {
    tables.rules.caps.most_foes = 4;
    SurvivorsRun run = StartRun(tables.rules, PillarArena(kArenaSideCells));
    StepStill(run, kFirstBatchStep + 1);
    CHECK(run.field().foes.size() == 4U);
  }
}

TEST_CASE("Événements survivors : les PV et la marche des boss suivent leurs réglages",
          "[sim][survivors]") {
  constexpr std::uint32_t kBossMs = 60000;  // minute 1
  QuietContent tables;
  std::array<ro::sim::BossRule, 1> bosses{{{.at_ms = kBossMs, .foe_id = kContentBaphometId}}};
  tables.rules.bosses = bosses;
  tables.rules.run_ms = 2 * kBossMs;
  tables.rules.tuning.boss_hp_growth_pct_per_minute = 0;
  tables.rules.tuning.boss_walk_scale_pct = 200;
  const FoeRule* baphomet = FindFoe(tables.rules, kContentBaphometId);
  REQUIRE(baphomet != nullptr);
  constexpr std::uint32_t kBossStep = kBossMs / ro::sim::kSurvivorsStepMs;

  SECTION("PV fixes, marche à l'échelle des boss") {
    SurvivorsRun run = StartRun(tables.rules, PillarArena(kArenaSideCells));
    StepStill(run, kBossStep + 1);
    REQUIRE(run.field().foes.size() == 1U);
    const SurvivorsFoe& boss = run.field().foes.front();
    CHECK(boss.max_hp == baphomet->hp);
    CHECK(boss.units_per_step == ro::sim::UnitsPerStep(baphomet->ro_walk_ms_per_cell * 2));
  }

  SECTION("la marche propre d'un boss l'emporte, telle quelle") {
    constexpr std::uint32_t kOwnWalkMs = 200;
    bosses[0].walk_ms_per_cell = kOwnWalkMs;
    tables.rules.tuning.boss_hp_growth_pct_per_minute = 15;
    SurvivorsRun run = StartRun(tables.rules, PillarArena(kArenaSideCells));
    StepStill(run, kBossStep + 1);
    REQUIRE(run.field().foes.size() == 1U);
    const SurvivorsFoe& boss = run.field().foes.front();
    CHECK(boss.units_per_step == ro::sim::UnitsPerStep(kOwnWalkMs));
    CHECK(boss.max_hp == baphomet->hp * 115 / 100);
  }
}
