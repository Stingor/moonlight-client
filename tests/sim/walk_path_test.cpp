#include "ro/sim/walk_path.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "ro/sim/movement.h"
#include "ro/sim/walk_map.h"
#include "ro/sim/world.h"

using Catch::Matchers::WithinAbs;
using ro::sim::Actor;
using ro::sim::CellMovement;
using ro::sim::CellPosition;
using ro::sim::FacingAt;
using ro::sim::FindEasyWalkPath;
using ro::sim::FindWalkPath;
using ro::sim::InterpolatedCell;
using ro::sim::IsDiagonal;
using ro::sim::IsWalking;
using ro::sim::kMaxWalkSteps;
using ro::sim::MovementDurationMs;
using ro::sim::StepDurationMs;
using ro::sim::StraightWalkPath;
using ro::sim::WalkMap;
using ro::sim::WalkPath;
using ro::sim::WalkPlanner;

namespace {

using Steps = std::vector<std::uint8_t>;

// Les directions du serveur, nommées pour que les attentes se lisent.
constexpr std::uint8_t kNorth = 0;
constexpr std::uint8_t kNorthWest = 1;
constexpr std::uint8_t kWest = 2;
constexpr std::uint8_t kSouthWest = 3;
constexpr std::uint8_t kSouth = 4;
constexpr std::uint8_t kSouthEast = 5;
constexpr std::uint8_t kEast = 6;
constexpr std::uint8_t kNorthEast = 7;

CellPosition At(std::uint16_t x, std::uint16_t y) { return CellPosition{x, y, 0}; }

Steps DirectionsOf(const std::optional<WalkPath>& path) {
  REQUIRE(path.has_value());
  return path->directions;
}

WalkMap Open(std::int32_t size) {
  const std::vector<std::string> rows(static_cast<std::size_t>(size),
                                      std::string(static_cast<std::size_t>(size), '.'));
  const std::vector<std::string_view> views(rows.begin(), rows.end());
  return WalkMap::FromPicture(views);
}

// Un mur vertical au milieu, ouvert en haut et en bas : la ligne droite le
// traverse, le serveur le contourne.
WalkMap Walled() {
  constexpr std::array<std::string_view, 5> kRows = {
      ".....",  // y = 4
      "..#..",  // y = 3
      "..#..",  // y = 2
      "..#..",  // y = 1
      ".....",  // y = 0
  };
  return WalkMap::FromPicture(kRows);
}

// Un acteur qui marche à cent millisecondes la case.
Actor Walker(CellPosition from, CellPosition to, std::uint32_t started_ms) {
  Actor actor;
  actor.id = 1;
  actor.position = from;
  actor.speed_ms_per_cell = 100;
  actor.movement = CellMovement{from, to, 8, 8};
  actor.movement_started_ms = started_ms;
  return actor;
}

}  // namespace

TEST_CASE("Chemin : la ligne droite va en diagonale d'abord, puis tout droit", "[sim][walk]") {
  CHECK(StraightWalkPath(At(0, 0), At(3, 1)).directions == Steps{kNorthEast, kEast, kEast});
  CHECK(StraightWalkPath(At(5, 5), At(5, 2)).directions == Steps{kSouth, kSouth, kSouth});
  CHECK(StraightWalkPath(At(2, 2), At(0, 4)).directions == Steps{kNorthWest, kNorthWest});
  CHECK(StraightWalkPath(At(3, 3), At(3, 3)).directions.empty());
  // Sans carte, ni obstacle ni borne : le serveur a annoncé cette arrivée.
  CHECK(StraightWalkPath(At(0, 0), At(40, 0)).directions.size() == 40);
}

TEST_CASE("Chemin : en terrain dégagé, le serveur retient la diagonale d'abord", "[sim][walk]") {
  // L'heuristique du client surestime : un pas en diagonale coûte 14 mais en
  // retranche 20, et la recherche le préfère. Le résultat est celui de la ligne
  // droite — et c'est ce que le serveur fait marcher.
  const WalkMap map = Open(8);
  CHECK(DirectionsOf(FindWalkPath(map, At(0, 0), At(2, 1))) == Steps{kNorthEast, kEast});
  CHECK(DirectionsOf(FindWalkPath(map, At(0, 0), At(5, 2))) ==
        Steps{kNorthEast, kNorthEast, kEast, kEast, kEast});
  CHECK(DirectionsOf(FindWalkPath(map, At(7, 7), At(4, 4))) ==
        Steps{kSouthWest, kSouthWest, kSouthWest});
  CHECK(DirectionsOf(FindWalkPath(map, At(3, 3), At(3, 3))).empty());
}

