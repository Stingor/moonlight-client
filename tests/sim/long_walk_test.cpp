#include "ro/sim/long_walk.h"

#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/pending_action.h"
#include "ro/sim/walk_map.h"
#include "ro/sim/walk_path.h"
#include "ro/sim/world.h"

using ro::sim::Actor;
using ro::sim::ActorKind;
using ro::sim::AdvanceLongWalk;
using ro::sim::ApproachWalkClock;
using ro::sim::BeginLongWalk;
using ro::sim::CellMovement;
using ro::sim::CellPosition;
using ro::sim::FindRoute;
using ro::sim::FindWalkPath;
using ro::sim::kLegConfirmMs;
using ro::sim::LongWalk;
using ro::sim::StepsApart;
using ro::sim::WalkMap;
using ro::sim::WalkPlanner;
using ro::sim::WalkStopToward;
using ro::sim::World;

namespace {

constexpr std::uint32_t kPlayerId = 150'001;
constexpr std::uint16_t kSpeedMsPerCell = 150;
// La borne de rAthena d'usine.
constexpr std::size_t kServerSteps = 17;
constexpr std::int32_t kOpenSide = 64;

CellPosition At(std::uint16_t x, std::uint16_t y) { return CellPosition{x, y, 0}; }

bool Same(CellPosition a, CellPosition b) { return a.x == b.x && a.y == b.y; }

WalkMap Open(std::int32_t size) {
  const std::vector<std::string> rows(static_cast<std::size_t>(size),
                                      std::string(static_cast<std::size_t>(size), '.'));
  const std::vector<std::string_view> views(rows.begin(), rows.end());
  return WalkMap::FromPicture(views);
}

// Le serveur accepte-t-il de marcher de `from` à `to` ?
bool ServerAccepts(const WalkMap& map, CellPosition from, CellPosition to) {
  const auto path = FindWalkPath(map, from, to);
  return path.has_value() && path->directions.size() <= kServerSteps;
}

World WorldWith(const WalkMap& map, CellPosition where) {
  World world;
  world.SetWalkMap(&map);
  world.set_player_id(kPlayerId);
  Actor player;
  player.id = kPlayerId;
  player.kind = ActorKind::kPlayer;
  player.position = where;
  player.speed_ms_per_cell = kSpeedMsPerCell;
  world.Spawn(player);
  world.session().max_walk_path_steps = static_cast<int>(kServerSteps);
  return world;
}

}  // namespace

TEST_CASE("Marche par étapes : le fil complet compte un pas par case de Tchebychev",
          "[sim][long_walk]") {
  const WalkMap map = Open(kOpenSide);
  const auto route = FindRoute(map, At(2, 2), At(50, 20));
  REQUIRE(route.has_value());
  CHECK(route->size() == StepsApart(At(2, 2), At(50, 20)));
  CHECK(Same(route->back(), At(50, 20)));
}

TEST_CASE("Marche par étapes : le fil contourne un mur, et rien ne relie une case enfermée",
          "[sim][long_walk]") {
  // Un mur nord-sud percé en bas ; la case (6, 4) est enfermée.
  const std::vector<std::string_view> rows = {
      "...#......",  //
      "...#.###..",  //
      "...#.#.#..",  //
      "...#.###..",  //
      "...#......",  //
      "..........",  //
  };
  const WalkMap map = WalkMap::FromPicture(rows);
  const auto around = FindRoute(map, At(0, 5), At(9, 5));
  REQUIRE(around.has_value());
  for (const CellPosition cell : *around) {
    CHECK(map.Passable(cell.x, cell.y));
  }
  CHECK_FALSE(FindRoute(map, At(0, 5), At(6, 3)).has_value());
}

