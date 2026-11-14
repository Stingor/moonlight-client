#include "ro/sim/survivors_terrain.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/error.h"
#include "ro/sim/survivors_rules.h"

#include "support/survivors_fixture.h"

using ro::sim::CheckSurvivorsRules;
using ro::sim::FootprintCovers;
using ro::sim::FootprintSize;
using ro::sim::FootprintSizeOf;
using ro::sim::GenerateTerrainChunk;
using ro::sim::kTerrainChunkCells;
using ro::sim::kTerrainSlotCells;
using ro::sim::kTerrainStartCell;
using ro::sim::PlacedPiece;
using ro::sim::TerrainChunk;
using ro::sim::TerrainPieceRule;
using ro::sim::TerrainRule;

namespace {

// LES RÈGLES D'ESSAI : un rocher carré, un arbre en croix, un bosquet en L
// (plus large que haut, pour que le quart de tour se voie) et un repère.
constexpr std::array<std::string_view, 2> kRock = {"##", "##"};
constexpr std::array<std::string_view, 3> kTree = {".#.", "###", ".#."};
constexpr std::array<std::string_view, 3> kGrove = {"#...", "#...", "####"};
constexpr std::array<std::string_view, 5> kWindmill = {
    "..##..", ".####.", "######", ".####.", "..##..",
};

constexpr std::uint16_t kRockId = 1;
constexpr std::uint16_t kTreeId = 2;
constexpr std::uint16_t kGroveId = 3;
constexpr std::uint16_t kWindmillId = 4;
constexpr std::uint16_t kTerrainId = 1;

constexpr std::array<TerrainPieceRule, 4> kPieces{{
    {.piece_id = kRockId, .key = "rock", .footprint = kRock, .weight = 3},
    {.piece_id = kTreeId, .key = "tree", .footprint = kTree, .weight = 4},
    {.piece_id = kGroveId, .key = "grove", .footprint = kGrove, .weight = 2},
    {.piece_id = kWindmillId,
     .key = "windmill",
     .footprint = kWindmill,
     .weight = 1,
     .landmark = true},
}};

constexpr std::int32_t kClearingCells = 12;
constexpr std::int32_t kLandmarkPeriod = 3;

constexpr TerrainRule kRule{.terrain_id = kTerrainId,
                            .key = "test_plains",
                            .pieces = kPieces,
                            .density_min_per_mille = 200,
                            .density_max_per_mille = 900,
                            .density_period_chunks = 2,
                            .clearing_cells = kClearingCells,
                            .landmark_period_chunks = kLandmarkPeriod};

constexpr std::int32_t kStartChunk = kTerrainStartCell / kTerrainChunkCells;
// La fenêtre des essais de connexité : 9 × 9 morceaux centrés sur le départ.
constexpr std::int32_t kWindowRadiusChunks = 4;
constexpr std::int32_t kWindowChunks = 2 * kWindowRadiusChunks + 1;
constexpr std::int32_t kWindowCells = kWindowChunks * kTerrainChunkCells;
constexpr std::uint64_t kSeedCount = 200;

// Le dessin d'un morceau, la première ligne au nord, comme WalkMap::FromPicture.
[[nodiscard]] std::vector<std::string> Picture(const TerrainChunk& chunk) {
  std::vector<std::string> rows;
  for (std::int32_t north = kTerrainChunkCells - 1; north >= 0; --north) {
    std::string row;
    for (std::int32_t east = 0; east < kTerrainChunkCells; ++east) {
      row.push_back(chunk.Passable(east, north) ? '.' : '#');
    }
    rows.push_back(row);
  }
  return rows;
}

// La fenêtre de morceaux autour du départ, une case par octet, du sud-ouest.
struct Window {
  std::vector<std::uint8_t> passable;
  std::int32_t west = 0;
  std::int32_t south = 0;
  [[nodiscard]] bool At(std::int32_t east, std::int32_t north) const {
    return passable[static_cast<std::size_t>(north) * kWindowCells +
                    static_cast<std::size_t>(east)] != 0U;
  }
};

[[nodiscard]] Window WindowOf(const TerrainRule& rule, std::uint64_t seed) {
  Window window;
  window.passable.assign(static_cast<std::size_t>(kWindowCells) * kWindowCells, 0U);
  const std::int32_t first_chunk = kStartChunk - kWindowRadiusChunks;
  window.west = first_chunk * kTerrainChunkCells;
  window.south = first_chunk * kTerrainChunkCells;
  for (std::int32_t chunk_y = 0; chunk_y < kWindowChunks; ++chunk_y) {
    for (std::int32_t chunk_x = 0; chunk_x < kWindowChunks; ++chunk_x) {
      const TerrainChunk chunk =
          GenerateTerrainChunk(rule, seed, first_chunk + chunk_x, first_chunk + chunk_y);
      for (std::int32_t north = 0; north < kTerrainChunkCells; ++north) {
        for (std::int32_t east = 0; east < kTerrainChunkCells; ++east) {
          const std::size_t cell =
              static_cast<std::size_t>(chunk_y * kTerrainChunkCells + north) * kWindowCells +
              static_cast<std::size_t>(chunk_x * kTerrainChunkCells + east);
          window.passable[cell] = chunk.Passable(east, north) ? 1U : 0U;
        }
      }
    }
  }
  return window;
}

// Les cases libres que l'inondation à quatre voisins depuis (east, north)
// n'atteint pas, sans sortir de la grille.
[[nodiscard]] std::size_t UnreachedFreeCells(const std::vector<std::uint8_t>& passable,
                                             std::int32_t side, std::int32_t east,
                                             std::int32_t north) {
  std::vector<std::uint8_t> reached(passable.size(), 0U);
  std::vector<std::size_t> pending;
  const auto start = static_cast<std::size_t>(north) * static_cast<std::size_t>(side) +
                     static_cast<std::size_t>(east);
  if (passable[start] != 0U) {
    reached[start] = 1U;
    pending.push_back(start);
  }
  const auto width = static_cast<std::size_t>(side);
  while (!pending.empty()) {
    const std::size_t cell = pending.back();
    pending.pop_back();
    const auto visit = [&](std::size_t next) {
      if (passable[next] != 0U && reached[next] == 0U) {
        reached[next] = 1U;
        pending.push_back(next);
      }
    };
    if (cell % width > 0) visit(cell - 1);
    if (cell % width + 1 < width) visit(cell + 1);
    if (cell >= width) visit(cell - width);
    if (cell + width < passable.size()) visit(cell + width);
  }
  std::size_t unreached = 0;
  for (std::size_t cell = 0; cell < passable.size(); ++cell) {
    if (passable[cell] != 0U && reached[cell] == 0U) ++unreached;
  }
  return unreached;
}

[[nodiscard]] std::vector<std::uint8_t> GridOf(std::span<const std::string_view> rows) {
  std::vector<std::uint8_t> grid;
  for (std::size_t row = rows.size(); row-- > 0;) {
    for (const char cell : rows[row]) grid.push_back(cell == '.' ? 1U : 0U);
  }
  return grid;
}

}  // namespace

