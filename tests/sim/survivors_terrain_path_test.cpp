// LES CHEMINS DU TERRAIN GÉNÉRÉ : un réseau de carrefours reliés à l'est et au
// nord, que chaque morceau calcule seul. Un chemin ne fait que libérer des
// cases — aucune pièce ordinaire ne le touche, aucun tirage ne se décale —, si
// bien que la connexité tient ; et deux carrefours voisins se rejoignent par
// des cases de chemin. Un repère se dresse au bord de la place de terre qui
// sert de carrefour à sa cellule, et aucun chemin n'entre sous son emprise.

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/error.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_terrain.h"

using ro::sim::FootprintSize;
using ro::sim::FootprintSizeOf;
using ro::sim::GenerateTerrainChunk;
using ro::sim::kTerrainChunkCells;
using ro::sim::kTerrainStartCell;
using ro::sim::PlacedPiece;
using ro::sim::TerrainCell;
using ro::sim::TerrainChunk;
using ro::sim::TerrainPieceRule;
using ro::sim::TerrainRule;

namespace {

constexpr std::int32_t kStartChunk = kTerrainStartCell / kTerrainChunkCells;
// La fenêtre des essais : 12 × 12 morceaux autour du départ, quatre cellules
// de chemins de côté pour la prairie.
constexpr std::int32_t kWindowChunks = 12;
constexpr std::int32_t kFirstChunk = kStartChunk - kWindowChunks / 2;
constexpr std::int32_t kWindowCells = kWindowChunks * kTerrainChunkCells;
constexpr std::uint64_t kSeedCount = 40;

[[nodiscard]] const TerrainRule& Plains() {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  REQUIRE_FALSE(rules.terrains.empty());
  return rules.terrains.front();
}

[[nodiscard]] const TerrainPieceRule* PieceOf(const TerrainRule& rule, std::uint16_t piece_id) {
  for (const TerrainPieceRule& piece : rule.pieces) {
    if (piece.piece_id == piece_id) return &piece;
  }
  return nullptr;
}

// Vrai si le rectangle d'emprise de la pièce posée touche un chemin du morceau.
[[nodiscard]] bool RectTouchesPath(const TerrainRule& rule, const TerrainChunk& chunk,
                                   const PlacedPiece& placed) {
  const TerrainPieceRule* piece = PieceOf(rule, placed.piece_id);
  REQUIRE(piece != nullptr);
  const FootprintSize size = FootprintSizeOf(*piece, placed.quarter);
  const std::int32_t west = placed.x - chunk.chunk_x * kTerrainChunkCells;
  const std::int32_t south = placed.y - chunk.chunk_y * kTerrainChunkCells;
  for (std::int32_t north = 0; north < size.height; ++north) {
    for (std::int32_t east = 0; east < size.width; ++east) {
      if (chunk.OnPath(west + east, south + north)) return true;
    }
  }
  return false;
}

// Les cases de la fenêtre, une par octet du sud-ouest : praticables, de chemin.
struct Window {
  std::vector<std::uint8_t> passable;
  std::vector<std::uint8_t> path;
  [[nodiscard]] std::size_t Index(std::int64_t x, std::int64_t y) const {
    return static_cast<std::size_t>(y - std::int64_t{kFirstChunk} * kTerrainChunkCells) *
               kWindowCells +
           static_cast<std::size_t>(x - std::int64_t{kFirstChunk} * kTerrainChunkCells);
  }
  [[nodiscard]] bool Inside(std::int64_t x, std::int64_t y) const {
    const std::int64_t west = std::int64_t{kFirstChunk} * kTerrainChunkCells;
    return x >= west && y >= west && x < west + kWindowCells && y < west + kWindowCells;
  }
};

[[nodiscard]] Window WindowOf(const TerrainRule& rule, std::uint64_t seed) {
  Window window;
  window.passable.assign(static_cast<std::size_t>(kWindowCells) * kWindowCells, 0U);
  window.path.assign(window.passable.size(), 0U);
  for (std::int32_t row = 0; row < kWindowChunks; ++row) {
    for (std::int32_t column = 0; column < kWindowChunks; ++column) {
      const TerrainChunk chunk =
          GenerateTerrainChunk(rule, seed, kFirstChunk + column, kFirstChunk + row);
      for (std::int32_t north = 0; north < kTerrainChunkCells; ++north) {
        for (std::int32_t east = 0; east < kTerrainChunkCells; ++east) {
          const std::size_t cell =
              static_cast<std::size_t>(row * kTerrainChunkCells + north) * kWindowCells +
              static_cast<std::size_t>(column * kTerrainChunkCells + east);
          window.passable[cell] = chunk.Passable(east, north) ? 1U : 0U;
          window.path[cell] = chunk.OnPath(east, north) ? 1U : 0U;
        }
      }
    }
  }
  return window;
}

// Vrai si l'inondation à quatre voisins par les seules cases de chemin mène de
// `from` à `to`.
[[nodiscard]] bool PathJoins(const Window& window, TerrainCell from, TerrainCell to) {
  if (!window.Inside(from.x, from.y) || !window.Inside(to.x, to.y)) return false;
  std::vector<std::uint8_t> reached(window.path.size(), 0U);
  std::vector<TerrainCell> pending;
  if (window.path[window.Index(from.x, from.y)] == 0U) return false;
  reached[window.Index(from.x, from.y)] = 1U;
  pending.push_back(from);
  constexpr std::array<std::array<std::int64_t, 2>, 4> kSteps{{{1, 0}, {-1, 0}, {0, 1}, {0, -1}}};
  while (!pending.empty()) {
    const TerrainCell cell = pending.back();
    pending.pop_back();
    if (cell == to) return true;
    for (const auto& step : kSteps) {
      const TerrainCell next{.x = cell.x + step[0], .y = cell.y + step[1]};
      if (!window.Inside(next.x, next.y)) continue;
      const std::size_t index = window.Index(next.x, next.y);
      if (window.path[index] == 0U || reached[index] != 0U) continue;
      reached[index] = 1U;
      pending.push_back(next);
    }
  }
  return false;
}

[[nodiscard]] bool Refused(const ro::sim::SurvivorsRules& rules) {
  const ro::core::Result<void> checked = ro::sim::CheckSurvivorsRules(rules);
  return !checked && checked.error().code == ro::core::ErrorCode::kInvalidArgument;
}

}  // namespace

