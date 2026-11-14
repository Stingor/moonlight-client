#include <algorithm>
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

using ro::sim::CellCenter;
using ro::sim::CellOf;
using ro::sim::Cells;
using ro::sim::FieldPoint;
using ro::sim::HazardKind;
using ro::sim::SurvivorsField;
using ro::sim::SurvivorsFoe;
using ro::sim::SurvivorsHazard;
using ro::sim::SurvivorsNews;
using ro::sim::SurvivorsRun;
using ro::sim::SurvivorsStart;
using ro::sim::SurvivorsStat;
using ro::sim::SurvivorsWarning;
using ro::sim::TurnVector;
using ro::sim::WalkMap;
using ro::sim::WarningKind;
using ro::test_support::ContentTables;
using ro::test_support::Heading;
using ro::test_support::kContentArcherId;
using ro::test_support::kContentBathoryId;
using ro::test_support::kContentFamiliarId;
using ro::test_support::kContentPoporingId;
using ro::test_support::kContentSavageId;
using ro::test_support::kContentSmokieId;
using ro::test_support::kContentThiefBugId;
using ro::test_support::kTinyLunaticId;
using ro::test_support::PillarArena;
using ro::test_support::PlaceFoe;
using ro::test_support::SetBonus;
using ro::test_support::StepStill;
using ro::test_support::TinyStart;

