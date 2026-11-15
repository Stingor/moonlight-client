// L'arène générée : une partie qui naît sans carte, ses plantes qui se
// perdent au loin, et son rejeu sans installation.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/error.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_ground.h"
#include "ro/sim/survivors_journal.h"
#include "ro/sim/survivors_play.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/survivors_state.h"
#include "ro/sim/survivors_terrain.h"

#include "support/survivors_fixture.h"
#include "support/survivors_golden.h"
#include "support/survivors_terrain_fixture.h"

using ro::sim::CellOf;
using ro::sim::FieldPoint;
using ro::sim::kTerrainChunkCells;
using ro::sim::kTerrainStartCell;
using ro::sim::SurvivorsField;
using ro::sim::SurvivorsGround;
using ro::sim::SurvivorsPlant;
using ro::sim::SurvivorsPlay;
using ro::sim::SurvivorsRun;
using ro::sim::SurvivorsStart;
using ro::sim::TerrainChunkOf;
using ro::test_support::TerrainRules;
using ro::test_support::TerrainStart;

namespace {

constexpr std::uint64_t kSeed = 20261026;
constexpr std::uint64_t kOtherSeed = 20261027;
// La marche vers l'est : 2 000 cases, et les relèves qui la font tenir.
constexpr std::int32_t kMarchCells = 2000;
constexpr std::uint8_t kMarchLives = 40;
// Au-delà, la marche est bloquée : elle a pris plus de deux fois le temps
// qu'il faut au héros, 150 ms par case.
constexpr std::uint32_t kMostMarchSteps = 32000;
// Le départ à l'est : 300 cases, au-delà de l'oubli.
constexpr std::int32_t kAwayCells = 300;
// Trente secondes de partie, en pas de 20 ms.
constexpr std::uint32_t kHalfMinuteSteps = 1500;
constexpr std::int32_t kSmallestCache = 1;
// Les caps, 0 au nord dans le sens trigonométrique : l'est, et les deux
// diagonales qui ramènent sur le rail.
constexpr std::uint8_t kEastHeading = 48;
constexpr std::uint8_t kHeadingStep = 8;
// Une horde suit le héros quand chacun de ses monstres reste à moins de cette
// distance de Tchebychev : le rappel et un pas de rattrapage.
constexpr std::int32_t kFollowingCells = 64;

[[nodiscard]] std::int64_t Chebyshev(FieldPoint a, FieldPoint b) {
  const std::int64_t dx = std::int64_t{a.x} - b.x;
  const std::int64_t dy = std::int64_t{a.y} - b.y;
  return std::max(dx < 0 ? -dx : dx, dy < 0 ? -dy : dy);
}

// LE RAIL : la ligne de cases du départ est la bordure sud des emplacements
// qu'elle traverse, libre dans tout le monde. Le héros y marche vers
// `heading` (l'est ou l'ouest) et y revient en diagonale s'il en est poussé.
[[nodiscard]] std::uint8_t RailHeading(const SurvivorsField& field, std::uint8_t heading) {
  const std::int32_t row = CellOf(field.hero.position.y);
  if (row == kTerrainStartCell) return heading;
  const bool north_of_rail = row > kTerrainStartCell;
  // Vers l'est, le sud est à droite (cap décroissant) ; vers l'ouest, à gauche.
  const bool turn_right = (heading == kEastHeading) == north_of_rail;
  return static_cast<std::uint8_t>(turn_right ? heading - kHeadingStep : heading + kHeadingStep);
}

// Ce que la marche observe à chaque pas.
struct MarchWatch {
  bool born_on_blocked = false;
  bool plant_on_blocked = false;
  bool plant_too_far = false;
  std::size_t most_plants = 0;
  std::uint32_t newest_plant_serial = 0;
  std::uint32_t last_seen_serial = 0;
};

void Watch(const SurvivorsField& field, MarchWatch& watch) {
  for (const ro::sim::SurvivorsFoe& foe : field.foes) {
    if (foe.serial <= watch.last_seen_serial || foe.serial >= ro::sim::kFirstCardSerial) continue;
    watch.born_on_blocked = watch.born_on_blocked ||
                            !field.ground.Passable(CellOf(foe.position.x), CellOf(foe.position.y));
  }
  watch.last_seen_serial = field.next_serial - 1;
  for (const SurvivorsPlant& plant : field.plants) {
    watch.plant_on_blocked =
        watch.plant_on_blocked ||
        !field.ground.Passable(CellOf(plant.position.x), CellOf(plant.position.y));
    watch.plant_too_far = watch.plant_too_far || Chebyshev(plant.position, field.hero.position) >
                                                     field.rules->tuning.plant_forget_units;
    watch.newest_plant_serial = std::max(watch.newest_plant_serial, plant.serial);
  }
  watch.most_plants = std::max(watch.most_plants, field.plants.size());
}

// Le héros marche sur le rail vers `heading` jusqu'à la case `goal_x`,
// répond aux choix comme le joueur scripté, et s'arrête si la partie finit
// ou s'il ne progresse plus.
void MarchTo(SurvivorsPlay& play, std::uint8_t heading, std::int32_t goal_x, MarchWatch& watch) {
  const std::uint32_t last_step = play.run().field().step + kMostMarchSteps;
  const auto reached = [&](const SurvivorsField& field) {
    const std::int32_t x = CellOf(field.hero.position.x);
    return heading == kEastHeading ? x >= goal_x : x <= goal_x;
  };
  while (play.run().field().step < last_step) {
    const SurvivorsField& field = play.run().field();
    if (field.phase == ro::sim::SurvivorsPhase::kOver || reached(field)) return;
    if (field.phase == ro::sim::SurvivorsPhase::kChoosing) {
      if (!play.Answer(ro::test_support::ScriptedAnswer(field))) return;
      continue;
    }
    if (play.Advance(ro::sim::kSurvivorsStepMs,
                     ro::sim::SurvivorsInput{.heading = RailHeading(field, heading)}) == 0) {
      return;
    }
    Watch(play.run().field(), watch);
  }
}

[[nodiscard]] SurvivorsPlay StartPlay(const SurvivorsStart& start) {
  auto run = SurvivorsRun::Start(TerrainRules(), start);
  REQUIRE(run.has_value());
  return SurvivorsPlay(std::move(*run));
}

// Les deux champs sont la même partie : bilan, héros, monstres, gemmes et
// plantes, champ par champ.
void RequireSameField(const SurvivorsField& left, const SurvivorsField& right) {
  REQUIRE(left.step == right.step);
  REQUIRE(left.tally == right.tally);
  REQUIRE(left.hero.position == right.hero.position);
  REQUIRE(left.hero.hp == right.hero.hp);
  REQUIRE(left.next_serial == right.next_serial);
  REQUIRE(left.foes.size() == right.foes.size());
  for (std::size_t index = 0; index < left.foes.size(); ++index) {
    REQUIRE(left.foes[index].serial == right.foes[index].serial);
    REQUIRE(left.foes[index].position == right.foes[index].position);
    REQUIRE(left.foes[index].hp == right.foes[index].hp);
  }
  REQUIRE(left.gems.size() == right.gems.size());
  for (std::size_t index = 0; index < left.gems.size(); ++index) {
    REQUIRE(left.gems[index].position == right.gems[index].position);
  }
  REQUIRE(left.plants.size() == right.plants.size());
  for (std::size_t index = 0; index < left.plants.size(); ++index) {
    REQUIRE(left.plants[index].serial == right.plants[index].serial);
    REQUIRE(left.plants[index].foe_id == right.plants[index].foe_id);
    REQUIRE(left.plants[index].position == right.plants[index].position);
  }
  REQUIRE(left.plant_kills_mark == right.plant_kills_mark);
}

// Les plantes `plants` sont `pinned`, champ par champ.
template <std::size_t kCount>
void RequireSamePlants(const std::vector<SurvivorsPlant>& plants,
                       const std::array<SurvivorsPlant, kCount>& pinned) {
  REQUIRE(plants.size() == kCount);
  for (std::size_t index = 0; index < kCount; ++index) {
    CHECK(plants[index].serial == pinned[index].serial);
    CHECK(plants[index].foe_id == pinned[index].foe_id);
    CHECK(plants[index].position == pinned[index].position);
  }
}

[[nodiscard]] bool Refused(const ro::core::Result<SurvivorsRun>& run, std::string_view part) {
  return !run && run.error().detail.find(part) != std::string::npos;
}

}  // namespace