TEST_CASE("Chemins survivors : la prairie en trace, toujours praticables, sans pièce dessus",
          "[sim][survivors]") {
  const TerrainRule& plains = Plains();
  REQUIRE(plains.path_period_chunks > 0);
  std::size_t path_cells = 0;
  std::size_t missing_pieces = 0;
  std::size_t kept_landmarks = 0;
  std::size_t dropped_landmarks = 0;
  for (std::uint64_t seed = 1; seed <= kSeedCount; ++seed) {
    INFO("graine " << seed);
    TerrainRule without_paths = plains;
    without_paths.path_period_chunks = 0;
    for (std::int32_t row = 0; row < kWindowChunks; ++row) {
      for (std::int32_t column = 0; column < kWindowChunks; ++column) {
        const TerrainChunk chunk =
            GenerateTerrainChunk(plains, seed, kFirstChunk + column, kFirstChunk + row);
        for (std::size_t cell = 0; cell < chunk.path.size(); ++cell) {
          if (chunk.path[cell] == 0U) continue;
          ++path_cells;
          REQUIRE(chunk.passable[cell] != 0U);
        }
        for (const PlacedPiece& placed : chunk.pieces) {
          const TerrainPieceRule* piece = PieceOf(plains, placed.piece_id);
          REQUIRE(piece != nullptr);
          if (!piece->landmark) CHECK_FALSE(RectTouchesPath(plains, chunk, placed));
        }
        // Les chemins ne décalent aucun tirage : les pièces ordinaires sont
        // celles du même morceau sans chemin, moins celles qui en toucheraient
        // un. Les repères, eux, se calent sur les places.
        const TerrainChunk bare =
            GenerateTerrainChunk(without_paths, seed, kFirstChunk + column, kFirstChunk + row);
        CHECK(std::ranges::all_of(bare.path, [](std::uint8_t cell) { return cell == 0U; }));
        const auto ordinary = [&plains](const PlacedPiece& placed) {
          return !PieceOf(plains, placed.piece_id)->landmark;
        };
        for (const PlacedPiece& placed : bare.pieces) {
          if (!ordinary(placed)) continue;
          if (std::ranges::find(chunk.pieces, placed) != chunk.pieces.end()) continue;
          ++missing_pieces;
          CHECK(RectTouchesPath(plains, chunk, placed));
        }
        // Le second élu d'une cellule de chemins ne porte pas de repère : il
        // reçoit ses pièces ordinaires, que le morceau nu ne pose pas.
        const bool landmark_dropped = !std::ranges::all_of(bare.pieces, ordinary) &&
                                      std::ranges::all_of(chunk.pieces, ordinary);
        if (landmark_dropped) {
          ++dropped_landmarks;
          continue;
        }
        if (!std::ranges::all_of(chunk.pieces, ordinary)) ++kept_landmarks;
        for (const PlacedPiece& placed : chunk.pieces) {
          if (!ordinary(placed)) continue;
          CHECK(std::ranges::find(bare.pieces, placed) != bare.pieces.end());
        }
      }
    }
  }
  CHECK(path_cells > 0);
  CHECK(missing_pieces > 0);
  // Deux élus dans une même cellule de chemins restent l'exception.
  CHECK(dropped_landmarks < kept_landmarks / 2);
}

