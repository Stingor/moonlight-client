#include "ro/sim/navi_guide.h"

#include <array>
#include <memory>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/navi_tables.h"
#include "ro/sim/walk_map.h"

using ro::formats::NaviData;
using ro::formats::NaviLink;
using ro::formats::NaviMap;
using ro::sim::CellPosition;
using ro::sim::NaviGoalKind;
using ro::sim::NaviGuide;
using ro::sim::NaviGuideRequest;
using ro::sim::NaviGuideStatus;
using ro::sim::WalkMap;

namespace {

constexpr std::uint16_t kTown = 0;
constexpr std::uint16_t kField = 1;

// Une ville et un champ reliés par un portail à l'est de la ville.
std::shared_ptr<const NaviData> TwoMaps() {
  auto data = std::make_shared<NaviData>();
  data->maps = {NaviMap{"town", "Town", 5001, 10, 3}, NaviMap{"field", "Field", 5001, 10, 3}};
  NaviLink portal;
  portal.map = kTown;
  portal.link_id = 100;
  portal.cell_x = 9;
  portal.cell_y = 1;
  portal.dest_map = kField;
  portal.dest_cell_x = 0;
  portal.dest_cell_y = 1;
  data->links.push_back(portal);
  data->BuildIndexes();
  return data;
}

WalkMap Open() {
  constexpr std::array<std::string_view, 3> kRows{"..........", "..........", ".........."};
  return WalkMap::FromPicture(kRows);
}

CellPosition At(std::uint16_t x, std::uint16_t y) { return {x, y, 0}; }

// La grille du champ, que l'application lirait dans l'archive : l'arrivée s'y
// mesure.
class OpenField final : public ro::sim::NaviGoalTerrain {
 public:
  const WalkMap* GridOf(std::string_view map_name) override {
    return map_name == "field" ? &field_ : nullptr;
  }

 private:
  WalkMap field_ = Open();
};

NaviGuideRequest CellOnField(std::int32_t x, std::int32_t y) {
  NaviGuideRequest request;
  request.map_name = "field";
  request.kind = NaviGoalKind::kCell;
  request.cell_x = x;
  request.cell_y = y;
  return request;
}

}  // namespace

TEST_CASE("Guidage : on suit l'itinéraire d'une carte à l'autre jusqu'à l'arrivée",
          "[sim][navi_guide]") {
  const WalkMap town = Open();
  const WalkMap field = Open();
  OpenField terrain;
  NaviGuide guide;
  guide.SetGoalTerrain(&terrain);
  guide.SetData(TwoMaps());
  guide.Request(CellOnField(8, 1));
  CHECK(guide.status() == NaviGuideStatus::kPending);

  guide.Update("town", &town, At(1, 1));
  REQUIRE(guide.status() == NaviGuideStatus::kGuiding);
  REQUIRE(guide.steps().size() == 1);
  CHECK(guide.current_step() == 0);
  REQUIRE(guide.local_target().has_value());
  CHECK(guide.local_target()->x == 9);
  // Une ligne droite : ses deux bouts suffisent.
  REQUIRE(guide.path_corners().size() == 2);
  CHECK(guide.path_corners().front().x == 1);
  CHECK(guide.path_corners().back().x == 9);

  // Un pas de côté ne demande aucun recalcul : le chemin se relit d'ici.
  guide.Update("town", &town, At(4, 2));
  CHECK(guide.path_cells().front().x == 4);
  CHECK(guide.path_cells().back().x == 9);

  guide.Update("field", &field, At(0, 1));
  CHECK(guide.current_step() == 1);
  REQUIRE(guide.local_target().has_value());
  CHECK(guide.local_target()->x == 8);
  CHECK(guide.status() == NaviGuideStatus::kGuiding);

  guide.Update("field", &field, At(7, 1));
  CHECK(guide.status() == NaviGuideStatus::kArrived);
  CHECK_FALSE(guide.active());
}

TEST_CASE("Guidage : déjà sur place, aucun chemin, et arrêt", "[sim][navi_guide]") {
  const WalkMap town = Open();
  OpenField terrain;
  NaviGuide guide;
  guide.SetGoalTerrain(&terrain);
  guide.SetData(TwoMaps());

  NaviGuideRequest here;
  here.map_name = "town";
  guide.Request(here);
  guide.Update("town", &town, At(1, 1));
  CHECK(guide.status() == NaviGuideStatus::kAlreadyHere);

  NaviGuideRequest nowhere;
  nowhere.map_name = "atlantis";
  guide.Request(nowhere);
  guide.Update("town", &town, At(1, 1));
  CHECK(guide.status() == NaviGuideStatus::kNoRoute);

  guide.Request(CellOnField(8, 1));
  guide.Update("town", &town, At(1, 1));
  REQUIRE(guide.active());
  guide.Stop();
  CHECK(guide.status() == NaviGuideStatus::kIdle);
  CHECK(guide.path_corners().empty());
}

