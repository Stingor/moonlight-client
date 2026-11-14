#include "ro/sim/survivors_flow.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/packed_position.h"
#include "ro/sim/survivors_ground.h"
#include "ro/sim/walk_map.h"

using ro::sim::CellPosition;
using ro::sim::HordeFlow;
using ro::sim::SurvivorsGround;
using ro::sim::WalkMap;

namespace {

// Un U ouvert au sud : le héros dans le creux, en (6, 3) ; un monstre au nord
// du fond du U doit en faire le tour. La première ligne est le nord.
SurvivorsGround UWall() {
  // clang-format off
  constexpr std::array<std::string_view, 9> kRows{
      ".............",
      ".............",
      "...#######...",
      "...#.....#...",
      "...#.....#...",
      "...#.....#...",
      "...#.....#...",
      ".............",
      ".............",
  };
  // clang-format on
  return SurvivorsGround::Bounded(WalkMap::FromPicture(kRows));
}

constexpr CellPosition kHeroInside{6, 3, 0};
constexpr std::int32_t kWideRadius = 20;

}  // namespace

TEST_CASE("Poursuite survivors : un monstre contourne le U", "[sim][survivors]") {
  const SurvivorsGround arena = UWall();
  HordeFlow flow;
  flow.Refresh(arena, kHeroInside, 0, kWideRadius, 5);
  CHECK(flow.floods() == 1U);

  // Du nord du fond du U, le monstre suit le pas suivant jusqu'au héros.
  std::int32_t x = 6;
  std::int32_t y = 8;
  int steps = 0;
  bool went_around = false;
  while (!(x == kHeroInside.x && y == kHeroInside.y)) {
    const std::optional<CellPosition> next = flow.NextCellFrom(x, y);
    REQUIRE(next.has_value());
    REQUIRE(arena.Passable(next->x, next->y));
    const std::int32_t dx = next->x - x;
    const std::int32_t dy = next->y - y;
    REQUIRE(dx >= -1);
    REQUIRE(dx <= 1);
    REQUIRE(dy >= -1);
    REQUIRE(dy <= 1);
    // Une diagonale ne coupe jamais de coin.
    if (dx != 0 && dy != 0) {
      CHECK(arena.Passable(x + dx, y));
      CHECK(arena.Passable(x, y + dy));
    }
    if (next->y <= 1) went_around = true;
    x = next->x;
    y = next->y;
    REQUIRE(++steps < 64);
  }
  // Le seul passage est l'ouverture du sud.
  CHECK(went_around);
  CHECK_FALSE(flow.NextCellFrom(kHeroInside.x, kHeroInside.y).has_value());
}

TEST_CASE("Poursuite survivors : la fenêtre borne le champ", "[sim][survivors]") {
  const SurvivorsGround arena = UWall();
  HordeFlow flow;
  flow.Refresh(arena, CellPosition{1, 1, 0}, 0, 2, 5);
  // Dans la fenêtre [0, 3] : un pas ; au-delà : rien.
  CHECK(flow.NextCellFrom(2, 3).has_value());
  CHECK_FALSE(flow.NextCellFrom(4, 1).has_value());
  CHECK_FALSE(flow.NextCellFrom(1, 4).has_value());
  CHECK_FALSE(flow.NextCellFrom(-1, 1).has_value());
  // Un mur dans la fenêtre : rien.
  CHECK_FALSE(flow.NextCellFrom(3, 2).has_value());
  // Les cases rendues sont des cases de l'arène, pas de la fenêtre : de (1, 3),
  // le seul plus court chemin descend au sud.
  const std::optional<CellPosition> next = flow.NextCellFrom(1, 3);
  REQUIRE(next.has_value());
  CHECK(next->x == 1);
  CHECK(next->y == 2);
  CHECK(next->direction == 4);

  // Une fenêtre décalée de l'origine : le héros en (11, 7), rayon 1.
  flow.Refresh(arena, CellPosition{11, 7, 0}, 100, 1, 5);
  const std::optional<CellPosition> shifted = flow.NextCellFrom(12, 7);
  REQUIRE(shifted.has_value());
  CHECK(shifted->x == 11);
  CHECK(shifted->y == 7);
  CHECK(shifted->direction == 2);
}

TEST_CASE("Poursuite survivors : une inondation par case et par délai", "[sim][survivors]") {
  const SurvivorsGround arena = UWall();
  HordeFlow flow;
  CHECK(flow.floods() == 0U);
  CHECK_FALSE(flow.NextCellFrom(0, 0).has_value());

  flow.Refresh(arena, CellPosition{1, 1, 0}, 10, kWideRadius, 5);
  CHECK(flow.floods() == 1U);
  // Même case : rien à refaire.
  flow.Refresh(arena, CellPosition{1, 1, 4}, 12, kWideRadius, 5);
  CHECK(flow.floods() == 1U);
  // Case changée, mais avant refresh_steps : le champ attend.
  flow.Refresh(arena, CellPosition{2, 1, 0}, 14, kWideRadius, 5);
  CHECK(flow.floods() == 1U);
  REQUIRE(flow.NextCellFrom(2, 1).has_value());
  CHECK(flow.NextCellFrom(2, 1)->x == 1);
  // Le délai passé : la nouvelle case devient la source.
  flow.Refresh(arena, CellPosition{2, 1, 0}, 15, kWideRadius, 5);
  CHECK(flow.floods() == 2U);
  CHECK_FALSE(flow.NextCellFrom(2, 1).has_value());
  REQUIRE(flow.NextCellFrom(1, 1).has_value());
  CHECK(flow.NextCellFrom(1, 1)->x == 2);
  // Même case, quel que soit le délai : rien à refaire.
  flow.Refresh(arena, CellPosition{2, 1, 0}, 1000, kWideRadius, 5);
  CHECK(flow.floods() == 2U);
}