TEST_CASE("Terrain survivors : un morceau épinglé en dessin, pour deux graines",
          "[sim][survivors]") {
  constexpr std::uint64_t kFirstSeed = 1;
  constexpr std::uint64_t kSecondSeed = 0x5EED'0F'7E44A1ULL;
  // Le même morceau sous MSVC et sous g++ : on compare les cases et les
  // pièces, on ne hache rien.
  const std::vector<std::string> first_picture = {
      "................................", "................................",
      "................................", ".....#..........................",
      "....###.........................", ".....#..........................",
      "................................", "................................",
      "................................", "................................",
      "...........#...............###..", "..........###..............#....",
      "...........#...............#....", "...........................#....",
      "................................", "................................",
      "................................", "................................",
      "................................", "................................",
      "...................#............", "..................###...........",
      "...................#............", "................................",
      "................................", "................................",
      "................................", "................................",
      "................................", "....................##..........",
      "....................##..........", "................................",
  };
  const std::vector<PlacedPiece> first_pieces = {
      {.piece_id = kRockId, .x = 32852, .y = 32737, .quarter = 2},
      {.piece_id = kTreeId, .x = 32850, .y = 32745, .quarter = 3},
      {.piece_id = kTreeId, .x = 32842, .y = 32755, .quarter = 3},
      {.piece_id = kGroveId, .x = 32859, .y = 32754, .quarter = 3},
      {.piece_id = kTreeId, .x = 32836, .y = 32762, .quarter = 3},
  };
  const std::vector<std::string> second_picture = {
      "................................", "................................",
      "................................", "...........................#....",
      "...........##.............###...", "...........##..............#....",
      "................................", "................................",
      "................................", "................................",
      "...........................###..", "...........................#....",
      ".....................##....#....", ".....................##....#....",
      "................................", "................................",
      "................................", "...........................#....",
      "..........................###...", "..........##...............#....",
      "..........##......#.............", ".................###............",
      "..................#.............", "................................",
      "................................", "................................",
      "..#.............................", ".###............................",
      "..#.............................", ".............................##.",
      ".............................##.", "................................",
  };
  const std::vector<PlacedPiece> second_pieces = {
      {.piece_id = kTreeId, .x = 32673, .y = 32835, .quarter = 3},
      {.piece_id = kRockId, .x = 32701, .y = 32833, .quarter = 3},
      {.piece_id = kRockId, .x = 32682, .y = 32843, .quarter = 2},
      {.piece_id = kTreeId, .x = 32689, .y = 32841, .quarter = 2},
      {.piece_id = kTreeId, .x = 32698, .y = 32844, .quarter = 1},
      {.piece_id = kRockId, .x = 32693, .y = 32850, .quarter = 2},
      {.piece_id = kGroveId, .x = 32699, .y = 32850, .quarter = 3},
      {.piece_id = kRockId, .x = 32683, .y = 32858, .quarter = 2},
      {.piece_id = kTreeId, .x = 32698, .y = 32858, .quarter = 2},
  };
  const TerrainChunk first =
      GenerateTerrainChunk(kRule, kFirstSeed, kStartChunk + 2, kStartChunk - 1);
  const TerrainChunk second =
      GenerateTerrainChunk(kRule, kSecondSeed, kStartChunk - 3, kStartChunk + 2);
  CHECK(Picture(first) == first_picture);
  CHECK(first.pieces == first_pieces);
  CHECK(Picture(second) == second_picture);
  CHECK(second.pieces == second_pieces);
}