namespace {

constexpr std::int32_t kArenaSideCells = 64;
constexpr std::size_t kFamiliarIndex = 3;
// Le cap 16 pointe à l'ouest, le cap 0 au nord.
constexpr std::uint8_t kWest = 16;
constexpr std::uint8_t kNorth = 0;
// Les mesures des comportements de la fixture de contenu, en pas.
constexpr std::uint32_t kTelegraph500Steps = 25;
constexpr std::uint32_t kTelegraph300Steps = 15;
constexpr std::uint32_t kSavagePeriodSteps = 250;
constexpr std::uint32_t kSmokiePeriodSteps = 300;
constexpr std::uint32_t kSmokieHideSteps = 50;
constexpr std::uint32_t kPhaseFactor = 7;
// Une charge à 16 cases par seconde : 327 sous-unités par pas.
constexpr std::int32_t kSavageDashPerStep = Cells(16) / 50;
// Une flèche à 10 cases par seconde : 204 sous-unités par pas.
constexpr std::int32_t kArrowPerStep = Cells(10) / 50;
constexpr std::int32_t kArrowDamage = 14;
constexpr std::int32_t kPuddleDamage = 5;
constexpr std::uint32_t kPuddlePeriodSteps = 50;
constexpr std::uint32_t kPuddleLifeSteps = 200;

// Les règles de contenu, silencieuses : ni vague, ni événement, ni boss, ni
// élite, ni plante. Un essai y pose ses monstres lui-même.
struct QuietContent : ContentTables {
  QuietContent() {
    rules.waves = ro::test_support::kQuietWaves;
    rules.events = {};
    rules.bosses = {};
    rules.elites = {};
    rules.plants = {};
  }
};

[[nodiscard]] SurvivorsRun StartRun(const ro::sim::SurvivorsRules& rules,
                                    SurvivorsStart start = TinyStart(1),
                                    WalkMap arena = PillarArena(kArenaSideCells)) {
  auto run = SurvivorsRun::Start(rules, std::move(arena), start);
  REQUIRE(run.has_value());
  return std::move(*run);
}

// Un héros sans skill, qui ne tue rien, après un premier pas : `step` vaut 1,
// pour qu'un `act_ready_step` posé par l'essai ne vaille jamais 0.
[[nodiscard]] SurvivorsRun UnarmedRun(const ro::sim::SurvivorsRules& rules,
                                      SurvivorsStart start = TinyStart(1),
                                      WalkMap arena = PillarArena(kArenaSideCells)) {
  SurvivorsRun run = StartRun(rules, start, std::move(arena));
  run.field_for_tests().hero.skills.clear();
  StepStill(run);
  (void)run.TakeNews();
  return run;
}

// Une arène ouverte, sauf un mur sur la colonne `kLongWallColumn`, des rangées
// `kLongWallFirstRow` à `kLongWallLastRow`.
constexpr std::int32_t kLongWallColumn = 30;
constexpr std::int32_t kLongWallFirstRow = 22;
constexpr std::int32_t kLongWallLastRow = 38;

[[nodiscard]] WalkMap LongWallArena() {
  const auto side = static_cast<std::size_t>(kArenaSideCells);
  std::vector<std::uint8_t> passable(side * side, 1U);
  for (std::int32_t row = kLongWallFirstRow; row <= kLongWallLastRow; ++row) {
    passable[static_cast<std::size_t>(row) * side + static_cast<std::size_t>(kLongWallColumn)] = 0U;
  }
  return WalkMap(kArenaSideCells, kArenaSideCells, std::move(passable));
}

// Le héros posé au centre de la case (x, y).
void PutHero(SurvivorsField& field, std::int32_t cell_x, std::int32_t cell_y) {
  field.hero.position = CellCenter(cell_x, cell_y);
  field.hero.previous_position = field.hero.position;
}

// Un monstre `foe_id` à `offset` du héros, dont l'acte est prêt à ce pas.
std::uint32_t PlaceReady(SurvivorsField& field, std::uint16_t foe_id, FieldPoint offset) {
  const std::uint32_t serial = PlaceFoe(field, foe_id, field.hero.position + offset);
  field.foes.back().act_ready_step = field.step;
  return serial;
}

[[nodiscard]] FieldPoint East(std::int32_t units) { return FieldPoint{units, 0}; }
[[nodiscard]] FieldPoint West(std::int32_t units) { return FieldPoint{-units, 0}; }

[[nodiscard]] const SurvivorsFoe* FindSerial(const SurvivorsField& field, std::uint32_t serial) {
  const auto found = std::find_if(field.foes.begin(), field.foes.end(),
                                  [&](const SurvivorsFoe& foe) { return foe.serial == serial; });
  return found != field.foes.end() ? &*found : nullptr;
}

[[nodiscard]] const SurvivorsFoe& FoeOf(const SurvivorsField& field, std::uint32_t serial) {
  const SurvivorsFoe* foe = FindSerial(field, serial);
  REQUIRE(foe != nullptr);
  return *foe;
}

[[nodiscard]] bool OnWall(const SurvivorsField& field, FieldPoint at) {
  return !field.ground.Passable(CellOf(at.x), CellOf(at.y));
}

// Un pas sans cap, et ses nouvelles.
[[nodiscard]] SurvivorsNews StepNews(SurvivorsRun& run) {
  StepStill(run);
  return run.TakeNews();
}

}  // namespace

// ── Les actes ───────────────────────────────────────────────────────────────

