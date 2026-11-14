#include "ro/sim/walk_field.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/walk_map.h"

using ro::sim::CellPosition;
using ro::sim::CornersOf;
using ro::sim::NearestPassable;
using ro::sim::OctileCost;
using ro::sim::WalkField;
using ro::sim::WalkMap;

namespace {

// La première ligne est le NORD : y = 4 en haut, y = 0 en bas.
WalkMap Corridor() {
  // clang-format off
  constexpr std::array<std::string_view, 5> kRows{
      "..........",
      ".########.",
      ".#......#.",
      ".#.####.#.",
      "..#......#",
  };
  // clang-format on
  return WalkMap::FromPicture(kRows);
}

}  // namespace

TEST_CASE("Champ de marche : le coût se lit depuis n'importe quelle case", "[sim][walk_field]") {
  constexpr std::array<std::string_view, 3> kRows{"...", "...", "..."};
  const WalkMap map = WalkMap::FromPicture(kRows);
  const std::array<CellPosition, 1> source{CellPosition{0, 0, 0}};
  const WalkField field = WalkField::Flood(map, source);
  CHECK(field.CostFrom(0, 0) == 0u);
  CHECK(field.CostFrom(2, 0) == 20u);
  // Deux diagonales valent quatorze chacune.
  CHECK(field.CostFrom(2, 2) == 28u);
  CHECK_FALSE(field.CostFrom(3, 0).has_value());
  const std::vector<CellPosition> path = field.PathFrom(2, 2);
  REQUIRE(path.size() == 3);
  CHECK(path.front().x == 2);
  CHECK(path.back().x == 0);
  CHECK(path.back().y == 0);
}

TEST_CASE("Champ de marche : pas de coin coupé, et un chemin sans borne de longueur",
          "[sim][walk_field]") {
  constexpr std::array<std::string_view, 2> kRows{".#", "#."};
  const WalkMap blocked = WalkMap::FromPicture(kRows);
  const std::array<CellPosition, 1> corner{CellPosition{0, 1, 0}};
  // La diagonale couperait deux coins : le serveur la refuse, le champ aussi.
  CHECK_FALSE(WalkField::Flood(blocked, corner).CostFrom(1, 0).has_value());

  // Bien plus long que les trente-deux pas du serveur.
  const std::string long_row(80, '.');
  const std::array<std::string_view, 1> kLongRows{long_row};
  const WalkMap line = WalkMap::FromPicture(kLongRows);
  const std::array<CellPosition, 1> west{CellPosition{0, 0, 0}};
  CHECK(WalkField::Flood(line, west).PathFrom(79, 0).size() == 80);

  // Un couloir en lacet : le chemin contourne les murs.
  const WalkMap corridor = Corridor();
  const std::array<CellPosition, 1> exit{CellPosition{2, 1, 0}};
  const WalkField field = WalkField::Flood(corridor, exit);
  const auto from_far = field.CostFrom(8, 0);
  REQUIRE(from_far.has_value());
  const std::vector<CellPosition> path = field.PathFrom(8, 0);
  REQUIRE(path.size() > 1);
  CHECK(path.back().x == 2);
  CHECK(path.back().y == 1);
  for (const CellPosition& cell : path) CHECK(corridor.Passable(cell.x, cell.y));
}

TEST_CASE("Champ de marche : coins d'un chemin et case praticable voisine", "[sim][walk_field]") {
  const std::vector<CellPosition> straight{{0, 0, 0}, {1, 0, 0}, {2, 0, 0}, {3, 1, 0}, {4, 2, 0}};
  const std::vector<CellPosition> corners = CornersOf(straight);
  REQUIRE(corners.size() == 3);
  CHECK(corners[1].x == 2);

  constexpr std::array<std::string_view, 3> kRows{"...", ".#.", "..."};
  const WalkMap map = WalkMap::FromPicture(kRows);
  const auto nearest = NearestPassable(map, 1, 1, 2);
  REQUIRE(nearest.has_value());
  CHECK(map.Passable(nearest->x, nearest->y));
  // Deux appels rendent la même case.
  const auto again = NearestPassable(map, 1, 1, 2);
  CHECK(again->x == nearest->x);
  CHECK(again->y == nearest->y);
  CHECK(OctileCost(0, 0, 3, 1) == 34u);
}

