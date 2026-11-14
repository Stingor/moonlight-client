// LE RAPPORT D'UN BUG DE NAVIGATION : un JSON valable, qui dit où était le
// joueur, ce qu'il avait choisi, et l'itinéraire liaison par liaison.

#include "world/navi_bug_report.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/bug_report.h"
#include "ro/sim/navi_guide.h"
#include "ro/sim/walk_map.h"

#include "web/json_value.h"

using moonclient::NaviBugScene;
using moonclient::NavigationBugReport;
using ro::formats::NaviData;

namespace {

constexpr std::uint16_t kTown = 0;
constexpr std::uint16_t kField = 1;

std::shared_ptr<NaviData> TwoMaps() {
  auto data = std::make_shared<NaviData>();
  data->maps.push_back(ro::formats::NaviMap{"town", "Ville \"close\"", 5001, 100, 100});
  data->maps.push_back(ro::formats::NaviMap{"field", "Champ", 5001, 100, 100});
  ro::formats::NaviLink link;
  link.map = kTown;
  link.link_id = 1000;
  link.kind = ro::formats::navi_link_kind::kWarpPortal;
  link.cell_x = 90;
  link.cell_y = 50;
  link.dest_map = kField;
  link.dest_cell_x = 10;
  link.dest_cell_y = 50;
  data->links.push_back(link);
  ro::formats::NaviNpc npc;
  npc.map = kField;
  npc.npc_id = 2000;
  npc.sprite_class = 61;
  npc.name = "Jean-Mich";
  npc.cell_x = 46;
  npc.cell_y = 55;
  data->npcs.push_back(npc);
  data->BuildIndexes();
  return data;
}

// La grille du champ, toute praticable, que l'application lirait dans l'archive.
class OpenField final : public ro::sim::NaviGoalTerrain {
 public:
  const ro::sim::WalkMap* GridOf(std::string_view map_name) override {
    return map_name == "field" ? &field_ : nullptr;
  }

 private:
  static constexpr std::int32_t kSide = 100;
  ro::sim::WalkMap field_{kSide, kSide,
                          std::vector<std::uint8_t>(static_cast<std::size_t>(kSide * kSide), 1)};
};

}  // namespace

TEST_CASE("Rapport de navigation : le choix, le guidage et l'itinéraire", "[app][bugreport]") {
  const std::shared_ptr<NaviData> data = TwoMaps();
  OpenField terrain;
  ro::sim::NaviGuide guide;
  guide.SetGoalTerrain(&terrain);
  guide.SetData(data);
  ro::sim::NaviGuideRequest request;
  request.kind = ro::sim::NaviGoalKind::kNpc;
  request.npc_id = 2000;
  guide.Request(request);
  guide.Update("town", nullptr, ro::sim::CellPosition{80, 50, 0});
  REQUIRE(guide.status() == ro::sim::NaviGuideStatus::kGuiding);

  const moonclient::BugReportContext report = NavigationBugReport(
      NaviBugScene{.navi = data.get(),
                   .guide = &guide,
                   .here_map = "town",
                   .player = ro::sim::CellPosition{80, 50, 0},
                   .term = "jean \"mich\"",
                   .selected = ro::sim::NaviHit{.kind = ro::sim::NaviHitKind::kNpc, .rank = 0}});

  REQUIRE(moonclient::web::ParseJson(report.json).has_value());
  CHECK(report.category == static_cast<std::uint8_t>(ro::net::BugReportCategory::kNavigation));
  CHECK(report.label.find("Jean-Mich (field 46,55)") != std::string::npos);
  CHECK(report.json.find(R"("npc_id":2000)") != std::string::npos);
  CHECK(report.json.find(R"("status":"guiding")") != std::string::npos);
  CHECK(report.json.find(R"({"link_id":1000,)") != std::string::npos);
  CHECK(report.json.find(R"("here":{"map":"town","x":80,"y":50})") != std::string::npos);
}

TEST_CASE("Rapport de navigation : sans tables ni choix, il part quand même", "[app][bugreport]") {
  const moonclient::BugReportContext report =
      NavigationBugReport(NaviBugScene{.loading = true, .here_map = "prontera"});
  REQUIRE(moonclient::web::ParseJson(report.json).has_value());
  CHECK(report.json.find(R"("loading":true)") != std::string::npos);
  CHECK(report.json.find("\"guide\"") == std::string::npos);
}