TEST_CASE("Arène générée : la partie naît au centre du monde, sans carte", "[sim][survivors]") {
  const SurvivorsPlay play = StartPlay(TerrainStart(kSeed));
  const SurvivorsField& field = play.run().field();
  REQUIRE_FALSE(field.ground.bounded());
  REQUIRE(field.hero.position == ro::sim::CellCenter(kTerrainStartCell, kTerrainStartCell));
  REQUIRE(field.ground.Passable(kTerrainStartCell, kTerrainStartCell));

  // Les plantes du départ, posées sur des cases libres.
  REQUIRE(field.plants.size() == TerrainRules().tuning.plant_start_count);
  std::uint32_t serial = 0;
  for (const SurvivorsPlant& plant : field.plants) {
    REQUIRE(plant.serial > serial);
    serial = plant.serial;
    REQUIRE(field.ground.Passable(CellOf(plant.position.x), CellOf(plant.position.y)));
  }
}

TEST_CASE("Arène générée : une carte et un sol généré ne se confondent pas", "[sim][survivors]") {
  // L'arène générée refuse une carte ; une arène à carte ne naît pas sans elle.
  const auto on_map = SurvivorsRun::Start(
      TerrainRules(), ro::test_support::PillarArena(ro::test_support::kGoldenArenaCells),
      TerrainStart(kSeed));
  CHECK(Refused(on_map, "est générée"));
  const auto without_map =
      SurvivorsRun::Start(ro::test_support::GoldenRules(), ro::test_support::GoldenStart(kSeed));
  CHECK(Refused(without_map, "lit une carte"));
  SurvivorsStart unknown = TerrainStart(kSeed);
  unknown.arena_id = ro::test_support::kTerrainArenaId + 1;
  CHECK(Refused(SurvivorsRun::Start(TerrainRules(), unknown), "arène inconnue"));

  // Un sol généré n'est accepté que s'il est celui que le rejeu redérive : le
  // terrain de l'arène dans ces règles, sous la graine du départ.
  const ro::sim::TerrainRule& terrain = TerrainRules().terrains.front();
  CHECK(SurvivorsRun::Start(TerrainRules(), SurvivorsGround::Generated(terrain, kSeed),
                            TerrainStart(kSeed))
            .has_value());
  CHECK(Refused(SurvivorsRun::Start(TerrainRules(), SurvivorsGround::Generated(terrain, kOtherSeed),
                                    TerrainStart(kSeed)),
                "n'est pas celui"));
  const ro::sim::TerrainRule copy = terrain;
  CHECK(Refused(SurvivorsRun::Start(TerrainRules(), SurvivorsGround::Generated(copy, kSeed),
                                    TerrainStart(kSeed)),
                "n'est pas celui"));
  CHECK(Refused(SurvivorsRun::Start(ro::test_support::GoldenRules(),
                                    SurvivorsGround::Generated(terrain, kSeed),
                                    ro::test_support::GoldenStart(kSeed)),
                "n'est pas celui"));
}