TEST_CASE("Champ de marche : le pas suivant rend exactement le chemin", "[sim][walk_field]") {
  const WalkMap corridor = Corridor();
  const std::array<CellPosition, 1> exit{CellPosition{2, 1, 0}};
  const WalkField field = WalkField::Flood(corridor, exit);
  for (std::int32_t y = 0; y < corridor.height(); ++y) {
    for (std::int32_t x = 0; x < corridor.width(); ++x) {
      const std::vector<CellPosition> path = field.PathFrom(x, y);
      if (path.empty()) {
        CHECK_FALSE(field.NextCellFrom(x, y).has_value());
        continue;
      }
      // Suivre le pas suivant depuis la case de départ redonne chaque case du
      // chemin, puis rien sur la source.
      std::int32_t at_x = x;
      std::int32_t at_y = y;
      for (std::size_t index = 1; index < path.size(); ++index) {
        const auto next = field.NextCellFrom(at_x, at_y);
        REQUIRE(next.has_value());
        CHECK(next->x == path[index].x);
        CHECK(next->y == path[index].y);
        // La direction serveur porte le pas : un pas droit change un seul axe,
        // une diagonale ne coupe jamais de coin.
        const std::int32_t dx = next->x - at_x;
        const std::int32_t dy = next->y - at_y;
        CHECK(next->direction < 8);
        if (dx != 0 && dy != 0) {
          CHECK(corridor.Passable(at_x + dx, at_y));
          CHECK(corridor.Passable(at_x, at_y + dy));
        }
        at_x = next->x;
        at_y = next->y;
      }
      CHECK_FALSE(field.NextCellFrom(at_x, at_y).has_value());
    }
  }
}

TEST_CASE("Champ de marche : pas suivant d'une source, d'un mur, hors carte", "[sim][walk_field]") {
  constexpr std::array<std::string_view, 3> kRows{"...", ".#.", "..."};
  const WalkMap map = WalkMap::FromPicture(kRows);
  const std::array<CellPosition, 1> source{CellPosition{0, 0, 0}};
  const WalkField field = WalkField::Flood(map, source);
  CHECK_FALSE(field.NextCellFrom(0, 0).has_value());
  CHECK_FALSE(field.NextCellFrom(1, 1).has_value());
  CHECK_FALSE(field.NextCellFrom(-1, 0).has_value());
  CHECK_FALSE(field.NextCellFrom(3, 0).has_value());
  CHECK_FALSE(WalkField().NextCellFrom(0, 0).has_value());

  // La diagonale de (0, 1) vers la source (1, 0) couperait le coin du mur en
  // (1, 1) : le pas suivant descend d'abord au sud.
  constexpr std::array<std::string_view, 2> kCornerRows{".#", ".."};
  const WalkMap corner = WalkMap::FromPicture(kCornerRows);
  const std::array<CellPosition, 1> east{CellPosition{1, 0, 0}};
  const WalkField around = WalkField::Flood(corner, east);
  const auto next = around.NextCellFrom(0, 1);
  REQUIRE(next.has_value());
  CHECK(next->x == 0);
  CHECK(next->y == 0);
  CHECK(next->direction == 4);
}

TEST_CASE("Champ de marche : la direction serveur du pas suivant", "[sim][walk_field]") {
  constexpr std::array<std::string_view, 3> kRows{"...", "...", "..."};
  const WalkMap map = WalkMap::FromPicture(kRows);
  const std::array<CellPosition, 1> middle{CellPosition{1, 1, 0}};
  const WalkField field = WalkField::Flood(map, middle);
  // Depuis le sud, le pas vers le centre va au nord (0) ; depuis l'est, à
  // l'ouest (2) ; depuis le nord-est, au sud-ouest (3).
  CHECK(field.NextCellFrom(1, 0)->direction == 0);
  CHECK(field.NextCellFrom(2, 1)->direction == 2);
  CHECK(field.NextCellFrom(2, 2)->direction == 3);
  CHECK(field.NextCellFrom(0, 0)->direction == 7);
}
