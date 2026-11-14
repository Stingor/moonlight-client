#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/survivors_terrain.h"

#include "support/survivors_golden.h"

namespace ro::test_support {

// L'ARÈNE GÉNÉRÉE D'ESSAI : les règles livrées, dont seules les arènes et les
// terrains changent. Une arène unique, qui nomme une prairie de trois pièces ;
// ses plantes naissent au fil des tués et se perdent au loin, comme celles du
// catalogue livré, qui n'en sait rien.

inline constexpr std::uint16_t kTerrainArenaId = 1;
inline constexpr std::uint16_t kTerrainRuleId = 1;

// Un rocher 2 × 2, un arbre 3 × 3, un bosquet en L.
inline constexpr std::array<std::string_view, 2> kTerrainRock = {"##", "##"};
inline constexpr std::array<std::string_view, 3> kTerrainTree = {"###", "###", "###"};
inline constexpr std::array<std::string_view, 3> kTerrainGrove = {"#...", "#...", "####"};

inline constexpr std::array<sim::TerrainPieceRule, 3> kTerrainPieces{{
    {.piece_id = 1, .key = "rock", .footprint = kTerrainRock, .weight = 3},
    {.piece_id = 2, .key = "tree", .footprint = kTerrainTree, .weight = 4},
    {.piece_id = 3, .key = "grove", .footprint = kTerrainGrove, .weight = 2},
}};

inline constexpr std::array<sim::TerrainRule, 1> kTerrainRules{{
    {.terrain_id = kTerrainRuleId,
     .key = "test_plains",
     .pieces = kTerrainPieces,
     .density_min_per_mille = 100,
     .density_max_per_mille = 500,
     .density_period_chunks = 2,
     .clearing_cells = 12,
     .landmark_period_chunks = 0},
}};

inline constexpr std::array<sim::ArenaRule, 1> kTerrainArenas{{
    {.arena_id = kTerrainArenaId,
     .key = "generated_plains",
     .map_name = "",
     .start = std::nullopt,
     .terrain_id = kTerrainRuleId,
     .ranked = true},
}};

[[nodiscard]] inline const sim::SurvivorsRules& TerrainRules() {
  static const sim::SurvivorsRules kRules = [] {
    sim::SurvivorsRules rules = sim::ShippedSurvivorsRules();
    rules.arenas = kTerrainArenas;
    rules.terrains = kTerrainRules;
    return rules;
  }();
  return kRules;
}

// Le départ de la partie en or, sur l'arène générée.
[[nodiscard]] inline sim::SurvivorsStart TerrainStart(std::uint64_t seed) {
  sim::SurvivorsStart start = GoldenStart(seed);
  start.arena_id = kTerrainArenaId;
  return start;
}

}  // namespace ro::test_support
