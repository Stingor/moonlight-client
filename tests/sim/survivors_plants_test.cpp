// Les plantes au fil des tués : cinq au départ, une juste hors de l'écran par
// compte de tués de la minute en cours, huit en vie au plus, perdues au loin.
// Les rulesets antérieurs gardent leurs plantes posées par blocs.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/survivors_bot.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_play.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/survivors_state.h"
#include "ro/sim/walk_map.h"

#include "support/survivors_fixture.h"
#include "support/survivors_golden.h"
#include "support/survivors_terrain_fixture.h"

using ro::sim::CellOf;
using ro::sim::Cells;
using ro::sim::FieldPoint;
using ro::sim::kStepsPerMinute;
using ro::sim::SurvivorsField;
using ro::sim::SurvivorsInput;
using ro::sim::SurvivorsPlant;
using ro::sim::SurvivorsRules;
using ro::sim::SurvivorsRun;

namespace {

constexpr std::uint64_t kSeed = 20261028;
// Les naissances observées par arène dans l'essai du cadre.
constexpr int kBirthsWatched = 40;

[[nodiscard]] std::int64_t Chebyshev(FieldPoint a, FieldPoint b) {
  const std::int64_t dx = std::llabs(std::int64_t{a.x} - b.x);
  const std::int64_t dy = std::llabs(std::int64_t{a.y} - b.y);
  return std::max(dx, dy);
}

// Strictement dans le cadre d'apparition centré sur `hero` : à l'écran.
[[nodiscard]] bool OnScreen(const SurvivorsRules& rules, FieldPoint hero, FieldPoint point) {
  const ro::sim::SpawnFrame& frame = rules.spawn_frame;
  const std::int64_t dx = std::int64_t{point.x} - hero.x;
  const std::int64_t dy = std::int64_t{point.y} - hero.y;
  return dx > -frame.half_width_units && dx < frame.half_width_units && dy > -frame.south_units &&
         dy < frame.north_units;
}

// Les règles livrées sur l'arène à piliers, plantes livrées comprises : la
// carte de l'autre arène du ruleset livré.
[[nodiscard]] const SurvivorsRules& ShippedOnPillars() {
  static const SurvivorsRules kRules = [] {
    SurvivorsRules rules = ro::sim::ShippedSurvivorsRules();
    rules.arenas = ro::test_support::kGoldenArenas;
    return rules;
  }();
  return kRules;
}

[[nodiscard]] SurvivorsRun StartGenerated() {
  auto run =
      SurvivorsRun::Start(ro::test_support::TerrainRules(), ro::test_support::TerrainStart(kSeed));
  REQUIRE(run.has_value());
  return std::move(*run);
}

// Le ruleset livré ne plafonne pas les plantes ; l'équilibrage peut en régler
// un, que ces règles posent.
constexpr std::uint8_t kBalancingCap = 8;
[[nodiscard]] const SurvivorsRules& CappedTerrainRules() {
  static const SurvivorsRules kRules = [] {
    SurvivorsRules rules = ro::test_support::TerrainRules();
    rules.tuning.most_live_plants = kBalancingCap;
    return rules;
  }();
  return kRules;
}

[[nodiscard]] SurvivorsRun StartOnPillars() {
  auto run = SurvivorsRun::Start(ShippedOnPillars(),
                                 ro::test_support::PillarArena(ro::test_support::kGoldenArenaCells),
                                 ro::test_support::GoldenStart(kSeed));
  REQUIRE(run.has_value());
  return std::move(*run);
}

// UN PAS AU COMPTE DONNÉ : la partie au pas `step`, sans monstre ni plante,
// `kills` tués dont `mark` avant la dernière plante. Le héros reste sur place,
// et rien ne meurt de ce pas : les monstres neufs naissent hors de l'écran.
// Rend vrai si une plante est née.
[[nodiscard]] bool StepWith(SurvivorsRun& run, std::uint32_t step, std::uint32_t kills,
                            std::uint32_t mark) {
  SurvivorsField& field = run.field_for_tests();
  field.step = step;
  field.foes.clear();
  field.plants.clear();
  field.tally.kills = kills;
  field.plant_kills_mark = mark;
  run.Step(SurvivorsInput{});
  REQUIRE(run.field().tally.kills == kills);
  return !run.field().plants.empty();
}

}  // namespace