TEST_CASE("Terrain survivors : l'ordre des demandes ne change aucun morceau", "[sim][survivors]") {
  constexpr std::uint64_t kSeed = 42;
  constexpr std::int32_t kFarX = 5;
  constexpr std::int32_t kFarY = -3;
  const TerrainChunk origin_first = GenerateTerrainChunk(kRule, kSeed, 0, 0);
  const TerrainChunk far_second = GenerateTerrainChunk(kRule, kSeed, kFarX, kFarY);
  const TerrainChunk far_first = GenerateTerrainChunk(kRule, kSeed, kFarX, kFarY);
  const TerrainChunk origin_second = GenerateTerrainChunk(kRule, kSeed, 0, 0);
  CHECK(origin_first == origin_second);
  CHECK(far_first == far_second);
  CHECK_FALSE(origin_first == far_first);
  // La référence case par case lit le même morceau.
  const std::int32_t west = kStartChunk * kTerrainChunkCells;
  const TerrainChunk start = GenerateTerrainChunk(kRule, kSeed, kStartChunk + 1, kStartChunk);
  for (std::int32_t north = 0; north < kTerrainChunkCells; ++north) {
    for (std::int32_t east = 0; east < kTerrainChunkCells; ++east) {
      CHECK(ro::sim::TerrainPassableAt(kRule, kSeed, west + kTerrainChunkCells + east,
                                       west + north) == start.Passable(east, north));
    }
  }
  // Hors du monde, tout est bloqué.
  CHECK_FALSE(ro::sim::TerrainPassableAt(kRule, kSeed, -1, kTerrainStartCell));
  CHECK_FALSE(
      ro::sim::TerrainPassableAt(kRule, kSeed, kTerrainStartCell, ro::sim::kTerrainWorldCells));
  CHECK(ro::sim::TerrainChunkOf(-1) == -1);
  CHECK(ro::sim::TerrainChunkOf(kTerrainChunkCells) == 1);
}

TEST_CASE("Terrain survivors : la graine change le terrain", "[sim][survivors]") {
  const TerrainChunk first = GenerateTerrainChunk(kRule, 1, kStartChunk + 2, kStartChunk + 2);
  const TerrainChunk second = GenerateTerrainChunk(kRule, 2, kStartChunk + 2, kStartChunk + 2);
  CHECK_FALSE(first == second);
  CHECK(ro::sim::TerrainChunkSeed(1, 0, 1) != ro::sim::TerrainChunkSeed(1, 1, 0));
}

