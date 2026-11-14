#include "ro/sim/survivors_ground.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_grid.h"
#include "ro/sim/survivors_play.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/survivors_state.h"
#include "ro/sim/survivors_terrain.h"
#include "ro/sim/walk_map.h"

#include "support/survivors_fixture.h"
#include "support/survivors_golden.h"

using ro::sim::CellOf;
using ro::sim::FieldPoint;
using ro::sim::GenerateTerrainChunk;
using ro::sim::kSubunitsPerCell;
using ro::sim::kTerrainChunkCells;
using ro::sim::kTerrainStartCell;
using ro::sim::kTerrainWorldCells;
using ro::sim::kTerrainWorldChunks;
using ro::sim::SurvivorsField;
using ro::sim::SurvivorsGrid;
using ro::sim::SurvivorsGround;
using ro::sim::SurvivorsPlay;
using ro::sim::SurvivorsRun;
using ro::sim::TerrainChunk;
using ro::sim::TerrainPassableAt;
using ro::sim::TerrainPieceRule;
using ro::sim::TerrainRule;
using ro::sim::WalkMap;

namespace {

// LES RÈGLES D'ESSAI DU SOL : un rocher, un arbre en croix, un bosquet en L.
constexpr std::array<std::string_view, 2> kRock = {"##", "##"};
constexpr std::array<std::string_view, 3> kTree = {".#.", "###", ".#."};
constexpr std::array<std::string_view, 3> kGrove = {"#...", "#...", "####"};

constexpr std::array<TerrainPieceRule, 3> kPieces{{
    {.piece_id = 1, .key = "rock", .footprint = kRock, .weight = 3},
    {.piece_id = 2, .key = "tree", .footprint = kTree, .weight = 4},
    {.piece_id = 3, .key = "grove", .footprint = kGrove, .weight = 2},
}};

constexpr std::uint16_t kTerrainId = 1;
constexpr std::array<TerrainRule, 1> kTerrains{{
    {.terrain_id = kTerrainId,
     .key = "ground_plains",
     .pieces = kPieces,
     .density_min_per_mille = 100,
     .density_max_per_mille = 500,
     .density_period_chunks = 2,
     .clearing_cells = 12,
     .landmark_period_chunks = 0},
}};
constexpr const TerrainRule& kRule = kTerrains[0];

// L'arène de la partie en or, générée sur la règle d'essai.
constexpr std::array<ro::sim::ArenaRule, 1> kGeneratedArenas{{
    {.arena_id = ro::test_support::kGoldenArenaId,
     .key = "ground_plains_arena",
     .map_name = "",
     .start = std::nullopt,
     .terrain_id = kTerrainId,
     .ranked = true},
}};

// Les règles en or, dont l'arène nomme la règle d'essai : un sol généré ne se
// joue que sur le terrain que son arène nomme.
[[nodiscard]] const ro::sim::SurvivorsRules& GeneratedGoldenRules() {
  static const ro::sim::SurvivorsRules kRules = [] {
    ro::sim::SurvivorsRules rules = ro::test_support::GoldenRules();
    rules.arenas = kGeneratedArenas;
    rules.terrains = kTerrains;
    return rules;
  }();
  return kRules;
}

constexpr std::uint64_t kSeed = 20261026;
// Trois minutes de partie, en pas de 20 ms.
constexpr std::uint32_t kThreeMinuteSteps = 9000;
constexpr std::uint8_t kSpareLives = 3;
constexpr std::int32_t kSmallestCache = 1;
// Le voisinage du départ que les essais de case balaient, en cases.
constexpr std::int32_t kProbeRadiusCells = 80;

// Ce qu'on compare d'une partie : le bilan, le héros, et chaque monstre, gemme
// et plante, champ par champ.
void RequireSameField(const SurvivorsField& left, const SurvivorsField& right) {
  REQUIRE(left.step == right.step);
  REQUIRE(left.tally == right.tally);
  REQUIRE(left.hero.position == right.hero.position);
  REQUIRE(left.hero.hp == right.hero.hp);
  REQUIRE(left.hero.level == right.hero.level);
  REQUIRE(left.next_serial == right.next_serial);
  REQUIRE(left.foes.size() == right.foes.size());
  for (std::size_t index = 0; index < left.foes.size(); ++index) {
    REQUIRE(left.foes[index].serial == right.foes[index].serial);
    REQUIRE(left.foes[index].foe_id == right.foes[index].foe_id);
    REQUIRE(left.foes[index].position == right.foes[index].position);
    REQUIRE(left.foes[index].hp == right.foes[index].hp);
  }
  REQUIRE(left.gems.size() == right.gems.size());
  for (std::size_t index = 0; index < left.gems.size(); ++index) {
    REQUIRE(left.gems[index].position == right.gems[index].position);
  }
  REQUIRE(left.plants.size() == right.plants.size());
}

// La partie en or, sur le sol généré de la règle d'essai, avec un cache de
// `cache_side_chunks` de côté.
[[nodiscard]] std::optional<SurvivorsPlay> PlayOnGeneratedGround(std::int32_t cache_side_chunks,
                                                                 std::uint32_t steps) {
  // Des relèves en réserve : le joueur scripté, qui ne voit pas le décor,
  // tient ainsi les trois minutes.
  ro::sim::SurvivorsStart start = ro::test_support::GoldenStart(kSeed);
  start.lives = kSpareLives;
  auto run = SurvivorsRun::Start(
      GeneratedGoldenRules(), SurvivorsGround::Generated(kRule, kSeed, cache_side_chunks), start);
  if (!run) return std::nullopt;
  std::optional<SurvivorsPlay> play(std::in_place, std::move(*run));
  ro::test_support::PlayScripted(*play, steps);
  return play;
}

}  // namespace