TEST_CASE("Chemin : un coin ne se coupe pas", "[sim][walk]") {
  // Aller de (0,0) à (1,1) en diagonale exige que (1,0) ET (0,1) soient libres.
  // Sinon le serveur fait le tour, par la case libre.
  constexpr std::array<std::string_view, 3> kEastBlocked = {"...", "...", ".#."};
  CHECK(DirectionsOf(FindWalkPath(WalkMap::FromPicture(kEastBlocked), At(0, 0), At(1, 1))) ==
        Steps{kNorth, kEast});
  constexpr std::array<std::string_view, 3> kNorthBlocked = {"...", "#..", "..."};
  CHECK(DirectionsOf(FindWalkPath(WalkMap::FromPicture(kNorthBlocked), At(0, 0), At(1, 1))) ==
        Steps{kEast, kNorth});
}

TEST_CASE("Chemin : le serveur contourne un mur là où la ligne droite le traverse", "[sim][walk]") {
  // Aucun tour en cinq pas : chacun couperait un coin du mur. Des deux tours en
  // six, de même coût, c'est le tas du serveur qui départage — par le haut.
  const WalkMap map = Walled();
  CHECK(DirectionsOf(FindWalkPath(map, At(0, 2), At(4, 2))) ==
        Steps{kNorthEast, kNorth, kEast, kEast, kSouthEast, kSouth});
  CHECK(StraightWalkPath(At(0, 2), At(4, 2)).directions == Steps{kEast, kEast, kEast, kEast});
}

TEST_CASE("Chemin : trois parcours confrontés à une translittération indépendante", "[sim][walk]") {
  // Les attentes viennent d'une translittération en Python de path_search et
  // des macros de tas de rAthena, écrite à part : là où deux tours se valent,
  // les deux copies doivent trancher pareil, sans quoi l'une d'elles ment.
  constexpr std::array<std::string_view, 10> kMaze = {
      "..........", ".########.", ".#......#.", ".#.####.#.", ".#.#..#.#.",
      ".#.#..#.#.", ".#.####.#.", ".#......#.", ".########.", "..........",
  };
  CHECK(DirectionsOf(FindWalkPath(WalkMap::FromPicture(kMaze), At(0, 0), At(9, 9))) ==
        Steps{kEast, kEast, kEast, kEast, kEast, kEast, kEast, kEast, kEast, kNorth, kNorth, kNorth,
              kNorth, kNorth, kNorth, kNorth, kNorth, kNorth});

  constexpr std::array<std::string_view, 6> kChicane = {
      "............", "....#.......", "....#...#...",
      "....#...#...", "........#...", "............",
  };
  const WalkMap chicane = WalkMap::FromPicture(kChicane);
  CHECK(DirectionsOf(FindWalkPath(chicane, At(1, 1), At(10, 4))) ==
        Steps{kEast, kEast, kEast, kEast, kNorthEast, kNorthEast, kNorth, kEast, kEast, kEast});
  CHECK(DirectionsOf(FindWalkPath(chicane, At(10, 4), At(1, 1))) ==
        Steps{kWest, kWest, kWest, kSouthWest, kSouthWest, kSouth, kWest, kWest, kWest, kWest});
}