TEST_CASE("Terrain survivors : une pièce franchissable se pose sans bloquer de case",
          "[sim][survivors]") {
  // Les mêmes règles, le bosquet franchissable : les mêmes pièces aux mêmes
  // places, et seules les cases de ses emprises deviennent praticables.
  std::array<TerrainPieceRule, kPieces.size()> walkable_pieces = kPieces;
  for (TerrainPieceRule& piece : walkable_pieces) piece.walkable = piece.piece_id == kGroveId;
  TerrainRule walkable_rule = kRule;
  walkable_rule.pieces = walkable_pieces;
  constexpr std::uint64_t kSeeds = 20;
  constexpr std::int32_t kChunksBeside = 2;
  std::size_t groves = 0;
  for (std::uint64_t seed = 1; seed <= kSeeds; ++seed) {
    for (std::int32_t dy = -kChunksBeside; dy <= kChunksBeside; ++dy) {
      for (std::int32_t dx = -kChunksBeside; dx <= kChunksBeside; ++dx) {
        const TerrainChunk blocking =
            GenerateTerrainChunk(kRule, seed, kStartChunk + dx, kStartChunk + dy);
        const TerrainChunk walkable =
            GenerateTerrainChunk(walkable_rule, seed, kStartChunk + dx, kStartChunk + dy);
        REQUIRE(blocking.pieces == walkable.pieces);
        CHECK(blocking.path == walkable.path);
        std::vector<std::uint8_t> under_grove(ro::sim::kTerrainChunkArea, 0U);
        for (const ro::sim::PlacedPiece& placed : walkable.pieces) {
          if (placed.piece_id != kGroveId) continue;
          ++groves;
          const TerrainPieceRule& grove = walkable_pieces[std::size_t{kGroveId} - 1U];
          const FootprintSize size = FootprintSizeOf(grove, placed.quarter);
          for (std::int32_t north = 0; north < size.height; ++north) {
            for (std::int32_t east = 0; east < size.width; ++east) {
              if (!FootprintCovers(grove, placed.quarter, east, north)) continue;
              const std::int32_t x = placed.x - walkable.chunk_x * kTerrainChunkCells + east;
              const std::int32_t y = placed.y - walkable.chunk_y * kTerrainChunkCells + north;
              CHECK(walkable.Passable(x, y));
              CHECK_FALSE(blocking.Passable(x, y));
              under_grove[static_cast<std::size_t>(y * kTerrainChunkCells + x)] = 1U;
            }
          }
        }
        for (std::int32_t y = 0; y < kTerrainChunkCells; ++y) {
          for (std::int32_t x = 0; x < kTerrainChunkCells; ++x) {
            if (under_grove[static_cast<std::size_t>(y * kTerrainChunkCells + x)] != 0U) continue;
            CHECK(walkable.Passable(x, y) == blocking.Passable(x, y));
          }
        }
      }
    }
  }
  CHECK(groves > 0);
}

TEST_CASE("Terrain survivors : depuis le départ, l'inondation atteint toute case libre",
          "[sim][survivors]") {
  // 200 graines × 9 × 9 morceaux, repères compris.
  const std::int32_t start_east = kTerrainStartCell - kStartChunk * kTerrainChunkCells +
                                  kWindowRadiusChunks * kTerrainChunkCells;
  std::size_t landmarks = 0;
  for (std::uint64_t seed = 1; seed <= kSeedCount; ++seed) {
    const Window window = WindowOf(kRule, seed);
    INFO("graine " << seed);
    REQUIRE(window.At(start_east, start_east));
    CHECK(UnreachedFreeCells(window.passable, kWindowCells, start_east, start_east) == 0);
    // La bordure de chaque emplacement est libre : c'est ce qui relie le monde.
    for (std::int32_t north = 0; north < kWindowCells; ++north) {
      for (std::int32_t east = 0; east < kWindowCells; ++east) {
        const std::int32_t slot_east = east % kTerrainSlotCells;
        const std::int32_t slot_north = north % kTerrainSlotCells;
        const bool border = slot_east == 0 || slot_north == 0 ||
                            slot_east == kTerrainSlotCells - 1 ||
                            slot_north == kTerrainSlotCells - 1;
        if (border && !window.At(east, north)) {
          // Seul un repère déborde de la grille des emplacements, jamais de
          // la bordure de son morceau.
          const std::int32_t chunk_east = east % kTerrainChunkCells;
          const std::int32_t chunk_north = north % kTerrainChunkCells;
          REQUIRE(chunk_east != 0);
          REQUIRE(chunk_north != 0);
          REQUIRE(chunk_east != kTerrainChunkCells - 1);
          REQUIRE(chunk_north != kTerrainChunkCells - 1);
        }
      }
    }
    for (std::int32_t chunk_y = 0; chunk_y < kWindowChunks; ++chunk_y) {
      for (std::int32_t chunk_x = 0; chunk_x < kWindowChunks; ++chunk_x) {
        const TerrainChunk chunk =
            GenerateTerrainChunk(kRule, seed, kStartChunk - kWindowRadiusChunks + chunk_x,
                                 kStartChunk - kWindowRadiusChunks + chunk_y);
        for (const PlacedPiece& piece : chunk.pieces) {
          if (piece.piece_id == kWindmillId) ++landmarks;
        }
      }
    }
  }
  // L'essai a vu des repères : leur grand emplacement est éprouvé aussi.
  CHECK(landmarks > kSeedCount);
}