TEST_CASE("un sol borné répond case par case ce que répond sa carte", "[sim][survivors]") {
  const WalkMap map = ro::test_support::PillarArena(ro::test_support::kGoldenArenaCells);
  const SurvivorsGround ground = SurvivorsGround::Bounded(map);
  REQUIRE(ground.bounded());
  REQUIRE(ground.width() == map.width());
  REQUIRE(ground.height() == map.height());
  REQUIRE(ground.ChunkAt(0, 0) == nullptr);
  // Une case de marge autour de la carte : hors de la carte, tout est bloqué.
  for (std::int32_t y = -1; y <= map.height(); ++y) {
    for (std::int32_t x = -1; x <= map.width(); ++x) {
      REQUIRE(ground.Passable(x, y) == map.Passable(x, y));
    }
  }
  const SurvivorsGround empty;
  REQUIRE(empty.bounded());
  REQUIRE_FALSE(empty.Passable(0, 0));
}

TEST_CASE("un sol généré répond ce que répond le générateur, quel que soit son cache",
          "[sim][survivors]") {
  const SurvivorsGround wide = SurvivorsGround::Generated(kRule, kSeed);
  const SurvivorsGround narrow = SurvivorsGround::Generated(kRule, kSeed, kSmallestCache);
  REQUIRE_FALSE(wide.bounded());
  REQUIRE(wide.width() == kTerrainWorldCells);
  REQUIRE(wide.height() == kTerrainWorldCells);
  for (std::int32_t y = kTerrainStartCell - kProbeRadiusCells;
       y <= kTerrainStartCell + kProbeRadiusCells; ++y) {
    for (std::int32_t x = kTerrainStartCell - kProbeRadiusCells;
         x <= kTerrainStartCell + kProbeRadiusCells; ++x) {
      const bool expected = TerrainPassableAt(kRule, kSeed, x, y);
      REQUIRE(wide.Passable(x, y) == expected);
      REQUIRE(narrow.Passable(x, y) == expected);
    }
  }
  // Le cache d'un morceau a régénéré à chaque changement de morceau.
  CHECK(narrow.chunks_generated() > wide.chunks_generated());

  // Hors du monde, tout est bloqué, et aucun morceau n'existe.
  for (const std::int32_t outside : {-1, kTerrainWorldCells}) {
    CHECK_FALSE(wide.Passable(outside, kTerrainStartCell));
    CHECK_FALSE(wide.Passable(kTerrainStartCell, outside));
  }
  CHECK(wide.ChunkAt(-1, 0) == nullptr);
  CHECK(wide.ChunkAt(0, kTerrainWorldChunks) == nullptr);

  // Le côté du cache est borné des deux côtés.
  CHECK(SurvivorsGround::Generated(kRule, kSeed, 0).cache_side_chunks() == kSmallestCache);
  CHECK(SurvivorsGround::Generated(kRule, kSeed, kTerrainWorldChunks).cache_side_chunks() ==
        ro::sim::kMostGroundCacheSideChunks);
  CHECK(wide.rule() == &kRule);
  CHECK(wide.run_seed() == kSeed);

  // Un morceau rendu est celui du générateur, pièces comprises, et le reste
  // après qu'un autre l'a chassé du cache.
  constexpr std::int32_t kStartChunk = kTerrainStartCell / kTerrainChunkCells;
  const TerrainChunk expected = GenerateTerrainChunk(kRule, kSeed, kStartChunk + 1, kStartChunk);
  const TerrainChunk* first = narrow.ChunkAt(kStartChunk + 1, kStartChunk);
  REQUIRE(first != nullptr);
  REQUIRE(*first == expected);
  REQUIRE(narrow.ChunkAt(kStartChunk, kStartChunk) != nullptr);
  const TerrainChunk* again = narrow.ChunkAt(kStartChunk + 1, kStartChunk);
  REQUIRE(again != nullptr);
  REQUIRE(*again == expected);
}