TEST_CASE("Chemins survivors : deux carrefours voisins se rejoignent par le chemin",
          "[sim][survivors]") {
  const TerrainRule& plains = Plains();
  // Les cellules entières de la fenêtre : un chemin ne quitte pas les deux
  // cellules qu'il relie, il reste donc dans la fenêtre.
  const std::int64_t first_cell =
      ro::sim::TerrainPathCellOf(plains, kFirstChunk + plains.path_period_chunks - 1);
  const std::int64_t last_cell =
      ro::sim::TerrainPathCellOf(plains, kFirstChunk + kWindowChunks) - 1;
  std::size_t joined = 0;
  // Les jonctions qui passent par une porte : en sortant d'une place dont le
  // site est du côté du chemin, en entrant dans une place dont il ne l'est pas ;
  // vers l'est, puis vers le nord.
  std::array<std::size_t, 2> gates_out{};
  std::array<std::size_t, 2> gates_in{};
  for (std::uint64_t seed = 1; seed <= kSeedCount; ++seed) {
    INFO("graine " << seed);
    const Window window = WindowOf(plains, seed);
    for (std::int64_t cell_y = first_cell; cell_y <= last_cell; ++cell_y) {
      for (std::int64_t cell_x = first_cell; cell_x <= last_cell; ++cell_x) {
        const TerrainCell here = ro::sim::TerrainPathCrossing(plains, seed, cell_x, cell_y);
        if (!window.Inside(here.x, here.y)) continue;
        const ro::sim::TerrainPlaza plaza_here =
            ro::sim::TerrainPathPlaza(plains, seed, cell_x, cell_y);
        for (const auto& [next_x, next_y] : std::array<std::array<std::int64_t, 2>, 2>{
                 {{cell_x + 1, cell_y}, {cell_x, cell_y + 1}}}) {
          if (next_x > last_cell || next_y > last_cell) continue;
          const bool eastward = next_x != cell_x;
          const std::size_t axis = eastward ? 0U : 1U;
          const TerrainCell there = ro::sim::TerrainPathCrossing(plains, seed, next_x, next_y);
          if (!window.Inside(there.x, there.y)) continue;
          INFO("de (" << here.x << ", " << here.y << ") à (" << there.x << ", " << there.y << ")");
          CHECK(PathJoins(window, here, there));
          ++joined;
          const ro::sim::TerrainPlaza plaza_there =
              ro::sim::TerrainPathPlaza(plains, seed, next_x, next_y);
          if (plaza_here.exists && (eastward ? plaza_here.site_east : plaza_here.site_north)) {
            ++gates_out[axis];
          }
          if (plaza_there.exists && !(eastward ? plaza_there.site_east : plaza_there.site_north)) {
            ++gates_in[axis];
          }
        }
      }
    }
  }
  CHECK(joined > kSeedCount);
  // Les chemins d'une cellule à repère mènent à sa place, par une porte dans
  // chacun des quatre cas.
  for (std::size_t axis = 0; axis < gates_out.size(); ++axis) {
    INFO("axe " << axis);
    CHECK(gates_out[axis] > 0);
    CHECK(gates_in[axis] > 0);
  }
}