TEST_CASE("Terrain survivors : l'inondation de l'essai voit une case enclavée",
          "[sim][survivors]") {
  constexpr std::array<std::string_view, 5> kEnclave = {
      ".....", ".###.", ".#.#.", ".###.", ".....",
  };
  constexpr std::int32_t kSide = 5;
  CHECK(UnreachedFreeCells(GridOf(kEnclave), kSide, 0, 0) == 1);
}

TEST_CASE("Terrain survivors : aucune case bloquée dans la clairière du départ",
          "[sim][survivors]") {
  for (std::uint64_t seed = 1; seed <= kSeedCount; ++seed) {
    INFO("graine " << seed);
    for (std::int32_t chunk_y = kStartChunk - 1; chunk_y <= kStartChunk; ++chunk_y) {
      for (std::int32_t chunk_x = kStartChunk - 1; chunk_x <= kStartChunk; ++chunk_x) {
        const TerrainChunk chunk = GenerateTerrainChunk(kRule, seed, chunk_x, chunk_y);
        for (std::int32_t north = 0; north < kTerrainChunkCells; ++north) {
          for (std::int32_t east = 0; east < kTerrainChunkCells; ++east) {
            const std::int32_t x = chunk_x * kTerrainChunkCells + east;
            const std::int32_t y = chunk_y * kTerrainChunkCells + north;
            const std::int32_t distance =
                std::max(std::abs(x - kTerrainStartCell), std::abs(y - kTerrainStartCell));
            if (distance < kClearingCells) REQUIRE(chunk.Passable(east, north));
          }
        }
      }
    }
  }
}

TEST_CASE("Terrain survivors : la densité suit la règle", "[sim][survivors]") {
  constexpr std::uint64_t kSeed = 7;
  SECTION("nulle : aucune pièce, aucun repère") {
    TerrainRule empty = kRule;
    empty.density_min_per_mille = 0;
    empty.density_max_per_mille = 0;
    empty.landmark_period_chunks = 0;
    const Window window = WindowOf(empty, kSeed);
    CHECK(std::all_of(window.passable.begin(), window.passable.end(),
                      [](std::uint8_t cell) { return cell != 0U; }));
  }
  SECTION("pleine : une pièce par emplacement hors de la clairière") {
    TerrainRule full = kRule;
    full.density_min_per_mille = ro::sim::kTerrainPerMille;
    full.density_max_per_mille = ro::sim::kTerrainPerMille;
    full.landmark_period_chunks = 0;
    full.clearing_cells = 0;
    const TerrainChunk chunk = GenerateTerrainChunk(full, kSeed, kStartChunk, kStartChunk);
    CHECK(chunk.pieces.size() ==
          static_cast<std::size_t>(ro::sim::kTerrainSlotsPerSide * ro::sim::kTerrainSlotsPerSide));
  }
  SECTION("variable : chaque emplacement dans [min, max], la moyenne au milieu") {
    std::uint64_t slots = 0;
    std::uint64_t pieces = 0;
    std::uint32_t lowest = ro::sim::kTerrainPerMille;
    std::uint32_t highest = 0;
    TerrainRule plain = kRule;
    plain.landmark_period_chunks = 0;
    plain.clearing_cells = 0;
    for (std::uint64_t seed = 1; seed <= kSeedCount; ++seed) {
      for (std::int32_t chunk_x = 0; chunk_x < kWindowChunks; ++chunk_x) {
        const TerrainChunk chunk = GenerateTerrainChunk(plain, seed, chunk_x, 0);
        pieces += chunk.pieces.size();
        slots += ro::sim::kTerrainSlotsPerSide * ro::sim::kTerrainSlotsPerSide;
        const std::uint32_t density =
            ro::sim::TerrainDensityPerMille(plain, seed, chunk_x * kTerrainChunkCells, 0);
        lowest = std::min(lowest, density);
        highest = std::max(highest, density);
      }
    }
    CHECK(lowest >= plain.density_min_per_mille);
    CHECK(highest <= plain.density_max_per_mille);
    // Des clairières et des bois : la densité varie vraiment.
    CHECK(highest - lowest > (plain.density_max_per_mille - plain.density_min_per_mille) / 2);
    // La part d'emplacements occupés, sur 28 800, autour de la moyenne des
    // nœuds (550 pour mille), largement dans les bornes.
    const std::uint64_t share = pieces * ro::sim::kTerrainPerMille / slots;
    CHECK(share > plain.density_min_per_mille + 100);
    CHECK(share < plain.density_max_per_mille - 100);
  }
}

