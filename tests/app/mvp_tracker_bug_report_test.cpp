// LE RAPPORT D'UN BUG DU CARNET DE CHASSE MVP : un JSON valable, qui décrit le
// créneau visé avec son minuteur, le tri et le groupe, et qui reste loin sous
// la borne du serveur.

#include "game/mvp_tracker_bug_report.h"

#include <array>
#include <string>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "web/json_value.h"

using moonclient::MvpTrackerBugReport;
using ro::ui::MvpMemberView;
using ro::ui::MvpSlotView;
using ro::ui::MvpTrackerView;
using ro::ui::MvpTrackerWindowState;

namespace {

// Le serveur tronque le contexte à 1024 octets ; le rapport doit en rester loin.
constexpr std::size_t kComfortableBytes = 900;

[[nodiscard]] bool Has(const moonclient::BugReportContext& report, std::string_view piece) {
  return report.json.find(piece) != std::string::npos;
}

[[nodiscard]] std::array<MvpSlotView, 2> Slots() {
  MvpSlotView baphomet{.slot_id = 12,
                       .mob_id = 1039,
                       .delay1_ms = 7200000,
                       .delay2_ms = 7800000,
                       .map = "prt_maze03",
                       .label = "Baphomet \"le cornu\"",
                       .favorite = true,
                       .window_known = true,
                       .window_from_s = 1000,
                       .window_to_s = 1600,
                       .observed = true,
                       .source_raw = 1,
                       .kill_time_s = 900,
                       .tomb_x = 150,
                       .tomb_y = 180};
  MvpSlotView osiris{.slot_id = 3, .mob_id = 1038, .map = "moc_pryd04", .label = "Osiris"};
  return {baphomet, osiris};
}

}  // namespace

TEST_CASE("Rapport du carnet MVP : le créneau visé, avec son minuteur", "[app][bugreport]") {
  const std::array slots = Slots();
  const std::array members = {MvpMemberView{.user_id = 7, .online = true, .name = "Moi"}};
  MvpTrackerView view{.server_now_s = 1200, .enabled = true, .link_ready = true};
  view.catalog_known = true;
  view.slots = slots;
  view.group_id = 44;
  view.members = members;
  MvpTrackerWindowState window;
  window.focus_slot = 12;
  window.order = {1, 0};

  const moonclient::BugReportContext report = MvpTrackerBugReport(window, view);
  REQUIRE(moonclient::web::ParseJson(report.json).has_value());
  CHECK(report.label.find("Baphomet") != std::string::npos);
  CHECK(report.label.find("12") != std::string::npos);
  CHECK(Has(report, R"("why":"focus","slot_id":12,"mob_id":1039)"));
  CHECK(Has(report, R"("window":{"known":true,"from_s":1000,"to_s":1600)"));
  CHECK(Has(report, R"("tomb":[150,180])"));
  CHECK(Has(report, R"("server_now_s":1200)"));
  CHECK(Has(report, R"("first_rows":[3,12])"));
  CHECK(Has(report, R"("group_id":44,"members":1,"online":1)"));
  CHECK(report.json.size() < kComfortableBytes);
}

TEST_CASE("Rapport du carnet MVP : sans créneau visé, le filtre et le tri", "[app][bugreport]") {
  const std::array slots = Slots();
  MvpTrackerView view{.enabled = true};
  view.slots = slots;
  MvpTrackerWindowState window;
  window.look.show_filter = true;
  window.filter = {'b', 'a', 'p', 'h'};
  window.order = {0};

  const moonclient::BugReportContext report = MvpTrackerBugReport(window, view);
  REQUIRE(moonclient::web::ParseJson(report.json).has_value());
  CHECK(report.label.find("baph") != std::string::npos);
  CHECK(Has(report, R"("filter":"baph")"));
  CHECK(Has(report, R"("sort":"none")"));
  CHECK(Has(report, R"("rows":1)"));
  CHECK_FALSE(Has(report, "\"slot\":"));
}

TEST_CASE("Rapport du carnet MVP : le mot du tri dit la colonne et le sens", "[app][bugreport]") {
  // La clé de la table : (colonne + 1) × 2, plus un en croissant.
  constexpr int kNameAscending = (2 + 1) * 2 + 1;
  constexpr int kReturnDescending = (5 + 1) * 2;
  CHECK(ro::ui::MvpSortWord(0) == "none");
  CHECK(ro::ui::MvpSortWord(kNameAscending) == "name_asc");
  CHECK(ro::ui::MvpSortWord(kReturnDescending) == "return_desc");
}
