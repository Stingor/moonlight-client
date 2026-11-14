// LES SALLES DU TERRAIN GÉNÉRÉ : un morceau est une salle ou un bloc de
// couloirs, chaque bord porte une porte ou s'ouvre en hall. La connexité tient
// par construction, et ces essais la vérifient sur une fenêtre de 12 × 12
// morceaux pour quarante graines : depuis le départ, l'inondation atteint toute
// case praticable. Les murs, les portes et les couloirs suivent les cubes du
// sol ; aucune pièce ne couvre un mur, aucune ne se pose dans un bloc.

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/error.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_terrain.h"

using ro::sim::FootprintCovers;
using ro::sim::FootprintSize;
using ro::sim::FootprintSizeOf;
using ro::sim::GenerateTerrainChunk;
using ro::sim::kTerrainChunkCells;
using ro::sim::kTerrainStartCell;
using ro::sim::kTerrainWallCells;
using ro::sim::PlacedPiece;
using ro::sim::TerrainChunk;
using ro::sim::TerrainPieceRule;
using ro::sim::TerrainRule;

namespace {

constexpr std::int32_t kStartChunk = kTerrainStartCell / kTerrainChunkCells;
constexpr std::int32_t kWindowChunks = 12;
constexpr std::int32_t kFirstChunk = kStartChunk - kWindowChunks / 2;
constexpr std::int32_t kWindowCells = kWindowChunks * kTerrainChunkCells;
constexpr std::uint64_t kSeedCount = 40;
constexpr std::string_view kLabKey = "lighthalzen_biolab";

[[nodiscard]] const TerrainRule& Lab() {
  const std::span<const TerrainRule> terrains = ro::sim::ShippedSurvivorsRules().terrains;
  const TerrainRule* lab = nullptr;
  for (const TerrainRule& rule : terrains) {
    if (rule.key == kLabKey) lab = &rule;
  }
  REQUIRE(lab != nullptr);
  return *lab;
}

[[nodiscard]] const TerrainPieceRule* PieceOf(const TerrainRule& rule, std::uint16_t piece_id) {
  for (const TerrainPieceRule& piece : rule.pieces) {
    if (piece.piece_id == piece_id) return &piece;
  }
  return nullptr;
}

[[nodiscard]] std::size_t WindowIndex(std::int32_t east, std::int32_t north) {
  return static_cast<std::size_t>(north) * kWindowCells + static_cast<std::size_t>(east);
}

// Les cases praticables de la fenêtre, et combien l'inondation à quatre voisins
// depuis le départ en laisse.
[[nodiscard]] std::size_t UnreachedFromStart(const TerrainRule& rule, std::uint64_t seed) {
  std::vector<std::uint8_t> passable(static_cast<std::size_t>(kWindowCells) * kWindowCells, 0U);
  for (std::int32_t row = 0; row < kWindowChunks; ++row) {
    for (std::int32_t column = 0; column < kWindowChunks; ++column) {
      const TerrainChunk chunk =
          GenerateTerrainChunk(rule, seed, kFirstChunk + column, kFirstChunk + row);
      for (std::int32_t north = 0; north < kTerrainChunkCells; ++north) {
        for (std::int32_t east = 0; east < kTerrainChunkCells; ++east) {
          passable[WindowIndex(column * kTerrainChunkCells + east,
                               row * kTerrainChunkCells + north)] =
              chunk.Passable(east, north) ? 1U : 0U;
        }
      }
    }
  }
  const std::int32_t start = kTerrainStartCell - kFirstChunk * kTerrainChunkCells;
  REQUIRE(passable[WindowIndex(start, start)] != 0U);
  std::vector<std::uint8_t> reached(passable.size(), 0U);
  std::vector<std::array<std::int32_t, 2>> pending{{start, start}};
  reached[WindowIndex(start, start)] = 1U;
  constexpr std::array<std::array<std::int32_t, 2>, 4> kSteps{{{1, 0}, {-1, 0}, {0, 1}, {0, -1}}};
  while (!pending.empty()) {
    const auto [east, north] = pending.back();
    pending.pop_back();
    for (const auto& step : kSteps) {
      const std::int32_t x = east + step[0];
      const std::int32_t y = north + step[1];
      if (x < 0 || y < 0 || x >= kWindowCells || y >= kWindowCells) continue;
      const std::size_t index = WindowIndex(x, y);
      if (passable[index] == 0U || reached[index] != 0U) continue;
      reached[index] = 1U;
      pending.push_back({x, y});
    }
  }
  std::size_t unreached = 0;
  for (std::size_t index = 0; index < passable.size(); ++index) {
    if (passable[index] != 0U && reached[index] == 0U) ++unreached;
  }
  return unreached;
}

[[nodiscard]] bool Refused(const ro::sim::SurvivorsRules& rules) {
  const ro::core::Result<void> checked = ro::sim::CheckSurvivorsRules(rules);
  return !checked && checked.error().code == ro::core::ErrorCode::kInvalidArgument;
}

}  // namespace