TEST_CASE("Chemins survivors : un repère se dresse au bord de sa place, hors de tout chemin",
          "[sim][survivors]") {
  const TerrainRule& plains = Plains();
  constexpr std::int32_t kChunkCentre = kTerrainChunkCells / 2;
  // Le disque inscrit de la place : ses cases sont toutes de terre.
  constexpr std::int32_t kPlazaRadius = ro::sim::kTerrainPlazaCells / 2;
  std::size_t landmarks = 0;
  std::size_t plazas = 0;
  for (std::uint64_t seed = 1; seed <= kSeedCount; ++seed) {
    INFO("graine " << seed);
    for (std::int32_t row = 0; row < kWindowChunks; ++row) {
      for (std::int32_t column = 0; column < kWindowChunks; ++column) {
        const TerrainChunk chunk =
            GenerateTerrainChunk(plains, seed, kFirstChunk + column, kFirstChunk + row);
        INFO("morceau (" << chunk.chunk_x << ", " << chunk.chunk_y << ")");
        const ro::sim::TerrainPlaza plaza = ro::sim::TerrainPathPlaza(
            plains, seed, ro::sim::TerrainPathCellOf(plains, chunk.chunk_x),
            ro::sim::TerrainPathCellOf(plains, chunk.chunk_y));
        const TerrainCell centre{
            .x = std::int64_t{chunk.chunk_x} * kTerrainChunkCells + kChunkCentre,
            .y = std::int64_t{chunk.chunk_y} * kTerrainChunkCells + kChunkCentre};
        const bool plaza_here = plaza.exists && plaza.centre == centre;
        std::size_t landmarks_here = 0;
        for (const PlacedPiece& placed : chunk.pieces) {
          const TerrainPieceRule* piece = PieceOf(plains, placed.piece_id);
          const FootprintSize size = FootprintSizeOf(*piece, placed.quarter);
          const std::int32_t west = placed.x - chunk.chunk_x * kTerrainChunkCells;
          const std::int32_t south = placed.y - chunk.chunk_y * kTerrainChunkCells;
          if (plaza_here) {
            // Aucune pièce sur la place.
            for (std::int32_t north = 0; north < size.height; ++north) {
              for (std::int32_t east = 0; east < size.width; ++east) {
                const std::int32_t dx = west + east - kChunkCentre;
                const std::int32_t dy = south + north - kChunkCentre;
                CHECK(dx * dx + dy * dy > kPlazaRadius * kPlazaRadius);
              }
            }
          }
          if (!piece->landmark) continue;
          ++landmarks_here;
          // Un repère est celui de la place de sa cellule, aucun chemin ni
          // case de place sous son emprise, et il borde la place du côté de
          // son site.
          REQUIRE(plaza_here);
          CHECK_FALSE(RectTouchesPath(plains, chunk, placed));
          if (plaza.site_east) {
            CHECK(west == kChunkCentre + ro::sim::kTerrainSiteOffsetCells);
          } else {
            CHECK(west + size.width - 1 == kChunkCentre - ro::sim::kTerrainSiteOffsetCells);
          }
          if (plaza.site_north) {
            CHECK(south == kChunkCentre + ro::sim::kTerrainSiteOffsetCells);
          } else {
            CHECK(south + size.height - 1 == kChunkCentre - ro::sim::kTerrainSiteOffsetCells);
          }
        }
        if (!plaza_here) {
          CHECK(landmarks_here == 0);
          continue;
        }
        // Une place porte son repère, et elle est toute de terre praticable.
        ++plazas;
        landmarks += landmarks_here;
        CHECK(landmarks_here == 1);
        for (std::int32_t dy = -kPlazaRadius; dy <= kPlazaRadius; ++dy) {
          for (std::int32_t dx = -kPlazaRadius; dx <= kPlazaRadius; ++dx) {
            if (dx * dx + dy * dy > kPlazaRadius * kPlazaRadius) continue;
            CHECK(chunk.OnPath(kChunkCentre + dx, kChunkCentre + dy));
            CHECK(chunk.Passable(kChunkCentre + dx, kChunkCentre + dy));
          }
        }
      }
    }
  }
  CHECK(plazas > kSeedCount);
  CHECK(landmarks == plazas);
}

