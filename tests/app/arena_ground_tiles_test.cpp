// LES CARREAUX DE CHEMIN DU SOL DESSINÉ ET LE SEMIS : la terre d'un chemin
// se lit à la place du cube dans le monde, un bord lit la colonne du côté de
// sa terre, et les champignons gardent leur part du semis.

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_terrain.h"

#include "survivors/arena_ground_look.h"
#include "survivors/arena_ground_paths.h"
#include "survivors/arena_ground_plan.h"

using moonclient::BiomeLook;
using moonclient::GroundPlan;
using moonclient::GroundPlanLight;
using moonclient::GroundTextureRole;
using moonclient::GroundWindowAround;
using moonclient::GroundWindowOrigin;
using moonclient::kGroundCellsPerCube;
using moonclient::kGroundWindowCells;
using moonclient::kGroundWindowCubes;
using moonclient::kSeedPose;
using moonclient::PlanGroundWindow;
using moonclient::PlannedCube;
using moonclient::PlannedPose;

namespace {

constexpr std::uint64_t kSeed = 20261084;
// Un soleil bas, au sud-est ; il ne change aucun carreau.
const GroundPlanLight kLowSun{.toward_sun = glm::normalize(glm::vec3(0.6F, 0.5F, 0.6F))};

// La règle de terrain livrée et son aspect.
struct Plains {
  const ro::sim::TerrainRule* rule = nullptr;
  const BiomeLook* look = nullptr;
};

Plains ShippedPlains() {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  Plains plains;
  for (const ro::sim::ArenaRule& arena : rules.arenas) {
    if (arena.terrain_id == 0 || plains.rule != nullptr) continue;
    plains.rule = ro::sim::FindTerrain(rules, arena.terrain_id);
  }
  REQUIRE(plains.rule != nullptr);
  plains.look = moonclient::FindBiomeLook(plains.rule->key);
  REQUIRE(plains.look != nullptr);
  return plains;
}

GroundWindowOrigin StartWindow() {
  return GroundWindowAround(ro::sim::kTerrainStartCell, ro::sim::kTerrainStartCell);
}

GroundPlan Plan(const Plains& plains, std::uint64_t seed, GroundWindowOrigin origin) {
  return PlanGroundWindow(*plains.rule, seed, *plains.look, origin, kLowSun);
}

bool OnPathAt(const GroundPlan& plan, std::int32_t x, std::int32_t y) {
  return plan.path[static_cast<std::size_t>(y) * kGroundWindowCells +
                   static_cast<std::size_t>(x)] != 0;
}

}  // namespace

namespace {

// Le point de texture, en cubes et modulo la texture, que lit le coin
// (`corner_east`, `corner_north`) de 0 ou 1 du cube, comme le sol dessiné le
// pose : un quart de tour envoie l'est du motif vers les v décroissants et le
// nord vers les u croissants.
std::pair<std::int32_t, std::int32_t> TexturePointOf(const PlannedCube& cube,
                                                     std::int32_t corner_east,
                                                     std::int32_t corner_north) {
  constexpr std::int32_t kSide = moonclient::kGroundTextureCubes;
  const std::int32_t x = cube.quarter_east + corner_east;
  const std::int32_t y = cube.quarter_north + corner_north;
  std::int32_t u = x;
  std::int32_t v = y;
  switch (cube.turn % ro::sim::kTerrainQuarterTurns) {
    case 1:
      u = y;
      v = kSide - x;
      break;
    case 2:
      u = kSide - x;
      v = kSide - y;
      break;
    case 3:
      u = kSide - y;
      v = x;
      break;
    default:
      break;
  }
  return {((u % kSide) + kSide) % kSide, ((v % kSide) + kSide) % kSide};
}

}  // namespace