TEST_CASE("Chemin : refus quand l'arrivée est bloquée, hors carte ou inaccessible", "[sim][walk]") {
  constexpr std::array<std::string_view, 3> kCenterBlocked = {"...", ".#.", "..."};
  const WalkMap map = WalkMap::FromPicture(kCenterBlocked);
  CHECK_FALSE(FindWalkPath(map, At(0, 0), At(1, 1)).has_value());
  CHECK_FALSE(FindWalkPath(map, At(0, 0), At(5, 5)).has_value());
  CHECK_FALSE(FindWalkPath(map, At(5, 5), At(0, 0)).has_value());
  // Le départ, lui, n'est pas vérifié : on ne sortirait jamais d'une case bloquée.
  CHECK(DirectionsOf(FindWalkPath(map, At(1, 1), At(0, 2))) == Steps{kNorthWest});

  constexpr std::array<std::string_view, 3> kSplit = {".#...", ".#...", ".#..."};
  CHECK_FALSE(FindWalkPath(WalkMap::FromPicture(kSplit), At(0, 0), At(4, 0)).has_value());

  // La case (0,0) est injoignable : la table de nœuds du serveur, remplie de
  // zéros, la tient pour déjà vue. Aucune carte n'y met une case marchable,
  // et le reproduire vaut mieux que de le corriger sans le dire.
  CHECK_FALSE(FindWalkPath(Open(3), At(2, 2), At(0, 0)).has_value());
}

TEST_CASE("Chemin : trente-deux pas au plus, comme le serveur", "[sim][walk]") {
  const std::string corridor(40, '.');
  const std::array<std::string_view, 1> kRows = {corridor};
  const WalkMap map = WalkMap::FromPicture(kRows);
  CHECK(DirectionsOf(FindWalkPath(map, At(0, 0), At(32, 0))).size() == kMaxWalkSteps);
  CHECK_FALSE(FindWalkPath(map, At(0, 0), At(33, 0)).has_value());
}

TEST_CASE("Chemin facile : un obstacle sur la ligne le fait échouer", "[sim][walk]") {
  constexpr std::array<std::string_view, 5> kRows = {".....", ".....", "..#..", ".....", "....."};
  const WalkMap map = WalkMap::FromPicture(kRows);
  CHECK_FALSE(FindEasyWalkPath(map, At(0, 0), At(4, 4)).has_value());
  CHECK(DirectionsOf(FindEasyWalkPath(map, At(0, 0), At(4, 1))) ==
        Steps{kNorthEast, kEast, kEast, kEast});
  // La vraie recherche, elle, fait le tour — sans couper le coin de l'obstacle.
  CHECK(DirectionsOf(FindWalkPath(map, At(0, 0), At(4, 4))) ==
        Steps{kNorthEast, kEast, kEast, kNorthEast, kNorth, kNorth});
}

TEST_CASE("Grille : seuls le mur et la crevasse bloquent", "[sim][walk]") {
  constexpr std::array<std::uint8_t, 7> kCells = {0, 1, 2, 3, 4, 5, 6};
  const WalkMap map = WalkMap::FromServerCells(7, 1, kCells);
  CHECK(map.Passable(0, 0));
  CHECK_FALSE(map.Passable(1, 0));
  CHECK(map.Passable(2, 0));
  CHECK(map.Passable(3, 0));
  CHECK(map.Passable(4, 0));
  CHECK_FALSE(map.Passable(5, 0));
  CHECK(map.Passable(6, 0));
  CHECK_FALSE(map.Passable(7, 0));
  CHECK_FALSE(map.Passable(0, 1));
  CHECK_FALSE(map.Passable(-1, 0));
}

TEST_CASE("Pas : une diagonale dure quatorze dixièmes, en entier", "[sim][walk]") {
  CHECK(StepDurationMs(150, kEast) == 150);
  CHECK(StepDurationMs(150, kNorthEast) == 210);
  // 155 × 14 / 10 vaut 217 en entier, pas 217,5 : le serveur compte ainsi.
  CHECK(StepDurationMs(155, kSouthWest) == 217);
  for (const std::uint8_t direction : {kNorthWest, kSouthWest, kSouthEast, kNorthEast}) {
    CHECK(IsDiagonal(direction));
  }
  for (const std::uint8_t direction : {kNorth, kWest, kSouth, kEast}) {
    CHECK_FALSE(IsDiagonal(direction));
  }
}