TEST_CASE("Comportements survivors : la charge s'annonce, file au pas annoncé, puis recharge",
          "[sim][survivors]") {
  QuietContent tables;
  SurvivorsRun run = UnarmedRun(tables.rules);
  SurvivorsField& field = run.field_for_tests();
  const std::uint32_t announced = field.step;
  const std::uint32_t savage = PlaceReady(field, kContentSavageId, East(Cells(5)));
  const FieldPoint start = FoeOf(field, savage).position;

  const SurvivorsNews first = StepNews(run);
  REQUIRE(first.warnings.size() == 1);
  const SurvivorsWarning& warning = first.warnings[0];
  CHECK(warning.kind == WarningKind::kCharge);
  CHECK(warning.source_serial == savage);
  CHECK(warning.from == start);
  CHECK(warning.strike_step == announced + kTelegraph500Steps);
  // Sans active_ms, la charge couvre sa portée : 19 pas de 327 sous-unités.
  const std::int32_t dash_steps = (Cells(6) + kSavageDashPerStep - 1) / kSavageDashPerStep;
  CHECK(warning.to == start + TurnVector(kWest, kSavageDashPerStep * dash_steps));
  CHECK(FoeOf(field, savage).act_ready_step == announced + kSavagePeriodSteps);

  // Rien ne bouge pendant l'annonce : l'acte ne joue pas avant son pas.
  while (field.step < warning.strike_step) {
    CHECK(StepNews(run).warnings.empty());
    CHECK(FoeOf(field, savage).position == start);
  }
  // Au pas annoncé, le premier bond de la charge, droit à l'ouest.
  StepStill(run);
  CHECK(FoeOf(field, savage).position == start + TurnVector(kWest, kSavageDashPerStep));
  CHECK(FoeOf(field, savage).act_until_step == warning.strike_step + dash_steps);

  // Aucune autre annonce avant la période, une au pas de la période.
  std::size_t later_warnings = 0;
  while (field.step < announced + kSavagePeriodSteps) {
    later_warnings += StepNews(run).warnings.size();
  }
  CHECK(later_warnings == 0);
  CHECK(StepNews(run).warnings.size() == 1);
}

TEST_CASE("Comportements survivors : le tireur annonce, tire au pas annoncé, la flèche touche",
          "[sim][survivors]") {
  QuietContent tables;
  SurvivorsRun run = UnarmedRun(tables.rules);
  SurvivorsField& field = run.field_for_tests();
  const std::uint32_t announced = field.step;
  const std::uint32_t archer = PlaceReady(field, kContentArcherId, East(Cells(6)));
  const FieldPoint start = FoeOf(field, archer).position;
  const FieldPoint hero = field.hero.position;
  const std::int32_t hp = field.hero.hp;

  const SurvivorsNews first = StepNews(run);
  REQUIRE(first.warnings.size() == 1);
  CHECK(first.warnings[0].kind == WarningKind::kShot);
  CHECK(first.warnings[0].to == hero);
  CHECK(first.warnings[0].radius_units == ro::sim::CellTenths(3));
  CHECK(first.warnings[0].strike_step == announced + kTelegraph500Steps);
  while (field.step < announced + kTelegraph500Steps) {
    StepStill(run);
    CHECK(field.foe_shots.empty());
    CHECK(FoeOf(field, archer).position == start);
  }
  // Tirée au pas annoncé, la flèche a déjà fait son premier pas.
  StepStill(run);
  REQUIRE(field.foe_shots.size() == 1);
  CHECK(field.foe_shots[0].from_serial == archer);
  CHECK(field.foe_shots[0].position == start + TurnVector(kWest, kArrowPerStep));

  // Elle touche le héros une fois, puis s'éteint.
  for (std::uint32_t step = 0; step < 40 && !field.foe_shots.empty(); ++step) StepStill(run);
  CHECK(field.foe_shots.empty());
  CHECK(field.hero.hp == hp - kArrowDamage);
}

TEST_CASE("Comportements survivors : une flèche esquivée s'éteint au bout de sa vie",
          "[sim][survivors]") {
  QuietContent tables;
  SurvivorsRun run = UnarmedRun(tables.rules);
  SurvivorsField& field = run.field_for_tests();
  const std::uint32_t announced = field.step;
  (void)PlaceReady(field, kContentArcherId, East(Cells(6)));
  const std::int32_t hp = field.hero.hp;
  StepStill(run, kTelegraph500Steps + 1);
  REQUIRE(field.foe_shots.size() == 1);
  // La portée plus une case, sur la vitesse : 35 pas de vie.
  const std::uint32_t expires = announced + kTelegraph500Steps + (Cells(7) / kArrowPerStep);
  CHECK(field.foe_shots[0].expires_step == expires);
  // Le héros file au nord, hors de la ligne de tir.
  while (field.step < expires) {
    REQUIRE(field.foe_shots.size() == 1);
    run.Step(Heading(kNorth));
  }
  // Au pas de sa fin de vie, elle s'éteint sans avoir touché.
  run.Step(Heading(kNorth));
  CHECK(field.foe_shots.empty());
  CHECK(field.hero.hp == hp);
}