TEST_CASE("Terrain survivors : un repère par carré, seul dans son morceau", "[sim][survivors]") {
  constexpr std::uint64_t kSeed = 11;
  // Un carré loin de la clairière.
  constexpr std::int32_t kBlock = kStartChunk / kLandmarkPeriod + 2;
  std::size_t hosts = 0;
  for (std::int32_t chunk_y = 0; chunk_y < kLandmarkPeriod; ++chunk_y) {
    for (std::int32_t chunk_x = 0; chunk_x < kLandmarkPeriod; ++chunk_x) {
      const TerrainChunk chunk = GenerateTerrainChunk(
          kRule, kSeed, kBlock * kLandmarkPeriod + chunk_x, kBlock * kLandmarkPeriod + chunk_y);
      const bool hosts_landmark =
          std::any_of(chunk.pieces.begin(), chunk.pieces.end(),
                      [](const PlacedPiece& piece) { return piece.piece_id == kWindmillId; });
      if (!hosts_landmark) continue;
      ++hosts;
      CHECK(chunk.pieces.size() == 1);
      // Sa bordure d'une case est libre.
      for (std::int32_t cell = 0; cell < kTerrainChunkCells; ++cell) {
        CHECK(chunk.Passable(cell, 0));
        CHECK(chunk.Passable(cell, kTerrainChunkCells - 1));
        CHECK(chunk.Passable(0, cell));
        CHECK(chunk.Passable(kTerrainChunkCells - 1, cell));
      }
    }
  }
  CHECK(hosts == 1);
}

TEST_CASE("Terrain survivors : le quart de tour d'une emprise", "[sim][survivors]") {
  const TerrainPieceRule& grove = kPieces[2];
  CHECK(FootprintSizeOf(grove, 0) == FootprintSize{.width = 4, .height = 3});
  CHECK(FootprintSizeOf(grove, 1) == FootprintSize{.width = 3, .height = 4});
  // Le dessin tourné, la première ligne au nord, pour chaque quart.
  const std::array<std::vector<std::string>, 4> expected = {{
      {"#...", "#...", "####"},
      {"..#", "..#", "..#", "###"},
      {"####", "...#", "...#"},
      {"###", "#..", "#..", "#.."},
  }};
  for (std::uint8_t quarter = 0; quarter < ro::sim::kTerrainQuarterTurns; ++quarter) {
    INFO("quart " << unsigned{quarter});
    const FootprintSize size = FootprintSizeOf(grove, quarter);
    std::vector<std::string> rows;
    for (std::int32_t north = size.height - 1; north >= 0; --north) {
      std::string row;
      for (std::int32_t east = 0; east < size.width; ++east) {
        row.push_back(FootprintCovers(grove, quarter, east, north) ? '#' : '.');
      }
      rows.push_back(row);
    }
    CHECK(rows == expected[quarter]);
    CHECK_FALSE(FootprintCovers(grove, quarter, -1, 0));
    CHECK_FALSE(FootprintCovers(grove, quarter, size.width, 0));
  }
}

// ── Le contrôle des règles de terrain ───────────────────────────────────────

namespace {

[[nodiscard]] bool Refused(const ro::sim::SurvivorsRules& rules) {
  const ro::core::Result<void> checked = CheckSurvivorsRules(rules);
  return !checked && checked.error().code == ro::core::ErrorCode::kInvalidArgument &&
         !checked.error().detail.empty();
}

[[nodiscard]] bool Accepted(const ro::sim::SurvivorsRules& rules) {
  const ro::core::Result<void> checked = CheckSurvivorsRules(rules);
  INFO((checked ? std::string_view{} : std::string_view{checked.error().detail}));
  return checked.has_value();
}

// Les règles minuscules, un terrain et une arène qui le nomme. Les spans
// pointent dans l'objet : il ne se copie pas.
struct TerrainVariant {
  std::array<TerrainPieceRule, 4> pieces = kPieces;
  std::array<TerrainRule, 1> terrains{kRule};
  std::array<ro::sim::ArenaRule, 1> arenas = ro::test_support::kTinyArenas;
  ro::sim::SurvivorsRules rules = ro::test_support::TinyRules();