TEST_CASE("un cache d'un seul morceau rend la même partie qu'un cache de 256", "[sim][survivors]") {
  const std::optional<SurvivorsPlay> wide =
      PlayOnGeneratedGround(ro::sim::kGroundCacheSideChunks, kThreeMinuteSteps);
  const std::optional<SurvivorsPlay> narrow =
      PlayOnGeneratedGround(kSmallestCache, kThreeMinuteSteps);
  REQUIRE(wide.has_value());
  REQUIRE(narrow.has_value());
  const SurvivorsField& wide_field = wide->run().field();
  const SurvivorsField& narrow_field = narrow->run().field();
  // La partie a bien joué ses trois minutes : la horde est née et le héros a
  // tué.
  REQUIRE(wide_field.step == kThreeMinuteSteps);
  REQUIRE(wide_field.tally.kills > 0);
  REQUIRE_FALSE(wide_field.foes.empty());
  REQUIRE(wide_field.ground.Passable(CellOf(wide_field.hero.position.x),
                                     CellOf(wide_field.hero.position.y)));
  RequireSameField(wide_field, narrow_field);
  // Le petit cache a bien été chassé : il a régénéré bien plus de morceaux.
  CHECK(narrow_field.ground.chunks_generated() > wide_field.ground.chunks_generated());
}

TEST_CASE("une grille recentrée rend les mêmes voisins qu'une grille fixe", "[sim][survivors]") {
  // Des points étalés sur 300 cases, plus loin que l'aire de la grille
  // recentrée : ceux du dehors tombent dans ses seaux du bord.
  constexpr std::int32_t kSpreadCells = 300;
  constexpr std::int32_t kPointCount = 600;
  // Des pas premiers entre eux et l'étendue : les points ne s'alignent pas.
  constexpr std::int32_t kStrideXSubunits = 12347;
  constexpr std::int32_t kStrideYSubunits = 7919;
  constexpr std::int32_t kSpreadSubunits = kSpreadCells * kSubunitsPerCell;
  std::vector<FieldPoint> points;
  for (std::int32_t index = 0; index < kPointCount; ++index) {
    points.push_back(FieldPoint{(index * kStrideXSubunits) % kSpreadSubunits,
                                (index * kStrideYSubunits) % kSpreadSubunits});
  }
  SurvivorsGrid fixed;
  fixed.Reset(kSpreadCells, kSpreadCells);
  fixed.Rebuild(points);

  constexpr std::array<std::int32_t, 3> kRadiiCells = {1, 6, 20};
  std::vector<std::uint32_t> from_fixed;
  std::vector<std::uint32_t> from_recentred;
  SurvivorsGrid recentred;
  recentred.Reset(ro::sim::kFollowingGridCells, ro::sim::kFollowingGridCells);
  // Des origines en deçà, au milieu et au-delà de l'étendue des points.
  constexpr std::int32_t kFirstOriginCells = -40;
  constexpr std::int32_t kOriginStrideCells = 70;
  for (std::int32_t origin = kFirstOriginCells; origin <= kSpreadCells;
       origin += kOriginStrideCells) {
    recentred.Recenter(origin, origin / 2);
    recentred.Rebuild(points);
    for (const FieldPoint& center : points) {
      for (const std::int32_t radius_cells : kRadiiCells) {
        const std::int32_t radius = radius_cells * kSubunitsPerCell;
        fixed.Near(center, radius, points, from_fixed);
        recentred.Near(center, radius, points, from_recentred);
        REQUIRE(from_fixed == from_recentred);
      }
    }
  }
}