TEST_CASE("Arène générée : un héros qui marche 2 000 cases vers l'est garde sa horde",
          "[sim][survivors]") {
  SurvivorsStart start = TerrainStart(kSeed);
  start.lives = kMarchLives;
  SurvivorsPlay play = StartPlay(start);
  MarchWatch watch;
  MarchTo(play, kEastHeading, kTerrainStartCell + kMarchCells, watch);
  const SurvivorsField& field = play.run().field();
  INFO("pas " << field.step << ", case " << CellOf(field.hero.position.x));
  REQUIRE(field.phase != ro::sim::SurvivorsPhase::kOver);
  REQUIRE(CellOf(field.hero.position.x) >= kTerrainStartCell + kMarchCells);
  REQUIRE(field.ground.Passable(CellOf(field.hero.position.x), CellOf(field.hero.position.y)));

  // La horde a suivi, et rien n'est né sur une case bloquée.
  REQUIRE(field.foes.size() > 0);
  for (const ro::sim::SurvivorsFoe& foe : field.foes) {
    REQUIRE(Chebyshev(foe.position, field.hero.position) <= ro::sim::Cells(kFollowingCells));
  }
  CHECK_FALSE(watch.born_on_blocked);

  // Les plantes restent sur des cases libres, près du héros, et leur liste
  // sous son plafond s'il y en a un ; il en naît en chemin.
  CHECK_FALSE(watch.plant_on_blocked);
  CHECK_FALSE(watch.plant_too_far);
  INFO("plantes au plus " << watch.most_plants << ", prochain numéro " << field.next_serial);
  const std::uint8_t cap = TerrainRules().tuning.most_live_plants;
  CHECK((cap == 0 || watch.most_plants <= cap));
  CHECK(watch.newest_plant_serial > TerrainRules().tuning.plant_start_count);
}