TEST_CASE("Marche par étapes : deux cases sont reliées exactement quand le fil les joint",
          "[sim][long_walk]") {
  // La diagonale de (2, 4) à (3, 3) est interdite : ses deux côtés sont bloqués.
  const std::vector<std::string_view> rows = {
      "...#.#",  //
      "...#.#",  //
      "..#..#",  //
      "..#...",  //
      "###.##",  //
      ".#..#.",  //
  };
  const WalkMap map = WalkMap::FromPicture(rows);
  CHECK_FALSE(map.Connected(1, 3, 3, 3));
  CHECK(map.Connected(3, 3, 2, 0));
  CHECK_FALSE(map.Connected(-1, 0, 0, 0));
  CHECK_FALSE(map.Connected(0, 5, 0, 1));

  // L'étalon parcourt huit directions, sous la règle de la diagonale du fil.
  const auto reached_from = [&map](std::int32_t from_x, std::int32_t from_y) {
    std::set<std::pair<std::int32_t, std::int32_t>> seen = {{from_x, from_y}};
    std::vector<std::pair<std::int32_t, std::int32_t>> pending = {{from_x, from_y}};
    while (!pending.empty()) {
      const auto [x, y] = pending.back();
      pending.pop_back();
      for (std::int32_t dy = -1; dy <= 1; ++dy) {
        for (std::int32_t dx = -1; dx <= 1; ++dx) {
          const bool diagonal = dx != 0 && dy != 0;
          if (!map.Passable(x + dx, y + dy) ||
              (diagonal && (!map.Passable(x + dx, y) || !map.Passable(x, y + dy)))) {
            continue;
          }
          if (seen.insert({x + dx, y + dy}).second) {
            pending.emplace_back(x + dx, y + dy);
          }
        }
      }
    }
    return seen;
  };
  for (std::int32_t from_y = 0; from_y < map.height(); ++from_y) {
    for (std::int32_t from_x = 0; from_x < map.width(); ++from_x) {
      if (!map.Passable(from_x, from_y)) {
        continue;
      }
      const auto reached = reached_from(from_x, from_y);
      for (std::int32_t to_y = 0; to_y < map.height(); ++to_y) {
        for (std::int32_t to_x = 0; to_x < map.width(); ++to_x) {
          if (map.Passable(to_x, to_y)) {
            CHECK(map.Connected(from_x, from_y, to_x, to_y) == reached.contains({to_x, to_y}));
          }
        }
      }
    }
  }
}

TEST_CASE("Marche par étapes : un mur de script coupe puis rend le passage", "[sim][long_walk]") {
  // Le seul passage entre les deux moitiés est la case (1, 1).
  const std::vector<std::string_view> rows = {
      "...",  //
      "#.#",  //
      "...",  //
  };
  WalkMap map = WalkMap::FromPicture(rows);
  REQUIRE(map.Connected(0, 0, 0, 2));

  map.SetServerCell(1, 1, ro::sim::kServerCellWall);
  CHECK_FALSE(map.Passable(1, 1));
  CHECK_FALSE(map.Connected(0, 0, 0, 2));

  constexpr std::uint8_t kServerCellWalkable = 0;
  map.SetServerCell(1, 1, kServerCellWalkable);
  CHECK(map.Passable(1, 1));
  CHECK(map.Connected(0, 0, 0, 2));

  // Hors de la carte, rien ne change ni ne casse.
  map.SetServerCell(-1, 7, ro::sim::kServerCellWall);
}

TEST_CASE("Marche par étapes : une case de départ bloquée peut encore sortir", "[sim][long_walk]") {
  const std::vector<std::string_view> rows = {
      "...",  //
      ".#.",  //
      "...",  //
  };
  const WalkMap map = WalkMap::FromPicture(rows);
  CHECK(FindRoute(map, At(1, 1), At(0, 0)).has_value());
}

TEST_CASE("Marche par étapes : à portée d'une marche, la destination part telle quelle",
          "[sim][long_walk]") {
  const WalkMap map = Open(kOpenSide);
  CHECK(Same(WalkStopToward(map, At(10, 10), At(20, 15), kServerSteps), At(20, 15)));
}

TEST_CASE("Marche par étapes : au-delà, l'étape est la case la plus avancée que le serveur accepte",
          "[sim][long_walk]") {
  const WalkMap map = Open(kOpenSide);
  const CellPosition from = At(5, 30);
  const CellPosition stop = WalkStopToward(map, from, At(60, 30), kServerSteps);
  CHECK(Same(stop, At(5 + kServerSteps, 30)));
  CHECK(ServerAccepts(map, from, stop));
}

