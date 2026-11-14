#include "ro/sim/survivors_openness.h"

#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/packed_position.h"
#include "ro/sim/walk_map.h"

using ro::sim::ArenaOpenness;
using ro::sim::CellPosition;
using ro::sim::MeasureArenaOpenness;
using ro::sim::OpennessWindow;
using ro::sim::WalkMap;
using ro::sim::WindowAround;

namespace {

constexpr std::int32_t kSmallWindow = 4;

// Deux régions séparées par la colonne 11 : à l'ouest, deux rangées de onze
// cases (22) ; à l'est, un bloc plein de 4 × 4 (16), plus ouvert mais plus
// petit. La première ligne du dessin est le nord.
constexpr std::array<std::string_view, 4> kTwoRegions = {
    "############....",
    "############....",
    "...........#....",
    "...........#....",
};

}  // namespace

TEST_CASE("Ouverture survivors : une carte toute praticable", "[sim][survivors]") {
  constexpr std::array<std::string_view, 8> kOpen = {
      "........", "........", "........", "........",
      "........", "........", "........", "........",
  };
  const ArenaOpenness openness = MeasureArenaOpenness(WalkMap::FromPicture(kOpen), kSmallWindow);
  CHECK(openness.window_cells == kSmallWindow);
  CHECK(openness.map_cells == 64);
  CHECK(openness.passable_cells == 64);
  CHECK(openness.main_region_cells == 64);
  CHECK(openness.tiled_passable == std::vector<std::int32_t>{16, 16, 16, 16});
  // Toutes les fenêtres intérieures sont pleines : la plus proche du centre.
  CHECK(openness.best.centre == CellPosition{.x = 4, .y = 4});
  CHECK(openness.best.passable_cells == 16);
  CHECK(openness.best.reachable_cells == 16);
}

TEST_CASE("Ouverture survivors : la meilleure fenêtre reste dans la région principale",
          "[sim][survivors]") {
  const ArenaOpenness openness =
      MeasureArenaOpenness(WalkMap::FromPicture(kTwoRegions), kSmallWindow);
  CHECK(openness.passable_cells == 38);
  CHECK(openness.main_region_cells == 22);
  CHECK(openness.tiled_passable == std::vector<std::int32_t>{6, 8, 8, 16});
  // Le bloc de l'est donnerait 16, mais il n'est pas la région principale. À
  // l'ouest, huit cases au plus ; à distance 1 du centre (8, 2), la plus au
  // sud puis la plus à l'ouest.
  CHECK(openness.best.centre == CellPosition{.x = 7, .y = 1});
  CHECK(openness.best.passable_cells == 8);
  CHECK(openness.best.reachable_cells == 8);
}

TEST_CASE("Ouverture survivors : une fenêtre compte à part ce que son centre atteint",
          "[sim][survivors]") {
  const WalkMap map = WalkMap::FromPicture(kTwoRegions);
  // Centre sur le mur : rien d'atteignable, mais des cases praticables.
  const OpennessWindow on_wall = WindowAround(map, CellPosition{.x = 11, .y = 1}, kSmallWindow);
  CHECK(on_wall.passable_cells == 7);
  CHECK(on_wall.reachable_cells == 0);
  // À l'est du mur : la fenêtre voit huit cases, son centre n'en atteint que six.
  const OpennessWindow east = WindowAround(map, CellPosition{.x = 12, .y = 1}, kSmallWindow);
  CHECK(east.passable_cells == 8);
  CHECK(east.reachable_cells == 6);
  // À l'ouest du mur, tout ce qui est vu est atteint.
  const OpennessWindow west = WindowAround(map, CellPosition{.x = 10, .y = 1}, kSmallWindow);
  CHECK(west.passable_cells == 6);
  CHECK(west.reachable_cells == 6);
}

TEST_CASE("Ouverture survivors : une carte vide ou bloquée", "[sim][survivors]") {
  const ArenaOpenness empty = MeasureArenaOpenness(WalkMap(), kSmallWindow);
  CHECK(empty.map_cells == 0);
  CHECK(empty.tiled_passable.empty());
  constexpr std::array<std::string_view, 2> kWalls = {"####", "####"};
  const ArenaOpenness walls = MeasureArenaOpenness(WalkMap::FromPicture(kWalls), 0);
  CHECK(walls.window_cells == 1);
  CHECK(walls.passable_cells == 0);
  CHECK(walls.main_region_cells == 0);
  CHECK(walls.best.passable_cells == 0);
  CHECK(walls.best.reachable_cells == 0);
}