TEST_CASE("Plantes au fil des tués : cinq au départ, à l'écran, sur des cases libres",
          "[sim][survivors]") {
  for (const bool generated : {true, false}) {
    INFO((generated ? "sol généré" : "carte à piliers"));
    const SurvivorsRun run = generated ? StartGenerated() : StartOnPillars();
    const SurvivorsField& field = run.field();
    const ro::sim::SurvivorsTuning& tuning = field.rules->tuning;
    // Aucune plante par blocs : seules celles du départ.
    REQUIRE(field.plants.size() == tuning.plant_start_count);
    REQUIRE(field.plants.size() == 5U);
    const std::int64_t least = Cells(tuning.plant_start_cells - tuning.spawn_search_ring);
    const std::int64_t most = Cells(tuning.plant_start_cells + tuning.spawn_search_ring);
    for (const SurvivorsPlant& plant : field.plants) {
      CHECK(field.ground.Passable(CellOf(plant.position.x), CellOf(plant.position.y)));
      CHECK(OnScreen(*field.rules, field.hero.position, plant.position));
      CHECK(Chebyshev(plant.position, field.hero.position) >= least);
      CHECK(Chebyshev(plant.position, field.hero.position) <= most);
      CHECK(
          std::any_of(field.rules->plants.begin(), field.rules->plants.end(),
                      [&](const ro::sim::PlantRule& rule) { return rule.foe_id == plant.foe_id; }));
    }
    // Est, ouest, nord, sud, puis nord-est du héros ; cinq cases distinctes.
    const FieldPoint hero = field.hero.position;
    CHECK(field.plants[0].position.x > hero.x);
    CHECK(field.plants[1].position.x < hero.x);
    CHECK(field.plants[2].position.y > hero.y);
    CHECK(field.plants[3].position.y < hero.y);
    CHECK(field.plants[4].position.x > hero.x);
    CHECK(field.plants[4].position.y > hero.y);
    for (std::size_t first = 0; first < field.plants.size(); ++first) {
      for (std::size_t second = first + 1; second < field.plants.size(); ++second) {
        CHECK_FALSE(field.plants[first].position == field.plants[second].position);
      }
    }
    CHECK(field.plant_kills_mark == 0U);
  }
}

TEST_CASE("Plantes au fil des tués : 10 × la minute en cours, prolongation comprise",
          "[sim][survivors]") {
  SurvivorsRun run = StartGenerated();
  REQUIRE(run.field().rules->tuning.plant_kills_per_minute == 10U);
  // La minute 1 court jusqu'au pas qui commence avant 1:00 : 10 tués.
  constexpr std::uint32_t kLastOfFirst = kStepsPerMinute - 1;
  CHECK_FALSE(StepWith(run, 0, 9, 0));
  CHECK(StepWith(run, 0, 10, 0));
  CHECK(run.field().plant_kills_mark == 10U);
  CHECK_FALSE(StepWith(run, kLastOfFirst, 9, 0));
  CHECK(StepWith(run, kLastOfFirst, 10, 0));
  // Le pas de 1:00 compte déjà à 20 ; le compteur part de la dernière plante.
  CHECK_FALSE(StepWith(run, kStepsPerMinute, 19, 0));
  CHECK(StepWith(run, kStepsPerMinute, 20, 0));
  CHECK_FALSE(StepWith(run, kStepsPerMinute, 129, 110));
  CHECK(StepWith(run, kStepsPerMinute, 130, 110));
  CHECK(run.field().plant_kills_mark == 130U);
  // L'excédent se perd : le compteur repart de zéro.
  CHECK(StepWith(run, kStepsPerMinute, 500, 0));
  CHECK(run.field().plant_kills_mark == 500U);
  // La minute 15 (14:00 à 14:59) : 150.
  constexpr std::uint32_t kMinute15 = 14 * kStepsPerMinute;
  CHECK_FALSE(StepWith(run, kMinute15, 149, 0));
  CHECK(StepWith(run, kMinute15, 150, 0));
  // La prolongation garde la règle : la minute 17 (16:00 à 16:59) compte 170.
  constexpr std::uint32_t kMinute17 = 16 * kStepsPerMinute + kStepsPerMinute / 2;
  CHECK_FALSE(StepWith(run, kMinute17, 169, 0));
  CHECK(StepWith(run, kMinute17, 170, 0));
}