TEST_CASE("Sol dessiné : la terre d'un chemin se lit à la place du cube dans le monde",
          "[app][survivors]") {
  const Plains plains = ShippedPlains();
  const GroundWindowOrigin start = StartWindow();
  const GroundPlan plan = Plan(plains, kSeed, start);
  const auto role_of = [&plains](const PlannedCube& cube) {
    return plains.look->textures[cube.texture].role;
  };
  const auto cube_at = [&plan](std::int32_t i, std::int32_t j) -> const PlannedCube& {
    return plan
        .cubes[static_cast<std::size_t>(j) * kGroundWindowCubes + static_cast<std::size_t>(i)];
  };
  // Un cube de terre pleine lit le morceau de sa place dans le monde, debout.
  std::size_t full_cubes = 0;
  for (std::int32_t j = 0; j < kGroundWindowCubes; ++j) {
    for (std::int32_t i = 0; i < kGroundWindowCubes; ++i) {
      const PlannedCube& cube = cube_at(i, j);
      if (role_of(cube) != GroundTextureRole::kPathFull) continue;
      ++full_cubes;
      const std::int32_t world_x = start.x_cells / kGroundCellsPerCube + i;
      const std::int32_t world_y = start.y_cells / kGroundCellsPerCube + j;
      CHECK(cube.quarter_east == world_x % moonclient::kGroundTextureCubes);
      CHECK(cube.quarter_north == world_y % moonclient::kGroundTextureCubes);
      CHECK(cube.turn == 0);
    }
  }
  CHECK(full_cubes > 0);
  // DEUX CUBES VOISINS de terre pleine, ou de bord, d'une même texture, que
  // leur côté commun ne sépare pas d'herbe (deux chemins dos à dos), lisent
  // des morceaux contigus : les deux coins de ce côté tombent sur les mêmes
  // points de la texture. Le long d'un bord aussi : un bord glisse le long de
  // sa colonne avec la case du monde.
  const auto contiguous_role = [&role_of](const PlannedCube& cube) {
    const GroundTextureRole role = role_of(cube);
    return role == GroundTextureRole::kPathFull || role == GroundTextureRole::kPathEdge;
  };
  // Le coin (a, b) de cube est de terre quand au moins deux des quatre cases
  // qui le touchent sont de chemin ; un coin dont une case sort de la fenêtre
  // compte pour de l'herbe.
  constexpr std::int32_t kDirtCornerLeastCells = 2;
  const auto dirt_corner = [&plan](std::int32_t a, std::int32_t b) {
    const std::int32_t x = a * kGroundCellsPerCube;
    const std::int32_t y = b * kGroundCellsPerCube;
    if (x < 1 || y < 1 || x >= kGroundWindowCells || y >= kGroundWindowCells) return false;
    const std::int32_t paths = static_cast<std::int32_t>(OnPathAt(plan, x - 1, y - 1)) +
                               static_cast<std::int32_t>(OnPathAt(plan, x, y - 1)) +
                               static_cast<std::int32_t>(OnPathAt(plan, x - 1, y)) +
                               static_cast<std::int32_t>(OnPathAt(plan, x, y));
    return paths >= kDirtCornerLeastCells;
  };
  const auto is_edge = [&role_of](const PlannedCube& cube) {
    return role_of(cube) == GroundTextureRole::kPathEdge;
  };
  std::size_t compared = 0;
  std::size_t along_edges = 0;
  for (std::int32_t j = 0; j < kGroundWindowCubes; ++j) {
    for (std::int32_t i = 0; i < kGroundWindowCubes; ++i) {
      const PlannedCube& here = cube_at(i, j);
      if (!contiguous_role(here)) continue;
      if (i + 1 < kGroundWindowCubes && (dirt_corner(i + 1, j) || dirt_corner(i + 1, j + 1))) {
        const PlannedCube& east = cube_at(i + 1, j);
        if (east.texture == here.texture && contiguous_role(east)) {
          INFO("cube (" << i << ", " << j << ") et son voisin de l'est");
          CHECK(TexturePointOf(here, 1, 0) == TexturePointOf(east, 0, 0));
          CHECK(TexturePointOf(here, 1, 1) == TexturePointOf(east, 0, 1));
          ++compared;
          if (is_edge(here)) ++along_edges;
        }
      }
      if (j + 1 < kGroundWindowCubes && (dirt_corner(i, j + 1) || dirt_corner(i + 1, j + 1))) {
        const PlannedCube& north = cube_at(i, j + 1);
        if (north.texture == here.texture && contiguous_role(north)) {
          INFO("cube (" << i << ", " << j << ") et son voisin du nord");
          CHECK(TexturePointOf(here, 0, 1) == TexturePointOf(north, 0, 0));
          CHECK(TexturePointOf(here, 1, 1) == TexturePointOf(north, 1, 0));
          ++compared;
          if (is_edge(here)) ++along_edges;
        }
      }
    }
  }
  CHECK(compared > 0);
  CHECK(along_edges > 0);
  // UN BORD NE FAIT JAMAIS DE DEMI-TOUR : la terre à l'est ou au nord lit
  // l'autre moitié de la bande, tournée comme la terre à l'ouest ou au sud,
  // et le grain peint de la texture garde le sens de celui de la terre pleine.
  constexpr std::uint8_t kHalfTurn = ro::sim::kTerrainQuarterTurns / 2;
  std::size_t edges = 0;
  for (const PlannedCube& cube : plan.cubes) {
    if (!is_edge(cube)) continue;
    ++edges;
    CHECK(cube.turn < kHalfTurn);
  }
  CHECK(edges > 0);
  // LA TERRE D'UN BORD EST DU BON CÔTÉ : la colonne lue est `quarter_u` quand
  // les coins de terre du cube sont à l'ouest ou au sud, `east_quarter_u`
  // quand ils sont à l'est ou au nord. Les cubes du pourtour, dont un coin
  // touche une case hors de la fenêtre, ne se jugent pas.
  const moonclient::GroundTextureLook* edge_look = nullptr;
  for (const moonclient::GroundTextureLook& look : plains.look->textures) {
    if (look.role == GroundTextureRole::kPathEdge) {
      edge_look = &look;
      break;
    }
  }
  REQUIRE(edge_look != nullptr);
  REQUIRE(edge_look->east_quarter_u != moonclient::GroundTextureLook::kNoEastColumn);
  REQUIRE(edge_look->east_quarter_u != edge_look->quarter_u);
  std::size_t west_or_south = 0;
  std::size_t east_or_north = 0;
  for (std::int32_t j = 1; j + 1 < kGroundWindowCubes; ++j) {
    for (std::int32_t i = 1; i + 1 < kGroundWindowCubes; ++i) {
      const PlannedCube& cube = cube_at(i, j);
      if (!is_edge(cube)) continue;
      std::uint8_t corners = 0;
      if (dirt_corner(i, j)) corners |= moonclient::kDirtSouthWest;
      if (dirt_corner(i + 1, j)) corners |= moonclient::kDirtSouthEast;
      if (dirt_corner(i, j + 1)) corners |= moonclient::kDirtNorthWest;
      if (dirt_corner(i + 1, j + 1)) corners |= moonclient::kDirtNorthEast;
      const std::uint8_t column =
          moonclient::TextureCubeOf(cube.quarter_east, cube.quarter_north, cube.turn).u;
      INFO("bord (" << i << ", " << j << "), coins " << static_cast<int>(corners));
      if (corners == (moonclient::kDirtSouthWest | moonclient::kDirtNorthWest) ||
          corners == (moonclient::kDirtSouthWest | moonclient::kDirtSouthEast)) {
        CHECK(column == edge_look->quarter_u);
        ++west_or_south;
      } else if (corners == (moonclient::kDirtSouthEast | moonclient::kDirtNorthEast) ||
                 corners == (moonclient::kDirtNorthWest | moonclient::kDirtNorthEast)) {
        CHECK(column == edge_look->east_quarter_u);
        ++east_or_north;
      } else {
        FAIL_CHECK("un bord dont les coins de terre ne font pas un côté");
      }
    }
  }
  CHECK(west_or_south > 0);
  CHECK(east_or_north > 0);
}

