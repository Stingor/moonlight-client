#include "ro/sim/navi_route.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/navi_tables.h"
#include "ro/sim/walk_field.h"
#include "ro/sim/walk_path.h"

using ro::formats::NaviData;
using ro::formats::NaviLink;
using ro::formats::NaviMap;
using ro::formats::NaviNpc;
using ro::sim::kPortalHopCost;
using ro::sim::NaviGoal;
using ro::sim::NaviGoalCost;
using ro::sim::NaviGoalKind;
using ro::sim::NaviRouteOptions;
using ro::sim::NaviStart;
using ro::sim::OctileCost;
using ro::sim::PlanNaviRoute;

namespace {

constexpr std::uint16_t kTown = 0;
constexpr std::uint16_t kField = 1;
constexpr std::uint16_t kDungeon = 2;
constexpr std::uint16_t kIsland = 3;

NaviLink Link(std::uint16_t from, std::uint32_t id, std::uint16_t kind, std::int16_t x,
              std::int16_t y, std::uint16_t to, std::int16_t dest_x, std::int16_t dest_y) {
  NaviLink link;
  link.map = from;
  link.link_id = id;
  link.kind = kind;
  link.cell_x = x;
  link.cell_y = y;
  link.dest_map = to;
  link.dest_cell_x = dest_x;
  link.dest_cell_y = dest_y;
  return link;
}

// Une ville, un champ, un donjon au fond du champ, et une île que seul un
// service de transport relie à la ville.
NaviData World() {
  NaviData data;
  for (const char* name : {"town", "field", "dungeon", "island"})
    data.maps.push_back(NaviMap{name, name, 5001, 100, 100});
  data.links = {
      Link(kTown, 1000, 200, 90, 50, kField, 10, 50),
      Link(kField, 1001, 200, 5, 50, kTown, 85, 50),
      Link(kField, 1002, 200, 90, 50, kDungeon, 10, 10),
      Link(kTown, 1003, 204, 50, 50, kIsland, 20, 20),
  };
  NaviNpc npc;
  npc.map = kField;
  npc.npc_id = 2000;
  npc.name = "Guide";
  npc.cell_x = 40;
  npc.cell_y = 50;
  data.npcs.push_back(npc);
  data.BuildIndexes();

  // Sorti de 1000 sur le champ : le retour à cinq pas, le donjon à quatre-vingts.
  data.link_passes = {{1001, 1, 5}, {1002, 1, 80}};
  data.link_pass_spans[1000] = {0, 2};
  return data;
}

// L'arrivée au PNJ 2000 : la marche, en pas, depuis la sortie des seules
// liaisons nommées ; les autres ne le joignent pas.
NaviGoalCost ArrivalAfter(std::uint32_t link_id, std::uint32_t steps) {
  return [link_id, steps](const NaviLink& link) -> std::optional<std::uint32_t> {
    if (link.link_id != link_id) return std::nullopt;
    return steps * ro::sim::kMoveCost;
  };
}

NaviGoal NpcGoal() {
  NaviGoal goal;
  goal.kind = NaviGoalKind::kNpc;
  goal.npc_id = 2000;
  return goal;
}

std::optional<std::uint32_t> NoWalk(std::int32_t, std::int32_t) { return 0; }

NaviGoal MapGoal(std::uint16_t map) {
  NaviGoal goal;
  goal.kind = NaviGoalKind::kMap;
  goal.map = map;
  return goal;
}

}  // namespace

TEST_CASE("Itinéraire : d'une carte à l'autre par les liaisons", "[sim][navi]") {
  const NaviData data = World();
  const NaviStart start{kTown, 80, 50};
  const auto route = PlanNaviRoute(data, start, MapGoal(kDungeon), NaviRouteOptions{});
  REQUIRE(route.has_value());
  REQUIRE(route->link_ids.size() == 2);
  CHECK(route->link_ids[0] == 1000);
  CHECK(route->link_ids[1] == 1002);
  // La marche jusqu'au portail, deux forfaits, et quatre-vingts pas de champ.
  const std::uint32_t expected =
      OctileCost(80, 50, 90, 50) + kPortalHopCost + 80 * ro::sim::kMoveCost + kPortalHopCost;
  CHECK(route->total_cost == expected);
}

TEST_CASE("Itinéraire : le service de transport se refuse sur demande", "[sim][navi]") {
  const NaviData data = World();
  const NaviStart start{kTown, 50, 50};
  NaviRouteOptions walking_only;
  walking_only.allow_transport_services = false;
  CHECK_FALSE(PlanNaviRoute(data, start, MapGoal(kIsland), walking_only).has_value());
  const auto route = PlanNaviRoute(data, start, MapGoal(kIsland), NaviRouteOptions{});
  REQUIRE(route.has_value());
  REQUIRE(route->link_ids.size() == 1);
  CHECK(route->link_ids[0] == 1003);
}