TEST_CASE("Plantes au fil des tués : un plafond réglé retient la naissance, aucun ne la retient",
          "[sim][survivors]") {
  CHECK(StartGenerated().field().rules->tuning.most_live_plants == 0U);
  auto started = SurvivorsRun::Start(CappedTerrainRules(), ro::test_support::TerrainStart(kSeed));
  REQUIRE(started.has_value());
  SurvivorsRun run = std::move(*started);
  SurvivorsField& field = run.field_for_tests();
  const std::uint8_t most = field.rules->tuning.most_live_plants;
  REQUIRE(most == kBalancingCap);
  // Huit plantes à l'écran, loin du contact du héros.
  field.plants.clear();
  for (std::uint8_t index = 0; index < most; ++index) {
    field.plants.push_back(
        SurvivorsPlant{.serial = field.next_serial++,
                       .foe_id = field.rules->plants.front().foe_id,
                       .position = {field.hero.position.x + Cells(10),
                                    field.hero.position.y + Cells(index) - Cells(2)}});
  }
  field.foes.clear();
  field.tally.kills = 1000;
  field.plant_kills_mark = 0;
  run.Step(SurvivorsInput{});
  // Au plafond, le compteur continue de compter : rien ne naît.
  CHECK(run.field().plants.size() == most);
  CHECK(run.field().plant_kills_mark == 0U);
  // Une plante prise : la naissance due se fait au pas suivant.
  field.plants.erase(field.plants.begin());
  field.foes.clear();
  run.Step(SurvivorsInput{});
  CHECK(run.field().plants.size() == most);
  CHECK(run.field().plant_kills_mark == run.field().tally.kills);
}