TEST_CASE("Comportements survivors : Bathory saute de trois cases, jamais sur un mur",
          "[sim][survivors]") {
  QuietContent tables;
  SECTION("en terrain libre, trois cases vers le héros, au pas annoncé") {
    SurvivorsRun run = UnarmedRun(tables.rules);
    SurvivorsField& field = run.field_for_tests();
    const std::uint32_t announced = field.step;
    const std::uint32_t bathory = PlaceReady(field, kContentBathoryId, East(Cells(8)));
    const FieldPoint start = FoeOf(field, bathory).position;
    const SurvivorsNews first = StepNews(run);
    REQUIRE(first.warnings.size() == 1);
    CHECK(first.warnings[0].kind == WarningKind::kLeap);
    CHECK(first.warnings[0].to == start + TurnVector(kWest, Cells(3)));
    while (field.step < announced + kTelegraph300Steps) {
      StepStill(run);
      CHECK(FoeOf(field, bathory).position == start);
    }
    StepStill(run);
    CHECK(FoeOf(field, bathory).position == start + TurnVector(kWest, Cells(3)));
  }
  SECTION("une case plus près quand trois cases tombent dans un pilier") {
    SurvivorsRun run = UnarmedRun(tables.rules);
    SurvivorsField& field = run.field_for_tests();
    // Sur la rangée 30, le pilier occupe les colonnes 42 et 43.
    PutHero(field, 36, 30);
    const std::uint32_t bathory = PlaceReady(field, kContentBathoryId, FieldPoint{});
    field.foes.back().position = CellCenter(46, 30);
    field.foes.back().previous_position = CellCenter(46, 30);
    const SurvivorsNews first = StepNews(run);
    REQUIRE(first.warnings.size() == 1);
    CHECK(first.warnings[0].to == CellCenter(46, 30) + TurnVector(kWest, Cells(2)));
    for (std::uint32_t step = 0; step <= kTelegraph300Steps; ++step) {
      StepStill(run);
      CHECK_FALSE(OnWall(field, FoeOf(field, bathory).position));
    }
    CHECK(CellOf(FoeOf(field, bathory).position.x) == 44);
  }
}

// ── La cachette ─────────────────────────────────────────────────────────────

TEST_CASE("Comportements survivors : Smokie caché n'est ni visé, ni touché, ni touchant",
          "[sim][survivors]") {
  QuietContent tables;
  SurvivorsRun run = StartRun(tables.rules);
  SurvivorsField& field = run.field_for_tests();
  StepStill(run);
  (void)run.TakeNews();
  // Au contact du héros, qui garde sa frappe.
  const std::uint32_t hidden_from = field.step;
  const std::uint32_t smokie = PlaceReady(field, kContentSmokieId, East(Cells(1) / 2));
  const std::int32_t hero_hp = field.hero.hp;
  const std::int32_t smokie_hp = FoeOf(field, smokie).hp;
  while (field.step < hidden_from + kSmokieHideSteps) {
    const SurvivorsNews news = StepNews(run);
    for (const ro::sim::SurvivorsCast& cast : news.casts) CHECK(cast.target_serial != smokie);
    CHECK(news.hits.empty());
    CHECK(FoeOf(field, smokie).hp == smokie_hp);
    CHECK(field.hero.hp == hero_hp);
  }
  CHECK(FoeOf(field, smokie).act_until_step == hidden_from + kSmokieHideSteps);
  // Ressorti, il touche et se fait viser.
  bool targeted = false;
  for (std::uint32_t step = 0; step < 5; ++step) {
    const SurvivorsNews news = StepNews(run);
    for (const ro::sim::SurvivorsCast& cast : news.casts) {
      targeted = targeted || cast.target_serial == smokie;
    }
  }
  CHECK(targeted);
  CHECK(field.hero.hp < hero_hp);
}