// Un repère efface le chemin sous ses cases bloquées : sous la règle livrée, un
// chemin qui entrerait dans le site s'y cacherait. Ce repère creux couvre le
// site entier de cases libres et n'en bloque qu'une, si bien que tout chemin
// tracé dans le site s'y voit.
TEST_CASE("Chemins survivors : aucun chemin n'entre dans le site d'un repère", "[sim][survivors]") {
  constexpr auto kSiteSide = static_cast<std::size_t>(ro::sim::kTerrainLandmarkSiteCells);
  std::vector<std::string> rows(kSiteSide, std::string(kSiteSide, '.'));
  rows.front().front() = '#';
  const std::vector<std::string_view> hollow(rows.begin(), rows.end());
  std::vector<TerrainPieceRule> pieces(Plains().pieces.begin(), Plains().pieces.end());
  for (TerrainPieceRule& piece : pieces) {
    if (piece.landmark) piece.footprint = hollow;
  }
  const ro::sim::SurvivorsRules& shipped = ro::sim::ShippedSurvivorsRules();
  std::vector<TerrainRule> terrains(shipped.terrains.begin(), shipped.terrains.end());
  terrains.front().pieces = pieces;
  ro::sim::SurvivorsRules rules = shipped;
  rules.terrains = terrains;
  REQUIRE_FALSE(Refused(rules));
  const TerrainRule& rule = terrains.front();
  constexpr std::int32_t kChunkCentre = kTerrainChunkCells / 2;
  std::size_t sites = 0;
  for (std::uint64_t seed = 1; seed <= kSeedCount; ++seed) {
    INFO("graine " << seed);
    for (std::int32_t row = 0; row < kWindowChunks; ++row) {
      for (std::int32_t column = 0; column < kWindowChunks; ++column) {
        const TerrainChunk chunk =
            GenerateTerrainChunk(rule, seed, kFirstChunk + column, kFirstChunk + row);
        const ro::sim::TerrainPlaza plaza =
            ro::sim::TerrainPathPlaza(rule, seed, ro::sim::TerrainPathCellOf(rule, chunk.chunk_x),
                                      ro::sim::TerrainPathCellOf(rule, chunk.chunk_y));
        if (!plaza.exists ||
            plaza.centre !=
                TerrainCell{.x = std::int64_t{chunk.chunk_x} * kTerrainChunkCells + kChunkCentre,
                            .y = std::int64_t{chunk.chunk_y} * kTerrainChunkCells + kChunkCentre}) {
          continue;
        }
        INFO("morceau (" << chunk.chunk_x << ", " << chunk.chunk_y << ")");
        ++sites;
        // Le site, recalculé depuis la place : son coin le plus proche du
        // centre s'en écarte de kTerrainSiteOffsetCells sur chaque axe.
        const std::int32_t west = plaza.site_east
                                      ? kChunkCentre + ro::sim::kTerrainSiteOffsetCells
                                      : kChunkCentre - ro::sim::kTerrainSiteOffsetCells -
                                            ro::sim::kTerrainLandmarkSiteCells + 1;
        const std::int32_t south = plaza.site_north
                                       ? kChunkCentre + ro::sim::kTerrainSiteOffsetCells
                                       : kChunkCentre - ro::sim::kTerrainSiteOffsetCells -
                                             ro::sim::kTerrainLandmarkSiteCells + 1;
        std::size_t path_in_site = 0;
        for (std::int32_t north = 0; north < ro::sim::kTerrainLandmarkSiteCells; ++north) {
          for (std::int32_t east = 0; east < ro::sim::kTerrainLandmarkSiteCells; ++east) {
            if (chunk.OnPath(west + east, south + north)) ++path_in_site;
          }
        }
        CHECK(path_in_site == 0);
        // Le repère creux couvre son site.
        CHECK(std::ranges::any_of(chunk.pieces, [&](const PlacedPiece& placed) {
          return PieceOf(rule, placed.piece_id)->landmark &&
                 placed.x == chunk.chunk_x * kTerrainChunkCells + west &&
                 placed.y == chunk.chunk_y * kTerrainChunkCells + south;
        }));
      }
    }
  }
  CHECK(sites > kSeedCount);
}