TEST_CASE("Marche : le long du chemin du serveur, on contourne", "[sim][walk]") {
  const WalkMap map = Walled();
  WalkPlanner planner(&map);
  const Actor actor = Walker(At(0, 2), At(4, 2), 0);
  const WalkPath& path = planner.PathFor(actor);
  REQUIRE(path.directions.size() == 6);
  // Deux diagonales à 140 ms, quatre pas droits à 100 ms.
  CHECK(MovementDurationMs(actor, path) == 680);

  const auto start = InterpolatedCell(actor, path, 0);
  CHECK_THAT(start.x, WithinAbs(0.0F, 0.001F));
  CHECK_THAT(start.y, WithinAbs(2.0F, 0.001F));
  const auto first = InterpolatedCell(actor, path, 140);
  CHECK_THAT(first.x, WithinAbs(1.0F, 0.001F));
  CHECK_THAT(first.y, WithinAbs(3.0F, 0.001F));
  // À mi-chemin du troisième pas, droit vers l'est, le long du haut du mur.
  const auto middle = InterpolatedCell(actor, path, 240 + 50);
  CHECK_THAT(middle.x, WithinAbs(1.5F, 0.001F));
  CHECK_THAT(middle.y, WithinAbs(4.0F, 0.001F));
  CHECK(FacingAt(actor, path, 240 + 50) == kEast);
  const auto arrived = InterpolatedCell(actor, path, 680);
  CHECK_THAT(arrived.x, WithinAbs(4.0F, 0.001F));
  CHECK_THAT(arrived.y, WithinAbs(2.0F, 0.001F));
  CHECK(IsWalking(actor, path, 679));
  CHECK_FALSE(IsWalking(actor, path, 680));
  // Arrivé, on garde le regard du dernier pas : vers le sud.
  CHECK(FacingAt(actor, path, 700) == kSouth);
}

TEST_CASE("Marche : la sous-case dit d'où l'on part", "[sim][walk]") {
  // Un acteur aperçu en pleine marche porte sa position exacte, en seizièmes
  // de case. Engagé d'un quart sur son premier pas, il le finit depuis là.
  Actor engaged = Walker(At(5, 5), At(8, 5), 1000);
  engaged.movement->sub_x = 12;
  const WalkPath path = StraightWalkPath(At(5, 5), At(8, 5));
  CHECK_THAT(InterpolatedCell(engaged, path, 1000).x, WithinAbs(5.25F, 0.001F));
  CHECK_THAT(InterpolatedCell(engaged, path, 1075).x, WithinAbs(6.0F, 0.001F));
  CHECK(MovementDurationMs(engaged, path) == 275);

  // En retrait de la case annoncée, il la rejoint d'abord, en regardant vers elle.
  Actor behind = Walker(At(5, 5), At(8, 5), 1000);
  behind.movement->sub_x = 4;
  CHECK_THAT(InterpolatedCell(behind, path, 1000).x, WithinAbs(4.75F, 0.001F));
  CHECK(FacingAt(behind, path, 1000) == kEast);
  CHECK_THAT(InterpolatedCell(behind, path, 1025).x, WithinAbs(5.0F, 0.001F));
  CHECK(MovementDurationMs(behind, path) == 325);

  // Au centre, rien ne change.
  const Actor centered = Walker(At(5, 5), At(8, 5), 1000);
  CHECK(MovementDurationMs(centered, path) == 300);
}

TEST_CASE("Planificateur : un chemin par déplacement, recalculé quand il change", "[sim][walk]") {
  const WalkMap map = Walled();
  WalkPlanner planner(&map);
  Actor actor = Walker(At(0, 2), At(4, 2), 0);
  const WalkPath* first = &planner.PathFor(actor);
  CHECK(first == &planner.PathFor(actor));
  CHECK(first->directions.size() == 6);

  actor.movement = CellMovement{At(0, 2), At(0, 4), 8, 8};
  CHECK(planner.PathFor(actor).directions == Steps{kNorth, kNorth});

  actor.movement.reset();
  CHECK(planner.PathFor(actor).directions.empty());

  // Sans carte, la ligne droite — à travers le mur.
  WalkPlanner blind(nullptr);
  CHECK(blind.PathFor(Walker(At(0, 2), At(4, 2), 0)).directions ==
        Steps{kEast, kEast, kEast, kEast});
}