TEST_CASE("Comportements survivors : deux Smokie de serials voisins se cachent déphasés",
          "[sim][survivors]") {
  QuietContent tables;
  SurvivorsRun run = UnarmedRun(tables.rules);
  SurvivorsField& field = run.field_for_tests();
  const std::uint32_t born = field.step;
  // Loin du héros et l'un de l'autre : rien ne les touche.
  const std::uint32_t first =
      PlaceFoe(field, kContentSmokieId, field.hero.position + East(Cells(9)));
  const std::uint32_t second =
      PlaceFoe(field, kContentSmokieId, field.hero.position + West(Cells(9)));
  REQUIRE(second == first + 1);
  // La phase de départ, sans aléa : born_step + (serial × 7) % période.
  const std::uint32_t first_hide = born + (first * kPhaseFactor) % kSmokiePeriodSteps;
  const std::uint32_t second_hide = born + (second * kPhaseFactor) % kSmokiePeriodSteps;
  std::uint32_t seen_first = 0;
  std::uint32_t seen_second = 0;
  while (field.step <= born + kSmokiePeriodSteps && (seen_first == 0 || seen_second == 0)) {
    const std::uint32_t step = field.step;
    StepStill(run);
    if (seen_first == 0 && FoeOf(field, first).act_until_step == step + kSmokieHideSteps) {
      seen_first = step;
    }
    if (seen_second == 0 && FoeOf(field, second).act_until_step == step + kSmokieHideSteps) {
      seen_second = step;
    }
  }
  CHECK(seen_first == first_hide);
  CHECK(seen_second == second_hide);
  CHECK(seen_first != seen_second);
}

// ── Le voleur ───────────────────────────────────────────────────────────────

TEST_CASE("Comportements survivors : Thief Bug vole une gemme et rend l'XP exacte",
          "[sim][survivors]") {
  QuietContent tables;
  SurvivorsRun run = UnarmedRun(tables.rules);
  SurvivorsField& field = run.field_for_tests();
  constexpr std::uint32_t kStolenExp = 7;
  const FieldPoint gem_at = field.hero.position + East(Cells(10));
  field.gems.push_back(ro::sim::SurvivorsGem{
      .position = gem_at, .previous_position = gem_at, .exp = kStolenExp, .pull_units = 0});
  const std::uint32_t thief = PlaceFoe(field, kContentThiefBugId, gem_at + East(Cells(2)));
  for (std::uint32_t step = 0; step < 60 && !field.gems.empty(); ++step) StepStill(run);
  REQUIRE(field.gems.empty());
  CHECK(FoeOf(field, thief).carried_exp == kStolenExp);
  CHECK(field.tally.exp_total == 0);

  // À sa mort, sa gemme, puis une gemme de l'XP volée.
  for (SurvivorsFoe& foe : field.foes) foe.hp = 0;
  StepStill(run);
  REQUIRE(field.gems.size() == 2);
  CHECK(field.gems[0].exp == tables.foes[4].exp);
  CHECK(field.gems[1].exp == kStolenExp);
}

// ── Les menaces au sol ──────────────────────────────────────────────────────