  TerrainVariant() {
    terrains[0].pieces = pieces;
    arenas[0].terrain_id = kTerrainId;
    rules.terrains = terrains;
    rules.arenas = arenas;
  }
  TerrainVariant(const TerrainVariant&) = delete;
  TerrainVariant& operator=(const TerrainVariant&) = delete;
};

}  // namespace

TEST_CASE("Terrain survivors : les règles d'essai sont acceptées, et inertes sans terrain",
          "[sim][survivors]") {
  CHECK(Accepted(ro::test_support::TinyRules()));
  const TerrainVariant variant;
  CHECK(Accepted(variant.rules));
}

TEST_CASE("Terrain survivors : chaque emprise fautive est refusée", "[sim][survivors]") {
  constexpr std::array<std::string_view, 1> kNoBlock = {"..."};
  constexpr std::array<std::string_view, 2> kUneven = {"##", "#"};
  constexpr std::array<std::string_view, 1> kStrange = {"#x"};
  constexpr std::array<std::string_view, 1> kTooWide = {"#######"};
  constexpr std::array<std::string_view, 6> kWidest = {
      "######", "######", "######", "######", "######", "######",
  };
  constexpr std::array<std::string_view, 3> kHole = {"###", "#.#", "###"};
  constexpr std::array<std::string_view, 3> kNotch = {"###", "#.#", "#.#"};
  constexpr std::array<std::string_view, 1> kEmptyRow = {""};
  const auto with_footprint = [](std::span<const std::string_view> footprint) {
    TerrainVariant variant;
    variant.pieces[0].footprint = footprint;
    return Accepted(variant.rules);
  };
  CHECK_FALSE(with_footprint({}));
  CHECK_FALSE(with_footprint(kEmptyRow));
  CHECK_FALSE(with_footprint(kNoBlock));
  CHECK_FALSE(with_footprint(kUneven));
  CHECK_FALSE(with_footprint(kStrange));
  CHECK_FALSE(with_footprint(kTooWide));
  CHECK_FALSE(with_footprint(kHole));
  // À la borne, et une encoche ouverte sur le bord, sont acceptées.
  CHECK(with_footprint(kWidest));
  CHECK(with_footprint(kNotch));
}

TEST_CASE("Terrain survivors : un repère tient dans son grand emplacement", "[sim][survivors]") {
  std::vector<std::string> widest(ro::sim::kMostLandmarkSideCells,
                                  std::string(ro::sim::kMostLandmarkSideCells, '#'));
  const std::vector<std::string> too_wide(ro::sim::kMostLandmarkSideCells + 1,
                                          std::string(ro::sim::kMostLandmarkSideCells + 1, '#'));
  const std::vector<std::string_view> widest_rows(widest.begin(), widest.end());
  const std::vector<std::string_view> too_wide_rows(too_wide.begin(), too_wide.end());
  {
    TerrainVariant variant;
    variant.pieces[3].footprint = widest_rows;
    CHECK(Accepted(variant.rules));
  }
  {
    TerrainVariant variant;
    variant.pieces[3].footprint = too_wide_rows;
    CHECK(Refused(variant.rules));
  }
  {
    // Une pièce ordinaire n'a pas droit au grand emplacement.
    TerrainVariant variant;
    variant.pieces[0].footprint = widest_rows;
    CHECK(Refused(variant.rules));
  }
  {
    TerrainVariant variant;
    variant.terrains[0].landmark_period_chunks = 0;
    CHECK(Refused(variant.rules));
  }
  {
    TerrainVariant variant;
    variant.pieces[3].weight = 0;
    CHECK(Refused(variant.rules));
  }
  {
    // Une période sans pièce repère laisserait nu le morceau élu de chaque
    // carré ; sans période, les mêmes pièces passent.
    TerrainVariant variant;
    variant.terrains[0].pieces = std::span<const TerrainPieceRule>(variant.pieces).first(3);
    CHECK(Refused(variant.rules));
    variant.terrains[0].landmark_period_chunks = 0;
    CHECK(Accepted(variant.rules));
  }
}