TEST_CASE("Salles survivors : depuis le départ, l'inondation atteint toute case praticable",
          "[sim][survivors]") {
  for (std::uint64_t seed = 1; seed <= kSeedCount; ++seed) {
    INFO("graine " << seed);
    CHECK(UnreachedFromStart(Lab(), seed) == 0);
  }
}

TEST_CASE("Salles survivors : murs, couloirs et blocs suivent la règle", "[sim][survivors]") {
  const TerrainRule& rule = Lab();
  std::size_t walls = 0;
  std::size_t corridor_blocks = 0;
  std::size_t landmarks = 0;
  for (std::uint64_t seed = 1; seed <= kSeedCount; ++seed) {
    INFO("graine " << seed);
    for (std::int32_t row = 0; row < kWindowChunks; ++row) {
      for (std::int32_t column = 0; column < kWindowChunks; ++column) {
        const TerrainChunk chunk =
            GenerateTerrainChunk(rule, seed, kFirstChunk + column, kFirstChunk + row);
        // Chaque cube du sol est d'un bloc : tout mur ou sans mur, tout chemin
        // ou sans chemin. Une case de mur est bloquée et n'est pas un chemin.
        for (std::int32_t y = 0; y < kTerrainChunkCells; y += kTerrainWallCells) {
          for (std::int32_t x = 0; x < kTerrainChunkCells; x += kTerrainWallCells) {
            const auto cell = static_cast<std::size_t>(y * kTerrainChunkCells + x);
            for (std::int32_t dy = 0; dy < kTerrainWallCells; ++dy) {
              for (std::int32_t dx = 0; dx < kTerrainWallCells; ++dx) {
                const auto other = static_cast<std::size_t>((y + dy) * kTerrainChunkCells + x + dx);
                REQUIRE(chunk.wall[other] == chunk.wall[cell]);
                REQUIRE(chunk.path[other] == chunk.path[cell]);
                if (chunk.wall[other] != 0U) {
                  REQUIRE_FALSE(chunk.Passable(x + dx, y + dy));
                  REQUIRE(chunk.path[other] == 0U);
                  ++walls;
                }
              }
            }
          }
        }
        // Un bloc de couloirs : un mur plein, percé de ses seuls chemins.
        bool block = true;
        for (std::size_t cell = 0; cell < chunk.wall.size(); ++cell) {
          block = block && (chunk.wall[cell] != 0U || chunk.path[cell] != 0U);
        }
        if (block) {
          ++corridor_blocks;
          CHECK(chunk.pieces.empty());
        }
        // Aucune pièce ne couvre un mur.
        for (const PlacedPiece& placed : chunk.pieces) {
          const TerrainPieceRule* piece = PieceOf(rule, placed.piece_id);
          REQUIRE(piece != nullptr);
          if (piece->landmark) ++landmarks;
          const FootprintSize size = FootprintSizeOf(*piece, placed.quarter);
          const std::int32_t west = placed.x - chunk.chunk_x * kTerrainChunkCells;
          const std::int32_t south = placed.y - chunk.chunk_y * kTerrainChunkCells;
          for (std::int32_t north = 0; north < size.height; ++north) {
            for (std::int32_t east = 0; east < size.width; ++east) {
              const auto cell =
                  static_cast<std::size_t>((south + north) * kTerrainChunkCells + west + east);
              REQUIRE(chunk.wall[cell] == 0U);
              if (!piece->landmark) REQUIRE(chunk.path[cell] == 0U);
              if (FootprintCovers(*piece, placed.quarter, east, north) && !piece->walkable) {
                REQUIRE_FALSE(chunk.Passable(west + east, south + north));
              }
            }
          }
        }
      }
    }
  }
  // L'essai a vu des murs, des blocs et des repères.
  CHECK(walls > 0);
  CHECK(corridor_blocks > kSeedCount);
  CHECK(landmarks > kSeedCount);
}