TEST_CASE("Arène générée : une plante lointaine se perd", "[sim][survivors]") {
  SurvivorsStart start = TerrainStart(kSeed);
  start.lives = kMarchLives;
  SurvivorsPlay play = StartPlay(start);
  const std::vector<SurvivorsPlant> first_plants = play.run().field().plants;
  REQUIRE_FALSE(first_plants.empty());

  MarchWatch watch;
  MarchTo(play, kEastHeading, kTerrainStartCell + kAwayCells, watch);
  REQUIRE(CellOf(play.run().field().hero.position.x) >= kTerrainStartCell + kAwayCells);
  // Les plantes du départ, à plus de l'oubli du héros, se sont perdues.
  for (const SurvivorsPlant& plant : play.run().field().plants) {
    for (const SurvivorsPlant& first : first_plants) REQUIRE(plant.serial != first.serial);
  }
  CHECK_FALSE(watch.plant_too_far);
}

TEST_CASE("Arène générée : même graine, même partie ; deux graines, deux terrains",
          "[sim][survivors]") {
  SurvivorsPlay left = StartPlay(TerrainStart(kSeed));
  SurvivorsPlay right = StartPlay(TerrainStart(kSeed));
  ro::test_support::PlayScripted(left, kHalfMinuteSteps);
  ro::test_support::PlayScripted(right, kHalfMinuteSteps);
  RequireSameField(left.run().field(), right.run().field());

  // Le cache du sol ne décide de rien, plantes comprises.
  auto narrow = SurvivorsRun::Start(
      TerrainRules(),
      SurvivorsGround::Generated(TerrainRules().terrains.front(), kSeed, kSmallestCache),
      TerrainStart(kSeed));
  REQUIRE(narrow.has_value());
  SurvivorsPlay narrow_play(std::move(*narrow));
  ro::test_support::PlayScripted(narrow_play, kHalfMinuteSteps);
  RequireSameField(left.run().field(), narrow_play.run().field());

  const SurvivorsPlay other = StartPlay(TerrainStart(kOtherSeed));
  const std::int32_t start_chunk = TerrainChunkOf(kTerrainStartCell);
  const ro::sim::TerrainChunk* mine =
      left.run().field().ground.ChunkAt(start_chunk + 1, start_chunk);
  REQUIRE(mine != nullptr);
  const ro::sim::TerrainChunk kept = *mine;
  const ro::sim::TerrainChunk* theirs =
      other.run().field().ground.ChunkAt(start_chunk + 1, start_chunk);
  REQUIRE(theirs != nullptr);
  CHECK_FALSE(kept == *theirs);
}

