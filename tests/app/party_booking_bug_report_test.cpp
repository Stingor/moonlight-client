// LE RAPPORT D'UN BUG DE LA RECHERCHE DE GROUPE : un JSON valable, qui décrit
// les critères tels qu'ils partiraient, la liste sans le nom des auteurs, et
// l'annonce du joueur ou celle qu'il rédige.

#include "game/party_booking_bug_report.h"

#include <array>
#include <string>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "web/json_value.h"

using moonclient::PartyBookingBugReport;
using ro::ui::PartyBookingFilter;
using ro::ui::PartyBookingPlaceView;
using ro::ui::PartyBookingRegionView;
using ro::ui::PartyBookingRowView;
using ro::ui::PartyBookingView;
using ro::ui::PartyBookingWindowState;

namespace {

// Le serveur tronque le contexte à 1024 octets ; le rapport doit en rester loin.
constexpr std::size_t kComfortableBytes = 900;
constexpr std::int16_t kGlastHeimCode = 7;
constexpr std::int16_t kPriestCode = 8;
constexpr std::int16_t kKnightCode = 7;
constexpr std::int16_t kEmpty = ro::ui::kPartyBookingEmptySlot;

[[nodiscard]] bool Has(const moonclient::BugReportContext& report, std::string_view piece) {
  return report.json.find(piece) != std::string::npos;
}

}  // namespace

TEST_CASE("Rapport de la recherche de groupe : les critères et la liste, sans les auteurs",
          "[app][bugreport]") {
  const std::array places = {
      PartyBookingPlaceView{.name = "Glast Heim", .map_code = kGlastHeimCode}};
  const std::array regions = {PartyBookingRegionView{.name = "Rune-Midgard", .places = places}};
  const std::array rows = {
      PartyBookingRowView{.leader = "Inconnu",
                          .levels = "40~55",
                          .place = "Glast Heim",
                          .jobs = "Priest, Knight",
                          .posted = "14:05"},
      PartyBookingRowView{.leader = "Autre", .levels = "1~99", .place = "Partout"},
      PartyBookingRowView{.leader = "Encore", .levels = "1~99"},
      PartyBookingRowView{.leader = "Quatrieme", .levels = "1~99"}};
  PartyBookingView view{.regions = regions, .rows = rows, .searched = true};
  view.most_level = 99;
  PartyBookingWindowState window;
  window.search_level = 50;
  window.filter = PartyBookingFilter::kPlace;
  window.search_region = 0;
  window.search_place = 0;
  window.register_jobs = {kPriestCode, kKnightCode, kEmpty, kEmpty, kEmpty, kEmpty};

  const moonclient::BugReportContext report = PartyBookingBugReport(window, view);
  REQUIRE(moonclient::web::ParseJson(report.json).has_value());
  CHECK(report.label.find("4") != std::string::npos);
  CHECK(Has(report, R"("level":50,"filter":"place","map_code":7)"));
  CHECK(Has(report, R"("rows":4)"));
  CHECK(Has(report, R"({"levels":"40~55","place":"Glast Heim","jobs":"Priest, Knight"})"));
  CHECK_FALSE(Has(report, "Inconnu"));
  CHECK_FALSE(Has(report, "Quatrieme"));
  CHECK(Has(report, R"("draft":{"level":1,"map_code":0,"jobs":[8,7])"));
  CHECK(report.json.size() < kComfortableBytes);
}

TEST_CASE("Rapport de la recherche de groupe : l'annonce du joueur et ses métiers modifiés",
          "[app][bugreport]") {
  PartyBookingView view{.has_mine = true};
  view.mine = PartyBookingRowView{.leader = "Moi", .levels = "80~95", .place = "Juperos"};
  view.mine_index = 31;
  view.mine_jobs = {kPriestCode, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty};
  PartyBookingWindowState window;
  window.edited_jobs = {kPriestCode, kKnightCode, kEmpty, kEmpty, kEmpty, kEmpty};

  const moonclient::BugReportContext report = PartyBookingBugReport(window, view);
  REQUIRE(moonclient::web::ParseJson(report.json).has_value());
  CHECK(report.label.find("Juperos") != std::string::npos);
  CHECK(Has(report, R"("mine":{"index":31,"levels":"80~95","place":"Juperos","jobs":[8],)"
                    R"("edited_jobs":[8,7]})"));
  CHECK_FALSE(Has(report, "\"draft\""));
}