TEST_CASE("Plantes au fil des tués : sans place hors de l'écran, la naissance attend",
          "[sim][survivors]") {
  // Une carte de 160 cases, bloquée sauf un îlot de 21 × 21 autour du départ
  // et une bande à l'est, à partir de la case 124. Du départ, tout le pourtour
  // du cadre (30 cases à l'est, plus la marge de 3 et l'anneau de 4) tombe sur
  // des cases bloquées : aucun essai ne trouve de place.
  constexpr std::int32_t kSide = ro::test_support::kGoldenArenaCells;
  constexpr std::int32_t kCentre = kSide / 2;
  constexpr std::int32_t kIslandHalfCells = 10;
  constexpr std::int32_t kEastBandFirstCell = 124;
  constexpr std::int32_t kEastHeroCell = 142;
  constexpr std::uint32_t kKillsOwed = 1000;
  constexpr int kStepsWaited = 50;
  constexpr int kStepsAllowed = 500;
  std::vector<std::uint8_t> passable(static_cast<std::size_t>(kSide) * kSide, 0U);
  for (std::int32_t y = 0; y < kSide; ++y) {
    for (std::int32_t x = 0; x < kSide; ++x) {
      const bool island =
          std::abs(x - kCentre) <= kIslandHalfCells && std::abs(y - kCentre) <= kIslandHalfCells;
      if (island || x >= kEastBandFirstCell) {
        passable[static_cast<std::size_t>(y) * kSide + static_cast<std::size_t>(x)] = 1U;
      }
    }
  }
  auto started = SurvivorsRun::Start(ShippedOnPillars(), ro::sim::WalkMap(kSide, kSide, passable),
                                     ro::test_support::GoldenStart(kSeed));
  REQUIRE(started.has_value());
  SurvivorsRun run = std::move(*started);
  SurvivorsField& field = run.field_for_tests();
  REQUIRE(CellOf(field.hero.position.x) == kCentre);
  const std::size_t start_plants = field.plants.size();
  field.tally.kills = kKillsOwed;
  field.plant_kills_mark = 0;
  // Sans place, rien ne naît et le compteur garde ses tués dus.
  for (int step = 0; step < kStepsWaited; ++step) run.Step(SurvivorsInput{});
  CHECK(run.field().plants.size() == start_plants);
  CHECK(run.field().plant_kills_mark == 0U);
  // Le héros passe dans la bande de l'est : la naissance due s'y fait.
  field.hero.position = ro::sim::CellCenter(kEastHeroCell, kCentre);
  int waited = 0;
  while (run.field().plant_kills_mark == 0U && waited < kStepsAllowed) {
    run.Step(SurvivorsInput{});
    ++waited;
  }
  CHECK(run.field().plant_kills_mark >= kKillsOwed);
  const SurvivorsPlant& born = run.field().plants.back();
  CHECK(CellOf(born.position.x) >= kEastBandFirstCell);
  CHECK_FALSE(OnScreen(*run.field().rules, run.field().hero.position, born.position));
}

TEST_CASE("Plantes au fil des tués : la plante naît juste hors de l'écran, sur une case libre",
          "[sim][survivors]") {
  for (const bool generated : {true, false}) {
    INFO((generated ? "sol généré" : "carte à piliers"));
    SurvivorsRun run = generated ? StartGenerated() : StartOnPillars();
    const SurvivorsRules& rules = *run.field().rules;
    const ro::sim::SpawnFrame& frame = rules.spawn_frame;
    const std::int64_t farthest =
        std::max({frame.half_width_units, frame.north_units, frame.south_units}) +
        frame.margin_units + Cells(rules.tuning.spawn_search_ring + 1);
    int births = 0;
    for (int watched = 0; watched < kBirthsWatched; ++watched) {
      const std::uint32_t mark = run.field().tally.kills;
      if (!StepWith(run, run.field().step, mark + rules.tuning.plant_kills_per_minute, mark)) {
        continue;
      }
      ++births;
      const SurvivorsField& field = run.field();
      const SurvivorsPlant& plant = field.plants.back();
      INFO("plante " << plant.serial);
      CHECK(field.ground.Passable(CellOf(plant.position.x), CellOf(plant.position.y)));
      CHECK_FALSE(OnScreen(rules, field.hero.position, plant.position));
      CHECK(Chebyshev(plant.position, field.hero.position) <= farthest);
      CHECK(Chebyshev(plant.position, field.hero.position) < rules.tuning.plant_forget_units);
    }
    // Sur la prairie comme entre les piliers, la place se trouve presque
    // toujours du premier coup.
    CHECK(births * 10 >= kBirthsWatched * 9);
  }
}

TEST_CASE("Plantes au fil des tués : une plante laissée à plus de 48 cases se perd",
          "[sim][survivors]") {
  for (const bool generated : {true, false}) {
    INFO((generated ? "sol généré" : "carte à piliers"));
    SurvivorsRun run = generated ? StartGenerated() : StartOnPillars();
    SurvivorsField& field = run.field_for_tests();
    const std::int32_t forget = field.rules->tuning.plant_forget_units;
    REQUIRE(forget == Cells(48));
    const FieldPoint hero = field.hero.position;
    const std::uint16_t foe_id = field.rules->plants.front().foe_id;
    field.plants.clear();
    field.foes.clear();
    const std::uint32_t kept = field.next_serial++;
    const std::uint32_t lost = field.next_serial++;
    field.plants.push_back(
        SurvivorsPlant{.serial = kept, .foe_id = foe_id, .position = {hero.x, hero.y - forget}});
    field.plants.push_back(SurvivorsPlant{
        .serial = lost, .foe_id = foe_id, .position = {hero.x - forget - 1, hero.y}});
    run.Step(SurvivorsInput{});
    REQUIRE(run.field().hero.position == hero);
    REQUIRE_FALSE(run.field().plants.empty());
    CHECK(run.field().plants.front().serial == kept);
    CHECK(std::none_of(run.field().plants.begin(), run.field().plants.end(),
                       [&](const SurvivorsPlant& plant) { return plant.serial == lost; }));
  }
}

