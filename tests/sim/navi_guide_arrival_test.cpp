// L'ARRIVÉE SUR LA CARTE VISÉE : mesurée sur sa grille ; sans grille, pas
// d'itinéraire.

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/navi_tables.h"
#include "ro/sim/navi_guide.h"
#include "ro/sim/navi_route.h"
#include "ro/sim/walk_field.h"
#include "ro/sim/walk_map.h"
#include "ro/sim/walk_path.h"

using ro::formats::NaviData;
using ro::formats::NaviLink;
using ro::formats::NaviMap;
using ro::formats::NaviNpc;
using ro::sim::CellPosition;
using ro::sim::kMoveCost;
using ro::sim::kPortalHopCost;
using ro::sim::NaviGoalKind;
using ro::sim::NaviGoalTerrain;
using ro::sim::NaviGuide;
using ro::sim::NaviGuideRequest;
using ro::sim::NaviGuideStatus;
using ro::sim::OctileCost;
using ro::sim::WalkField;
using ro::sim::WalkMap;

namespace {

constexpr std::uint16_t kTown = 0;
constexpr std::uint16_t kField = 1;
constexpr std::uint32_t kPortal = 100;
constexpr std::uint32_t kHermit = 500;
// L'arrivée du portail sur le champ, et le PNJ de l'autre côté d'un mur.
constexpr std::int32_t kArrivalX = 0;
constexpr std::int32_t kArrivalY = 2;
constexpr std::int32_t kHermitX = 8;
constexpr std::int32_t kHermitY = 2;
// La marche du joueur jusqu'au portail : huit cases droites sur la ville.
constexpr std::uint32_t kWalkToPortal = 8 * kMoveCost;

std::shared_ptr<const NaviData> TownAndField() {
  auto data = std::make_shared<NaviData>();
  data->maps = {NaviMap{"town", "Town", 5001, 10, 3}, NaviMap{"field", "Field", 5001, 10, 5}};
  NaviLink portal;
  portal.map = kTown;
  portal.link_id = kPortal;
  portal.cell_x = 9;
  portal.cell_y = 1;
  portal.dest_map = kField;
  portal.dest_cell_x = kArrivalX;
  portal.dest_cell_y = kArrivalY;
  data->links.push_back(portal);
  NaviNpc hermit;
  hermit.map = kField;
  hermit.npc_id = kHermit;
  hermit.name = "Hermit";
  hermit.cell_x = kHermitX;
  hermit.cell_y = kHermitY;
  data->npcs.push_back(hermit);
  data->BuildIndexes();
  return data;
}

WalkMap Town() {
  constexpr std::array<std::string_view, 3> kRows{"..........", "..........", ".........."};
  return WalkMap::FromPicture(kRows);
}

// Un mur coupe le champ, ouvert seulement au nord.
WalkMap WalledField() {
  constexpr std::array<std::string_view, 5> kRows{"..........", "....#.....", "....#.....",
                                                  "....#.....", "....#....."};
  return WalkMap::FromPicture(kRows);
}

// Le même mur, fermé de bout en bout.
WalkMap SplitField() {
  constexpr std::array<std::string_view, 5> kRows{"....#.....", "....#.....", "....#.....",
                                                  "....#.....", "....#....."};
  return WalkMap::FromPicture(kRows);
}

// Ce que l'application lirait dans l'archive, ici posé à la main.
class FakeTerrain final : public NaviGoalTerrain {
 public:
  const WalkMap* field = nullptr;
  int grid_asks = 0;