TEST_CASE("Sol dessiné : un semis sur quatre au plus est un champignon", "[app][survivors]") {
  const Plains plains = ShippedPlains();
  const auto is_mushroom = [](std::string_view model) {
    return model.find("버섯") != std::string_view::npos;
  };
  // Les parts du catalogue : un sur quatre dans l'herbe, un sur huit au bord
  // des chemins, et à l'échelle réduite.
  constexpr std::uint32_t kGrassShare = 4;
  constexpr std::uint32_t kVergeShare = 8;
  constexpr float kLargestMushroomScale = 0.7F;
  const auto mushroom_weight = [&](std::span<const moonclient::SeedLook> seeds) {
    std::uint32_t mushrooms = 0;
    std::uint32_t total = 0;
    for (const moonclient::SeedLook& seed : seeds) {
      total += seed.weight;
      if (!is_mushroom(seed.model)) continue;
      mushrooms += seed.weight;
      CHECK(seed.scale <= kLargestMushroomScale);
    }
    return std::pair{mushrooms, total};
  };
  const auto [grass_mushrooms, grass_total] = mushroom_weight(plains.look->seeds);
  CHECK(grass_mushrooms * kGrassShare <= grass_total);
  CHECK(grass_mushrooms > 0);
  const auto [verge_mushrooms, verge_total] = mushroom_weight(plains.look->verge_seeds);
  CHECK(verge_mushrooms * kVergeShare <= verge_total);
  // Dans une fenêtre, le tirage suit les parts : au plus un semis sur trois,
  // bordure comprise, est un champignon.
  constexpr std::size_t kMostShareInWindow = 3;
  const GroundPlan plan = Plan(plains, kSeed, StartWindow());
  std::size_t seeds = 0;
  std::size_t mushrooms = 0;
  for (const PlannedPose& pose : plan.poses) {
    if (pose.piece != kSeedPose) continue;
    ++seeds;
    if (is_mushroom(pose.model)) ++mushrooms;
  }
  REQUIRE(seeds > 0);
  CHECK(mushrooms > 0);
  CHECK(mushrooms * kMostShareInWindow <= seeds);
}