TEST_CASE("Comportements survivors : la flaque blesse par période, armure comprise, au moins 1",
          "[sim][survivors]") {
  QuietContent tables;
  struct Case {
    std::int32_t armor = 0;
    std::int32_t per_hurt = 0;
  };
  for (const Case wanted : {Case{0, kPuddleDamage}, Case{3, 2}, Case{10, 1}}) {
    SurvivorsStart start = TinyStart(1);
    SetBonus(start, SurvivorsStat::kArmor, wanted.armor);
    SurvivorsRun run = UnarmedRun(tables.rules, start);
    SurvivorsField& field = run.field_for_tests();
    const std::uint32_t fell = field.step;
    (void)PlaceFoe(field, kContentPoporingId, field.hero.position + East(Cells(1)));
    field.foes.back().hp = 0;
    const std::int32_t hp = field.hero.hp;
    std::uint32_t hurts = 0;
    while (field.step < fell + kPuddleLifeSteps + 1) {
      const std::uint32_t step = field.step;
      const SurvivorsNews news = StepNews(run);
      if (news.hero_hurt != 0) {
        CHECK(news.hero_hurt == wanted.per_hurt);
        CHECK((step - fell) % kPuddlePeriodSteps == 0);
        ++hurts;
      }
    }
    CHECK(hurts == kPuddleLifeSteps / kPuddlePeriodSteps);
    CHECK(field.hazards.empty());
    CHECK(field.hero.hp == hp - static_cast<std::int32_t>(hurts) * wanted.per_hurt);
  }
}

TEST_CASE("Comportements survivors : un séisme ne blesse qu'au pas où il s'arme",
          "[sim][survivors]") {
  QuietContent tables;
  SurvivorsRun run = UnarmedRun(tables.rules);
  SurvivorsField& field = run.field_for_tests();
  constexpr std::uint32_t kArmingSteps = 10;
  constexpr std::int32_t kQuakeDamage = 40;
  const std::uint32_t armed = field.step + kArmingSteps;
  field.hazards.push_back(SurvivorsHazard{.kind = HazardKind::kQuake,
                                          .center = field.hero.position,
                                          .radius_units = ro::sim::CellTenths(25),
                                          .damage = kQuakeDamage,
                                          .armed_step = armed,
                                          .until_step = armed + 1,
                                          .period_steps = 0,
                                          .next_hurt_step = armed});
  const std::int32_t hp = field.hero.hp;
  while (field.step < armed) {
    StepStill(run);
    CHECK(field.hero.hp == hp);
  }
  StepStill(run);
  CHECK(field.hero.hp == hp - kQuakeDamage);
  StepStill(run, 3);
  CHECK(field.hazards.empty());
  CHECK(field.hero.hp == hp - kQuakeDamage);
}

// ── Les marches ─────────────────────────────────────────────────────────────

TEST_CASE("Comportements survivors : le volant passe au-dessus de la horde", "[sim][survivors]") {
  for (const bool over_horde : {true, false}) {
    QuietContent tables;
    if (!over_horde) tables.foes[kFamiliarIndex].behaviour.traits = 0;
    SurvivorsRun run = UnarmedRun(tables.rules);
    SurvivorsField& field = run.field_for_tests();
    const FieldPoint spot = field.hero.position + East(Cells(5));
    (void)PlaceFoe(field, kTinyLunaticId, spot);
    field.foes.back().units_per_step = 0;
    const std::uint32_t familiar = PlaceFoe(field, kContentFamiliarId, spot + FieldPoint{0, 8});
    StepStill(run);
    CHECK(field.foes[0].position == spot);
    const SurvivorsFoe& flyer = FoeOf(field, familiar);
    const std::int64_t moved = ro::sim::DistanceSquared(flyer.position, flyer.previous_position);
    const std::int64_t pace = flyer.units_per_step + 1;
    // Au-dessus de la horde, son seul pas de vol ; dedans, la séparation l'écarte.
    CHECK((moved <= pace * pace) == over_horde);
  }
}

TEST_CASE("Comportements survivors : Whisper traverse un pilier, un volant ordinaire non",
          "[sim][survivors]") {
  for (const bool passes_walls : {true, false}) {
    QuietContent tables;
    if (passes_walls) tables.foes[kFamiliarIndex].behaviour.traits |= ro::sim::kFoePassesWalls;
    SurvivorsRun run = UnarmedRun(tables.rules);
    SurvivorsField& field = run.field_for_tests();
    // Sur la rangée 30, le pilier occupe les colonnes 30 et 31.
    PutHero(field, 34, 30);
    const std::uint32_t whisper = PlaceFoe(field, kContentFamiliarId, CellCenter(26, 30));
    bool crossed_wall = false;
    for (std::uint32_t step = 0; step < 120; ++step) {
      StepStill(run);
      const SurvivorsFoe* foe = FindSerial(field, whisper);
      if (foe == nullptr) break;
      crossed_wall = crossed_wall || OnWall(field, foe->position);
    }
    CHECK(crossed_wall == passes_walls);
  }
}