TEST_CASE("Terrain survivors : les poids, densités, périodes et clairière bornés",
          "[sim][survivors]") {
  const auto refused = [](auto&& retouch) {
    TerrainVariant variant;
    retouch(variant);
    return Refused(variant.rules);
  };
  const auto accepted = [](auto&& retouch) {
    TerrainVariant variant;
    retouch(variant);
    return Accepted(variant.rules);
  };
  CHECK(refused([](TerrainVariant& v) {
    for (std::size_t index = 0; index < 3; ++index) v.pieces[index].weight = 0;
  }));
  CHECK(refused([](TerrainVariant& v) { v.terrains[0].density_max_per_mille = 1001; }));
  CHECK(refused([](TerrainVariant& v) {
    v.terrains[0].density_min_per_mille = 500;
    v.terrains[0].density_max_per_mille = 400;
  }));
  CHECK(refused([](TerrainVariant& v) { v.terrains[0].density_period_chunks = 0; }));
  CHECK(refused([](TerrainVariant& v) {
    v.terrains[0].density_period_chunks = ro::sim::kTerrainWorldChunks + 1;
  }));
  CHECK(refused([](TerrainVariant& v) {
    v.terrains[0].landmark_period_chunks = ro::sim::kTerrainWorldChunks + 1;
  }));
  CHECK(refused(
      [](TerrainVariant& v) { v.terrains[0].clearing_cells = ro::sim::kMostArenaSideCells + 1; }));
  CHECK(refused([](TerrainVariant& v) { v.terrains[0].clearing_cells = -1; }));
  // Aux bornes.
  CHECK(accepted([](TerrainVariant& v) {
    v.terrains[0].density_min_per_mille = ro::sim::kTerrainPerMille;
    v.terrains[0].density_max_per_mille = ro::sim::kTerrainPerMille;
    v.terrains[0].density_period_chunks = ro::sim::kTerrainWorldChunks;
    v.terrains[0].landmark_period_chunks = ro::sim::kTerrainWorldChunks;
    v.terrains[0].clearing_cells = ro::sim::kMostArenaSideCells;
  }));
  CHECK(accepted([](TerrainVariant& v) {
    v.terrains[0].density_min_per_mille = 0;
    v.terrains[0].density_max_per_mille = 0;
    v.terrains[0].clearing_cells = 0;
  }));
}

TEST_CASE("Terrain survivors : Id et clés en double, terrain inconnu", "[sim][survivors]") {
  const auto refused = [](auto&& retouch) {
    TerrainVariant variant;
    retouch(variant);
    return Refused(variant.rules);
  };
  CHECK(refused([](TerrainVariant& v) { v.pieces[1].piece_id = kRockId; }));
  CHECK(refused([](TerrainVariant& v) { v.pieces[1].key = "rock"; }));
  CHECK(refused([](TerrainVariant& v) { v.terrains[0].terrain_id = 0; }));
  CHECK(refused([](TerrainVariant& v) { v.arenas[0].terrain_id = kTerrainId + 1; }));
  // Une arène générée naît au centre du monde : un départ à elle tomberait
  // hors de sa clairière.
  CHECK(refused([](TerrainVariant& v) {
    v.arenas[0].start = ro::sim::CellPosition{kTerrainStartCell, kTerrainStartCell};
  }));
  // Deux terrains d'un même Id, puis d'une même clé.
  std::array<TerrainRule, 2> twins{kRule, kRule};
  {
    TerrainVariant variant;
    twins[0].pieces = variant.pieces;
    twins[1].pieces = variant.pieces;
    variant.rules.terrains = twins;
    CHECK(Refused(variant.rules));
    twins[1].terrain_id = kTerrainId + 1;
    CHECK(Refused(variant.rules));
    twins[1].key = "other_plains";
    CHECK(Accepted(variant.rules));
  }
}

// Un banc, lancé à la main : il mesure, il ne juge pas. Ses chiffres ne valent
// que pour la configuration bâtie (Debug ou Release), qu'il faut citer avec eux.
TEST_CASE("Banc survivors : générer un morceau de terrain", "[sim][survivors][.bench]") {
  constexpr std::int32_t kSide = 64;
  constexpr double kMicrosecondsPerSecond = 1e6;
  std::size_t pieces = 0;
  const auto begin = std::chrono::steady_clock::now();
  for (std::int32_t chunk_y = 0; chunk_y < kSide; ++chunk_y) {
    for (std::int32_t chunk_x = 0; chunk_x < kSide; ++chunk_x) {
      pieces += GenerateTerrainChunk(kRule, 1, kStartChunk + chunk_x, kStartChunk + chunk_y)
                    .pieces.size();
    }
  }
  const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - begin;
  std::printf("%d morceaux, %zu pièces, %.2f µs par morceau\n", kSide * kSide, pieces,
              elapsed.count() * kMicrosecondsPerSecond / (kSide * kSide));
  CHECK(pieces > 0);
}