TEST_CASE("Chemins survivors : largeur, écart et période bornés", "[sim][survivors]") {
  const ro::sim::SurvivorsRules& shipped = ro::sim::ShippedSurvivorsRules();
  REQUIRE(ro::sim::CheckSurvivorsRules(shipped).has_value());
  const auto refused = [&shipped](auto&& retouch) {
    ro::sim::SurvivorsRules rules = shipped;
    std::vector<TerrainRule> terrains(shipped.terrains.begin(), shipped.terrains.end());
    retouch(terrains.front());
    rules.terrains = terrains;
    return Refused(rules);
  };
  CHECK(refused([](TerrainRule& rule) { rule.path_period_chunks = -1; }));
  CHECK(refused(
      [](TerrainRule& rule) { rule.path_period_chunks = ro::sim::kTerrainWorldChunks + 1; }));
  CHECK(refused([](TerrainRule& rule) { rule.path_width_cells = 0; }));
  CHECK(refused([](TerrainRule& rule) { rule.path_wiggle_cells = -1; }));
  CHECK(refused([](TerrainRule& rule) {
    rule.path_width_cells = ro::sim::kMostPlazaPathWidthCells;
    rule.path_wiggle_cells =
        ro::sim::kTerrainPathMarginCells - ro::sim::kMostPlazaPathWidthCells + 1;
  }));
  // Une route trop large pour longer le site d'une place.
  CHECK(refused([](TerrainRule& rule) {
    rule.path_width_cells = ro::sim::kMostPlazaPathWidthCells + 1;
    rule.path_wiggle_cells = 1;
  }));
  // Aux bornes, et sans chemin.
  CHECK_FALSE(refused([](TerrainRule& rule) {
    rule.path_width_cells = ro::sim::kMostPlazaPathWidthCells;
    rule.path_wiggle_cells = ro::sim::kTerrainPathMarginCells - ro::sim::kMostPlazaPathWidthCells;
  }));
  CHECK_FALSE(refused([](TerrainRule& rule) {
    rule.path_period_chunks = 0;
    rule.path_width_cells = 0;
  }));
}

TEST_CASE("Chemins survivors : un repère tient dans le site de sa place", "[sim][survivors]") {
  const ro::sim::SurvivorsRules& shipped = ro::sim::ShippedSurvivorsRules();
  const auto with_landmark_side = [&shipped](std::int32_t side_cells, std::int32_t period) {
    const std::vector<std::string> rows(static_cast<std::size_t>(side_cells),
                                        std::string(static_cast<std::size_t>(side_cells), '#'));
    const std::vector<std::string_view> footprint(rows.begin(), rows.end());
    std::vector<TerrainPieceRule> pieces(shipped.terrains.front().pieces.begin(),
                                         shipped.terrains.front().pieces.end());
    for (TerrainPieceRule& piece : pieces) {
      if (piece.landmark) piece.footprint = footprint;
    }
    std::vector<TerrainRule> terrains(shipped.terrains.begin(), shipped.terrains.end());
    terrains.front().pieces = pieces;
    terrains.front().path_period_chunks = period;
    ro::sim::SurvivorsRules rules = shipped;
    rules.terrains = terrains;
    return Refused(rules);
  };
  const std::int32_t period = Plains().path_period_chunks;
  CHECK_FALSE(with_landmark_side(ro::sim::kTerrainLandmarkSiteCells, period));
  CHECK(with_landmark_side(ro::sim::kTerrainLandmarkSiteCells + 1, period));
  // Sans chemin, le grand emplacement entier.
  CHECK_FALSE(with_landmark_side(ro::sim::kTerrainLandmarkSiteCells + 1, 0));
}

// Un banc, lancé à la main : la prairie livrée avec ses chemins, puis la même
// sans chemin. Ses chiffres ne valent que pour la configuration bâtie.
TEST_CASE("Banc survivors : générer un morceau de la prairie, chemins compris",
          "[sim][survivors][.bench]") {
  constexpr std::int32_t kSide = 64;
  constexpr double kMicrosecondsPerSecond = 1e6;
  TerrainRule without_paths = Plains();
  without_paths.path_period_chunks = 0;
  for (const TerrainRule* rule : {&Plains(), static_cast<const TerrainRule*>(&without_paths)}) {
    std::size_t path_cells = 0;
    const auto begin = std::chrono::steady_clock::now();
    for (std::int32_t chunk_y = 0; chunk_y < kSide; ++chunk_y) {
      for (std::int32_t chunk_x = 0; chunk_x < kSide; ++chunk_x) {
        const TerrainChunk chunk =
            GenerateTerrainChunk(*rule, 1, kStartChunk + chunk_x, kStartChunk + chunk_y);
        path_cells += static_cast<std::size_t>(std::ranges::count(chunk.path, std::uint8_t{1}));
      }
    }
    const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - begin;
    std::printf("%s : %d morceaux, %zu cases de chemin, %.2f µs par morceau\n",
                rule->path_period_chunks > 0 ? "avec chemins" : "sans chemin", kSide * kSide,
                path_cells, elapsed.count() * kMicrosecondsPerSecond / (kSide * kSide));
  }
  CHECK(Plains().path_period_chunks > 0);
}