TEST_CASE("Salles survivors : aucun mur hors d'un terrain à salles", "[sim][survivors]") {
  for (const TerrainRule& rule : ro::sim::ShippedSurvivorsRules().terrains) {
    if (rule.rooms) continue;
    INFO(rule.key);
    for (std::int32_t offset = -2; offset <= 2; ++offset) {
      const TerrainChunk chunk =
          GenerateTerrainChunk(rule, 1, kStartChunk + offset, kStartChunk - offset);
      for (const std::uint8_t wall : chunk.wall) REQUIRE(wall == 0U);
    }
  }
}

TEST_CASE("Salles survivors : portes, couloirs et halls bornés", "[sim][survivors]") {
  const ro::sim::SurvivorsRules& shipped = ro::sim::ShippedSurvivorsRules();
  REQUIRE(ro::sim::CheckSurvivorsRules(shipped).has_value());
  const auto refused = [&shipped](std::string_view key, auto&& retouch) {
    ro::sim::SurvivorsRules rules = shipped;
    std::vector<TerrainRule> terrains(shipped.terrains.begin(), shipped.terrains.end());
    for (TerrainRule& terrain : terrains) {
      if (terrain.key == key) retouch(terrain);
    }
    rules.terrains = terrains;
    return Refused(rules);
  };
  const auto lab = [&refused](auto&& retouch) { return refused(kLabKey, retouch); };
  CHECK(lab([](TerrainRule& rule) { rule.door_cells = ro::sim::kLeastTerrainDoorCells - 2; }));
  CHECK(lab([](TerrainRule& rule) { rule.door_cells = ro::sim::kMostTerrainDoorCells + 2; }));
  CHECK(lab([](TerrainRule& rule) { rule.door_cells = ro::sim::kLeastTerrainDoorCells + 1; }));
  CHECK(lab([](TerrainRule& rule) { rule.corridor_per_mille = ro::sim::kTerrainPerMille + 1; }));
  CHECK(lab([](TerrainRule& rule) { rule.hall_per_mille = ro::sim::kTerrainPerMille + 1; }));
  CHECK(lab([](TerrainRule& rule) {
    rule.path_period_chunks = 3;
    rule.path_width_cells = 4;
  }));
  // Des portes sans salles.
  CHECK(refused("midgard_plains", [](TerrainRule& rule) { rule.door_cells = 8; }));
  CHECK(refused("midgard_plains", [](TerrainRule& rule) { rule.hall_per_mille = 1; }));
  // Aux bornes.
  CHECK_FALSE(lab([](TerrainRule& rule) { rule.door_cells = ro::sim::kLeastTerrainDoorCells; }));
  CHECK_FALSE(lab([](TerrainRule& rule) {
    rule.door_cells = ro::sim::kMostTerrainDoorCells;
    rule.corridor_per_mille = ro::sim::kTerrainPerMille;
    rule.hall_per_mille = ro::sim::kTerrainPerMille;
  }));
}

TEST_CASE("Salles survivors : toutes portes, tout bloc ou tout hall restent connexes",
          "[sim][survivors]") {
  const TerrainRule& shipped = Lab();
  constexpr std::uint64_t kExtremeSeeds = 8;
  for (const std::uint32_t corridor : {0U, ro::sim::kTerrainPerMille}) {
    for (const std::uint32_t hall : {0U, ro::sim::kTerrainPerMille}) {
      for (const std::int32_t door :
           {ro::sim::kLeastTerrainDoorCells, ro::sim::kMostTerrainDoorCells}) {
        TerrainRule rule = shipped;
        rule.corridor_per_mille = corridor;
        rule.hall_per_mille = hall;
        rule.door_cells = door;
        for (std::uint64_t seed = 1; seed <= kExtremeSeeds; ++seed) {
          INFO("couloirs " << corridor << ", halls " << hall << ", portes " << door << ", graine "
                           << seed);
          CHECK(UnreachedFromStart(rule, seed) == 0);
        }
      }
    }
  }
}