TEST_CASE("Marche par étapes : derrière un mur, l'étape reste sur le fil et le serveur l'accepte",
          "[sim][long_walk]") {
  // Un long mur horizontal qu'il faut contourner par l'ouest.
  std::vector<std::string> rows(40, std::string(40, '.'));
  for (std::size_t x = 4; x < 40; ++x) {
    rows[20][x] = '#';
  }
  const std::vector<std::string_view> views(rows.begin(), rows.end());
  const WalkMap map = WalkMap::FromPicture(views);
  // La première ligne est le nord : la ligne 20 tient y = 19.
  const CellPosition from = At(30, 5);
  const CellPosition to = At(30, 35);
  const CellPosition stop = WalkStopToward(map, from, to, kServerSteps);
  CHECK_FALSE(Same(stop, to));
  CHECK(ServerAccepts(map, from, stop));
  CHECK(StepsApart(from, stop) > 1);
}

TEST_CASE("Marche par étapes : un clic proche ne retient rien", "[sim][long_walk]") {
  const WalkMap map = Open(kOpenSide);
  World world = WorldWith(map, At(10, 10));
  std::optional<LongWalk> walk;
  CHECK(Same(BeginLongWalk(world, walk, At(15, 12), 0), At(15, 12)));
  CHECK_FALSE(walk.has_value());
}

TEST_CASE("Marche par étapes : l'étape suivante part à la fin de la précédente",
          "[sim][long_walk]") {
  const WalkMap map = Open(kOpenSide);
  World world = WorldWith(map, At(5, 30));
  WalkPlanner planner(&map);
  ApproachWalkClock clock;
  std::optional<LongWalk> walk;

  const std::uint32_t sent_ms = 1000;
  const CellPosition first = BeginLongWalk(world, walk, At(60, 30), sent_ms);
  REQUIRE(walk.has_value());
  CHECK(Same(first, At(5 + kServerSteps, 30)));

  // Le serveur ne l'a pas encore fait partir : on attend.
  CHECK_FALSE(AdvanceLongWalk(world, planner, walk, clock, sent_ms + 50).has_value());
  REQUIRE(walk.has_value());

  // Il la fait partir.
  const std::uint32_t started_ms = sent_ms + 80;
  REQUIRE(world.StartMoving(kPlayerId, CellMovement{At(5, 30), first, 8, 8}, started_ms));
  // En pleine marche, rien.
  CHECK_FALSE(
      AdvanceLongWalk(world, planner, walk, clock, started_ms + 5 * kSpeedMsPerCell).has_value());
  // À deux pas de la fin, l'étape suivante.
  const std::uint32_t near_end_ms =
      started_ms + static_cast<std::uint32_t>(kServerSteps - 2) * kSpeedMsPerCell;
  const auto next = AdvanceLongWalk(world, planner, walk, clock, near_end_ms);
  REQUIRE(next.has_value());
  CHECK(next->x > first.x);
  CHECK(next->y == 30);
}

TEST_CASE("Marche par étapes : une étape que le serveur ne fait pas partir est abandonnée",
          "[sim][long_walk]") {
  const WalkMap map = Open(kOpenSide);
  World world = WorldWith(map, At(5, 30));
  WalkPlanner planner(&map);
  ApproachWalkClock clock;
  std::optional<LongWalk> walk;
  static_cast<void>(BeginLongWalk(world, walk, At(60, 30), 0));
  REQUIRE(walk.has_value());
  CHECK_FALSE(AdvanceLongWalk(world, planner, walk, clock, kLegConfirmMs + 1).has_value());
  CHECK_FALSE(walk.has_value());
}

TEST_CASE("Marche par étapes : une autre marche prend la place de l'étape", "[sim][long_walk]") {
  const WalkMap map = Open(kOpenSide);
  World world = WorldWith(map, At(5, 30));
  WalkPlanner planner(&map);
  ApproachWalkClock clock;
  std::optional<LongWalk> walk;
  static_cast<void>(BeginLongWalk(world, walk, At(60, 30), 0));
  // Une attaque fait marcher ailleurs.
  REQUIRE(world.StartMoving(kPlayerId, CellMovement{At(5, 30), At(5, 40), 8, 8}, 100));
  CHECK_FALSE(AdvanceLongWalk(world, planner, walk, clock, kLegConfirmMs + 200).has_value());
  CHECK_FALSE(walk.has_value());
}