TEST_CASE("Plantes : les rulesets antérieurs les posent par blocs, sans naissance ni oubli",
          "[sim][survivors]") {
  // Les plantes du départ de la partie en or, puis cinq minutes de jeu :
  // aucune plante neuve, et aucune ne s'est perdue sans avoir été cassée.
  const auto start = ro::test_support::PlayGolden(ro::test_support::kGoldenSeed, 0);
  REQUIRE(start.has_value());
  const std::vector<SurvivorsPlant> posed = start->run().field().plants;
  REQUIRE_FALSE(posed.empty());
  const auto played =
      ro::test_support::PlayGolden(ro::test_support::kGoldenSeed, ro::test_support::kGoldenSteps);
  REQUIRE(played.has_value());
  const SurvivorsField& field = played->run().field();
  CHECK(field.plant_kills_mark == 0U);
  for (const SurvivorsPlant& plant : field.plants) {
    CHECK(std::any_of(posed.begin(), posed.end(), [&](const SurvivorsPlant& first) {
      return first.serial == plant.serial && first.position == plant.position;
    }));
  }
  CHECK(field.plants.size() + field.plants_broken == posed.size());
}

TEST_CASE("Plantes au fil des tués : cinq minutes du joueur scripté entre les piliers",
          "[sim][survivors]") {
  // Les ors sur piliers jouent les plantes des rulesets antérieurs : ce rejeu
  // éprouve la règle livrée sur une carte bornée. À chaque pas, huit plantes
  // au plus, sur des cases praticables et distinctes, sous l'oubli.
  auto started = SurvivorsRun::Start(
      ShippedOnPillars(), ro::test_support::PillarArena(ro::test_support::kGoldenArenaCells),
      ro::test_support::GoldenStart(ro::test_support::kGoldenSeed));
  REQUIRE(started.has_value());
  ro::sim::SurvivorsPlay play(std::move(*started));
  const SurvivorsRules& rules = *play.run().field().rules;
  bool kept_rule = true;
  for (std::uint32_t step = 0; step < ro::test_support::kGoldenSteps && kept_rule; ++step) {
    ro::test_support::PlayScripted(play, 1);
    const SurvivorsField& field = play.run().field();
    kept_rule =
        rules.tuning.most_live_plants == 0 || field.plants.size() <= rules.tuning.most_live_plants;
    for (std::size_t first = 0; first < field.plants.size() && kept_rule; ++first) {
      const FieldPoint at = field.plants[first].position;
      kept_rule = field.ground.Passable(CellOf(at.x), CellOf(at.y)) &&
                  Chebyshev(at, field.hero.position) <= rules.tuning.plant_forget_units;
      for (std::size_t second = first + 1; second < field.plants.size() && kept_rule; ++second) {
        kept_rule = !(field.plants[second].position == at);
      }
    }
    INFO("pas " << field.step);
    CHECK(kept_rule);
  }
  // Des plantes sont nées au fil des tués.
  CHECK(play.run().field().plant_kills_mark > 0U);
}