TEST_CASE("Comportements survivors : un volant et un voleur contournent un mur long",
          "[sim][survivors]") {
  // Un mur qui coupe la ligne du but rend le monstre à la poursuite, que le
  // champ guide autour du mur : ni le volant ni le voleur ne restent butés.
  constexpr std::uint32_t kMostSteps = 1500;
  constexpr std::int32_t kWestCellX = 26;
  constexpr std::int32_t kRowY = 30;
  SECTION("le volant atteint le héros") {
    QuietContent tables;
    tables.foes[kFamiliarIndex].behaviour.traits = 0;
    SurvivorsRun run = UnarmedRun(tables.rules, TinyStart(1), LongWallArena());
    SurvivorsField& field = run.field_for_tests();
    PutHero(field, 34, kRowY);
    const std::uint32_t familiar =
        PlaceFoe(field, kContentFamiliarId, CellCenter(kWestCellX, kRowY));
    const std::int64_t near = Cells(2);
    bool reached = false;
    for (std::uint32_t step = 0; step < kMostSteps && !reached; ++step) {
      StepStill(run);
      const SurvivorsFoe& flyer = FoeOf(field, familiar);
      CHECK_FALSE(OnWall(field, flyer.position));
      reached = ro::sim::DistanceSquared(flyer.position, field.hero.position) <= near * near;
    }
    CHECK(reached);
  }
  SECTION("le voleur atteint la gemme") {
    QuietContent tables;
    SurvivorsRun run = UnarmedRun(tables.rules, TinyStart(1), LongWallArena());
    SurvivorsField& field = run.field_for_tests();
    PutHero(field, 44, kRowY);
    constexpr std::uint32_t kStolenExp = 7;
    const FieldPoint gem_at = CellCenter(34, kRowY);
    field.gems.push_back(ro::sim::SurvivorsGem{
        .position = gem_at, .previous_position = gem_at, .exp = kStolenExp, .pull_units = 0});
    const std::uint32_t thief = PlaceFoe(field, kContentThiefBugId, CellCenter(kWestCellX, kRowY));
    for (std::uint32_t step = 0; step < kMostSteps && !field.gems.empty(); ++step) StepStill(run);
    REQUIRE(field.gems.empty());
    CHECK(FoeOf(field, thief).carried_exp == kStolenExp);
  }
}

TEST_CASE("Comportements survivors : un essaim file droit et part au loin sans gemme",
          "[sim][survivors]") {
  QuietContent tables;
  tables.rules.tuning.recall_units = Cells(10);
  SurvivorsRun run = UnarmedRun(tables.rules);
  SurvivorsField& field = run.field_for_tests();
  const std::uint32_t drifter =
      PlaceFoe(field, kTinyLunaticId, field.hero.position + West(Cells(8)));
  field.foes.back().drift_heading = kWest;
  const std::int32_t row = field.foes.back().position.y;
  std::uint32_t steps = 0;
  while (FindSerial(field, drifter) != nullptr && steps < 100) {
    const FieldPoint before = FoeOf(field, drifter).position;
    StepStill(run);
    ++steps;
    if (const SurvivorsFoe* foe = FindSerial(field, drifter); foe != nullptr) {
      CHECK(foe->position.y == row);
      CHECK(foe->position.x == before.x - foe->units_per_step);
    }
  }
  CHECK(FindSerial(field, drifter) == nullptr);
  CHECK(steps < 100);
  CHECK(field.gems.empty());
  CHECK(field.tally.kills == 0);
}