TEST_CASE("Arène générée : un journal se rejoue sans carte, au même bilan", "[sim][survivors]") {
  SurvivorsPlay play = StartPlay(TerrainStart(kSeed));
  ro::test_support::PlayScripted(play, kHalfMinuteSteps);
  play.Abandon();
  const std::vector<std::uint8_t> bytes = ro::sim::EncodeSurvivorsJournal(play.journal());
  const auto decoded = ro::sim::DecodeSurvivorsJournal(bytes);
  REQUIRE(decoded.has_value());

  const auto replayed = ro::sim::ReplaySurvivors(TerrainRules(), *decoded);
  REQUIRE(replayed.has_value());
  REQUIRE(*replayed == play.run().tally());

  const auto halfway = ro::sim::ReplaySurvivorsTo(TerrainRules(), *decoded, kHalfMinuteSteps / 2);
  REQUIRE(halfway.has_value());
  REQUIRE(halfway->field().step == kHalfMinuteSteps / 2);

  // Une arène à carte ne se rejoue pas sans sa carte.
  const auto golden = ro::test_support::PlayGolden(kSeed, 1);
  REQUIRE(golden.has_value());
  CHECK_FALSE(ro::sim::ReplaySurvivors(ro::test_support::GoldenRules(), golden->journal()));
}

// L'OR DE L'ARÈNE GÉNÉRÉE : les plantes posées au départ et le bilan d'une
// demi-minute du joueur scripté, valeur pour valeur. L'ordre des caps du
// départ, les tirages d'une naissance ou la place de l'oubli et de la
// naissance dans le pas ne changent plus sans qu'il le dise. Les mêmes
// valeurs sous MSVC et sous g++-14.
TEST_CASE("Arène générée : la partie de la graine d'essai, épinglée", "[sim][survivors]") {
  // Les plantes du départ, champ par champ : à 12 cases à l'est, à l'ouest,
  // au nord, au sud et au nord-est de la case du départ.
  constexpr std::array<SurvivorsPlant, 5> kStartPlants{{
      {.serial = 1, .foe_id = 1078, .position = {33567232, 33554944}},
      {.serial = 2, .foe_id = 1079, .position = {33542656, 33554944}},
      {.serial = 3, .foe_id = 1078, .position = {33554944, 33567232}},
      {.serial = 4, .foe_id = 1079, .position = {33554944, 33542656}},
      {.serial = 5, .foe_id = 1080, .position = {33567232, 33567232}},
  }};
  const SurvivorsPlay play = StartPlay(TerrainStart(kSeed));
  RequireSamePlants(play.run().field().plants, kStartPlants);

  constexpr ro::sim::SurvivorsTally kHalfMinuteTally{
      .survived_steps = kHalfMinuteSteps,
      .kills = 14,
      .boss_kills = 0,
      .kill_points = 34,
      .level = 1,
      .exp_total = 4,
      .hp_left = 337,
      .damage_dealt = 565,
      .gems_gathered = 4,
      .foes_alive = 59,
      .outcome = ro::sim::SurvivorsOutcome::kRunning,
      .elite_kills = 0,
      .chests_opened = 0,
      .evolutions = 0,
  };
  constexpr FieldPoint kHalfMinuteHero{33543750, 33550630};
  // Une plante reste à la demi-minute, à 51 cases à l'ouest et une au sud de
  // la case du départ, et aucune n'est cassée ; la dernière est née à 10 tués.
  constexpr std::array<SurvivorsPlant, 1> kHalfMinutePlants{{
      {.serial = 79, .foe_id = 1083, .position = {33502720, 33553920}},
  }};
  constexpr std::uint32_t kHalfMinuteKillsMark = 10;
  constexpr std::uint32_t kHalfMinuteNextSerial = 92;
  SurvivorsPlay half = StartPlay(TerrainStart(kSeed));
  ro::test_support::PlayScripted(half, kHalfMinuteSteps);
  const SurvivorsField& played = half.run().field();
  CHECK(played.tally == kHalfMinuteTally);
  CHECK(played.hero.position == kHalfMinuteHero);
  RequireSamePlants(played.plants, kHalfMinutePlants);
  CHECK(played.plant_kills_mark == kHalfMinuteKillsMark);
  CHECK(played.plants_broken == 0U);
  CHECK(played.next_serial == kHalfMinuteNextSerial);
}