TEST_CASE("Plantes au fil des tués : le robot va chercher une plante à l'écran",
          "[sim][survivors]") {
  // Une plante à 15 cases à l'est, à l'écran mais hors de la portée de gemme
  // du robot (10 cases), sans monstre ni gemme : il part vers l'est.
  constexpr std::int32_t kPlantEastCells = 15;
  constexpr std::uint8_t kEastHeading = 48;
  SurvivorsRun run = StartGenerated();
  SurvivorsField& field = run.field_for_tests();
  REQUIRE(ro::sim::MageBotProfile().gem_reach_units < Cells(kPlantEastCells));
  field.foes.clear();
  field.gems.clear();
  field.plants.clear();
  field.plants.push_back(SurvivorsPlant{
      .serial = field.next_serial++,
      .foe_id = field.rules->plants.front().foe_id,
      .position = {field.hero.position.x + Cells(kPlantEastCells), field.hero.position.y}});
  ro::sim::SurvivorsBot bot(ro::sim::MageBotProfile());
  CHECK(bot.Steer(run.field()).heading == kEastHeading);
  // Hors de l'écran, il ne la voit pas.
  field.plants.back().position.x =
      field.hero.position.x + run.field().rules->spawn_frame.half_width_units;
  ro::sim::SurvivorsBot blind(ro::sim::MageBotProfile());
  CHECK(blind.Steer(run.field()).heading == ro::sim::kNoHeading);
}

TEST_CASE("Plantes : CheckSurvivorsRules borne leurs réglages", "[sim][survivors]") {
  const auto accepted = [](auto change) {
    SurvivorsRules rules = ro::sim::ShippedSurvivorsRules();
    change(rules.tuning);
    return ro::sim::CheckSurvivorsRules(rules).has_value();
  };
  using Tuning = ro::sim::SurvivorsTuning;
  CHECK(accepted([](Tuning&) {}));
  // Par blocs ET au fil des tués, ou ni l'un ni l'autre.
  CHECK_FALSE(accepted([](Tuning& t) { t.plant_block_cells = 12; }));
  CHECK_FALSE(accepted([](Tuning& t) { t.plant_cells_per_plant = 150; }));
  CHECK_FALSE(accepted([](Tuning& t) { t.plant_kills_per_minute = 0; }));
  CHECK(accepted([](Tuning& t) { ro::test_support::UseLegacyPlants(t); }));
  // Des plantes du départ ou un plafond sans naissance au fil des tués.
  CHECK_FALSE(accepted([](Tuning& t) {
    ro::test_support::UseLegacyPlants(t);
    t.most_live_plants = 8;
  }));
  // Sans plafond, c'est permis ; plus de plantes au départ qu'un plafond
  // réglé ou que les caps, non.
  CHECK(accepted([](Tuning& t) { t.most_live_plants = 0; }));
  CHECK_FALSE(accepted([](Tuning& t) {
    t.most_live_plants = 4;
    t.plant_start_count = 5;
  }));
  CHECK(accepted([](Tuning& t) { t.plant_start_count = 8; }));
  CHECK_FALSE(accepted([](Tuning& t) {
    t.most_live_plants = 9;
    t.plant_start_count = 9;
  }));
  CHECK(accepted([](Tuning& t) { t.plant_start_count = 0; }));
  // Les plantes du départ à l'écran : 12 cases plus l'anneau de 4 restent sous
  // le côté sud du cadre, 17 cases ; 13 l'atteignent.
  CHECK(accepted([](Tuning& t) { t.plant_start_cells = 12; }));
  CHECK_FALSE(accepted([](Tuning& t) { t.plant_start_cells = 13; }));
  CHECK_FALSE(accepted([](Tuning& t) { t.plant_start_cells = 0; }));
  // L'oubli au-delà de la plus lointaine naissance : 30 + 3 + 4 + 1 cases.
  CHECK_FALSE(accepted([](Tuning& t) { t.plant_forget_units = 0; }));
  CHECK_FALSE(accepted([](Tuning& t) { t.plant_forget_units = Cells(38); }));
  CHECK(accepted([](Tuning& t) { t.plant_forget_units = Cells(38) + 1; }));
}