TEST_CASE("Itinéraire : sur la même carte, aucune liaison à franchir", "[sim][navi]") {
  const NaviData data = World();
  NaviGoal goal;
  goal.kind = NaviGoalKind::kCell;
  goal.map = kTown;
  goal.cell_x = 10;
  goal.cell_y = 10;
  const auto route = PlanNaviRoute(data, NaviStart{kTown, 20, 20}, goal, NaviRouteOptions{});
  REQUIRE(route.has_value());
  CHECK(route->link_ids.empty());
  CHECK(route->total_cost == OctileCost(20, 20, 10, 10));
}

TEST_CASE("Itinéraire : un PNJ se rejoint par la marche mesurée depuis l'arrivée", "[sim][navi]") {
  const NaviData data = World();
  // Le départ se mesure par la fonction fournie : ici, une marche de 7 pas.
  const auto start_cost = [](std::int32_t, std::int32_t) -> std::optional<std::uint32_t> {
    return 7 * ro::sim::kMoveCost;
  };
  const auto route = PlanNaviRoute(data, NaviStart{kTown, 0, 0}, NpcGoal(), NaviRouteOptions{},
                                   start_cost, ArrivalAfter(1000, 30));
  REQUIRE(route.has_value());
  REQUIRE(route->link_ids.size() == 1);
  CHECK(route->total_cost == 7 * ro::sim::kMoveCost + kPortalHopCost + 30 * ro::sim::kMoveCost);
}

TEST_CASE("Itinéraire : une arrivée qui ne joint pas le PNJ n'est pas un dernier passage",
          "[sim][navi]") {
  // Deux liaisons entrent sur le champ ; seule la plus lointaine joint le PNJ à
  // pied. L'itinéraire ne s'arrête pas à l'autre, si proche soit-elle.
  NaviData data = World();
  data.links.push_back(Link(kTown, 1004, 200, 10, 50, kField, 95, 95));
  data.BuildIndexes();
  const auto route = PlanNaviRoute(data, NaviStart{kTown, 85, 50}, NpcGoal(), NaviRouteOptions{},
                                   {}, ArrivalAfter(1004, 60));
  REQUIRE(route.has_value());
  REQUIRE(route->link_ids.size() == 1);
  CHECK(route->link_ids[0] == 1004);
}

TEST_CASE("Itinéraire : une destination qu'aucune arrivée ne joint n'est pas une route",
          "[sim][navi]") {
  const NaviData data = World();
  const auto walled = [](const NaviLink&) -> std::optional<std::uint32_t> { return std::nullopt; };
  CHECK_FALSE(
      PlanNaviRoute(data, NaviStart{kTown, 0, 0}, NpcGoal(), NaviRouteOptions{}, NoWalk, walled)
          .has_value());
  // Une arrivée mesurée remplace l'estimation à vol d'oiseau.
  const auto measured = [](const NaviLink&) -> std::optional<std::uint32_t> {
    return 45 * ro::sim::kMoveCost;
  };
  const auto route =
      PlanNaviRoute(data, NaviStart{kTown, 0, 0}, NpcGoal(), NaviRouteOptions{}, NoWalk, measured);
  REQUIRE(route.has_value());
  CHECK(route->total_cost == kPortalHopCost + 45 * ro::sim::kMoveCost);
}

TEST_CASE("Itinéraire : la marche de la table se compte telle quelle", "[sim][navi]") {
  // Plus de 255 pas comptent en entier ; et une valeur courte n'est pas
  // redressée, même quand elle couvre moins que l'écart des cases.
  NaviData data = World();
  const auto start_cost = [](std::int32_t, std::int32_t) -> std::optional<std::uint32_t> {
    return 0;
  };
  data.link_passes = {{1001, 1, 5}, {1002, 1, 80 + 256}};
  const auto far = PlanNaviRoute(data, NaviStart{kTown, 90, 50}, MapGoal(kDungeon),
                                 NaviRouteOptions{}, start_cost);
  REQUIRE(far.has_value());
  CHECK(far->total_cost == 2 * kPortalHopCost + (80 + 256) * ro::sim::kMoveCost);

  data.link_passes = {{1001, 1, 5}, {1002, 1, 7}};
  const auto short_leg = PlanNaviRoute(data, NaviStart{kTown, 90, 50}, MapGoal(kDungeon),
                                       NaviRouteOptions{}, start_cost);
  REQUIRE(short_leg.has_value());
  CHECK(short_leg->total_cost == 2 * kPortalHopCost + 7 * ro::sim::kMoveCost);
}

TEST_CASE("Itinéraire : un départ injoignable n'est pas une route", "[sim][navi]") {
  const NaviData data = World();
  const auto walled = [](std::int32_t, std::int32_t) -> std::optional<std::uint32_t> {
    return std::nullopt;
  };
  CHECK_FALSE(
      PlanNaviRoute(data, NaviStart{kTown, 0, 0}, MapGoal(kDungeon), NaviRouteOptions{}, walled)
          .has_value());
}