TEST_CASE("Guidage : une demande attend les données et une carte connue", "[sim][navi_guide]") {
  const WalkMap town = Open();
  OpenField terrain;
  NaviGuide guide;
  guide.SetGoalTerrain(&terrain);
  guide.Request(CellOnField(8, 1));
  guide.Update("town", &town, At(1, 1));
  CHECK(guide.status() == NaviGuideStatus::kPending);
  guide.SetData(TwoMaps());
  // Une carte hors du graphe — une instance — ne lance rien.
  guide.Update("1@tower", &town, At(1, 1));
  CHECK(guide.status() == NaviGuideStatus::kPending);
  guide.Update("town", &town, At(1, 1));
  CHECK(guide.status() == NaviGuideStatus::kGuiding);
}

namespace {

// Une carte, deux pièces que sépare un mur, et un portail qui mène de l'ouest à
// l'est SANS changer de carte — le cas de #ep172_room02_in dans ba_in01.
std::shared_ptr<const NaviData> TwoRooms() {
  auto data = std::make_shared<NaviData>();
  data->maps = {NaviMap{"manor", "Manor", 5003, 20, 3}};
  NaviLink portal;
  portal.map = 0;
  portal.link_id = 200;
  portal.cell_x = 1;
  portal.cell_y = 1;
  portal.dest_map = 0;
  portal.dest_cell_x = 18;
  portal.dest_cell_y = 1;
  data->links.push_back(portal);
  data->BuildIndexes();
  return data;
}

WalkMap Walled() {
  constexpr std::array<std::string_view, 3> kRows{".....#..............", ".....#..............",
                                                  ".....#.............."};
  return WalkMap::FromPicture(kRows);
}

NaviGuideRequest CellInManor(std::int32_t x, std::int32_t y) {
  NaviGuideRequest request;
  request.map_name = "manor";
  request.kind = NaviGoalKind::kCell;
  request.cell_x = x;
  request.cell_y = y;
  return request;
}

}  // namespace

TEST_CASE("Guidage : un portail qui reste sur la carte fait passer à l'étape suivante",
          "[sim][navi_guide]") {
  const WalkMap manor = Walled();
  NaviGuide guide;
  guide.SetData(TwoRooms());
  guide.Request(CellInManor(7, 1));

  guide.Update("manor", &manor, At(3, 1));
  REQUIRE(guide.status() == NaviGuideStatus::kGuiding);
  REQUIRE(guide.steps().size() == 1);
  REQUIRE(guide.local_target().has_value());
  CHECK(guide.local_target()->x == 1);
  guide.Update("manor", &manor, At(2, 1));
  CHECK(guide.current_step() == 0);

  // Le portail dépose le joueur dans la pièce est : l'étape est franchie, la
  // trace part de lui jusqu'à la destination, et non plus du segment de repli.
  guide.Update("manor", &manor, At(18, 1));
  CHECK(guide.current_step() == 1);
  REQUIRE(guide.local_target().has_value());
  CHECK(guide.local_target()->x == 7);
  REQUIRE(guide.path_cells().size() == 12);
  CHECK(guide.path_cells().front().x == 18);
  CHECK(guide.path_cells().back().x == 7);
  CHECK(guide.status() == NaviGuideStatus::kGuiding);
}

TEST_CASE("Guidage : téléporté hors de portée de la trace, l'itinéraire repart d'ici",
          "[sim][navi_guide]") {
  const WalkMap manor = Walled();
  NaviGuide guide;
  guide.SetData(TwoRooms());
  guide.Request(CellInManor(7, 1));
  guide.Update("manor", &manor, At(3, 1));
  REQUIRE(guide.steps().size() == 1);

  // Une aile de mouche le pose dans la pièce est, loin de l'arrivée du portail.
  guide.Update("manor", &manor, At(12, 1));
  CHECK(guide.status() == NaviGuideStatus::kGuiding);
  CHECK(guide.steps().empty());
  REQUIRE(guide.local_target().has_value());
  CHECK(guide.local_target()->x == 7);
}

TEST_CASE("Guidage : entrer ailleurs que prévu relance l'itinéraire", "[sim][navi_guide]") {
  const WalkMap town = Open();
  const WalkMap field = Open();
  OpenField terrain;
  NaviGuide guide;
  guide.SetGoalTerrain(&terrain);
  guide.SetData(TwoMaps());
  guide.Request(CellOnField(8, 1));
  guide.Update("field", &field, At(0, 1));
  // Parti du champ, l'itinéraire n'a aucune liaison à franchir.
  REQUIRE(guide.status() == NaviGuideStatus::kGuiding);
  CHECK(guide.steps().empty());
  // Téléporté en ville : le guidage repart de là, par le portail.
  guide.Update("town", &town, At(2, 1));
  CHECK(guide.status() == NaviGuideStatus::kGuiding);
  CHECK(guide.steps().size() == 1);
}