  const WalkMap* GridOf(std::string_view map_name) override {
    ++grid_asks;
    return map_name == "field" ? field : nullptr;
  }
};

NaviGuideRequest ToHermit() {
  NaviGuideRequest request;
  request.kind = NaviGoalKind::kNpc;
  request.npc_id = kHermit;
  return request;
}

CellPosition At(std::uint16_t x, std::uint16_t y) { return {x, y, 0}; }

// Le guidage demandé depuis la ville, case (1, 1).
NaviGuide& PlannedFromTown(NaviGuide& guide, const WalkMap& town, NaviGoalTerrain* terrain,
                           NaviGuideRequest request) {
  guide.SetData(TownAndField());
  guide.SetGoalTerrain(terrain);
  guide.Request(std::move(request));
  guide.Update("town", &town, At(1, 1));
  return guide;
}

}  // namespace

TEST_CASE("Arrivée : la marche jusqu'au PNJ se mesure sur la grille de sa carte",
          "[sim][navi_guide]") {
  const WalkMap town = Town();
  const WalkMap field = WalledField();
  FakeTerrain terrain;
  terrain.field = &field;
  NaviGuide guide;
  PlannedFromTown(guide, town, &terrain, ToHermit());
  REQUIRE(guide.status() == NaviGuideStatus::kGuiding);
  REQUIRE(guide.steps().size() == 1);

  // Le détour par le nord, que le vol d'oiseau ignore.
  const std::array<CellPosition, 1> hermit{At(kHermitX, kHermitY)};
  const std::optional<std::uint32_t> around =
      WalkField::Flood(field, hermit).CostFrom(kArrivalX, kArrivalY);
  REQUIRE(around.has_value());
  CHECK(*around > OctileCost(kArrivalX, kArrivalY, kHermitX, kHermitY));
  CHECK(guide.estimated_cost() == kWalkToPortal + kPortalHopCost + *around);
  // Une seule lecture de la grille pour tout l'itinéraire.
  CHECK(terrain.grid_asks == 1);
}

TEST_CASE("Arrivée : une case visée se mesure aussi sur la grille", "[sim][navi_guide]") {
  const WalkMap town = Town();
  const WalkMap field = WalledField();
  FakeTerrain terrain;
  terrain.field = &field;
  NaviGuideRequest request;
  request.map_name = "field";
  request.kind = NaviGoalKind::kCell;
  request.cell_x = kHermitX;
  request.cell_y = kHermitY;
  NaviGuide guide;
  PlannedFromTown(guide, town, &terrain, request);
  REQUIRE(guide.status() == NaviGuideStatus::kGuiding);
  const std::array<CellPosition, 1> target{At(kHermitX, kHermitY)};
  CHECK(guide.estimated_cost() ==
        kWalkToPortal + kPortalHopCost +
            *WalkField::Flood(field, target).CostFrom(kArrivalX, kArrivalY));
}

TEST_CASE("Arrivée : un PNJ que la marche ne joint pas n'a pas d'itinéraire", "[sim][navi_guide]") {
  const WalkMap town = Town();
  const WalkMap field = SplitField();
  FakeTerrain terrain;
  terrain.field = &field;
  NaviGuide guide;
  PlannedFromTown(guide, town, &terrain, ToHermit());
  CHECK(guide.status() == NaviGuideStatus::kNoRoute);
}

TEST_CASE("Arrivée : sans grille, pas d'itinéraire", "[sim][navi_guide]") {
  // Rien ne dit la marche depuis l'arrivée du portail : elle n'est pas estimée.
  const WalkMap town = Town();
  FakeTerrain empty;
  NaviGuide guide;
  PlannedFromTown(guide, town, &empty, ToHermit());
  CHECK(guide.status() == NaviGuideStatus::kNoRoute);

  NaviGuide alone;
  PlannedFromTown(alone, town, nullptr, ToHermit());
  CHECK(alone.status() == NaviGuideStatus::kNoRoute);

  // Une case non plus.
  NaviGuideRequest cell;
  cell.map_name = "field";
  cell.kind = NaviGoalKind::kCell;
  cell.cell_x = kHermitX;
  cell.cell_y = kHermitY;
  NaviGuide toward_cell;
  PlannedFromTown(toward_cell, town, &empty, cell);
  CHECK(toward_cell.status() == NaviGuideStatus::kNoRoute);
}
