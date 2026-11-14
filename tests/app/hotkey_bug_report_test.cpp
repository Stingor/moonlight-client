// LE RAPPORT D'UN BUG DE RACCOURCIS : un JSON valable, qui dit l'onglet, la
// capture en cours, la touche reprise, et les lignes que la fenêtre montrait.

#include "game/hotkey_bug_report.h"

#include <array>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "web/json_value.h"

using moonclient::HotkeyBugReport;
using moonclient::HotkeyBugScene;
using ro::ui::HotkeyRowView;

namespace {

// Assez de lignes pour dépasser à coup sûr le plafond de la liste.
constexpr int kManyRows = 200;
// Ce que le serveur garde du contexte.
constexpr std::size_t kServerContextBytes = 1024;

[[nodiscard]] bool Has(const moonclient::BugReportContext& report, std::string_view piece) {
  return report.json.find(piece) != std::string::npos;
}

}  // namespace

TEST_CASE("Rapport de raccourcis : l'onglet, la capture et les lignes montrées",
          "[app][bugreport]") {
  const std::array rows = {
      HotkeyRowView{.action = "Inventaire", .key = "Alt+E", .tab = 1},
      HotkeyRowView{.action = "Équipement", .key = "Alt+Q", .tab = 1},
      HotkeyRowView{.action = "Banc d'effets", .key = "F9", .tab = 1, .hidden = true},
      HotkeyRowView{.action = "Barre 1, case 1", .key = "F1", .group = "Barre 1", .tab = 0},
  };
  const std::array<std::string, 4> keys = {"inventory", "equipment", "effect_bench", "bar1_slot1"};
  const std::array<std::string_view, 2> tabs = {"Barres", "Interface"};
  ro::ui::HotkeyWindowState window;
  window.tab = 1;
  window.capturing = 1;
  std::snprintf(window.robbed, sizeof(window.robbed), "%s", "Alt+E a été reprise à « Carte »");
  const moonclient::BugReportContext report = HotkeyBugReport(
      HotkeyBugScene{.window = &window, .view = {.rows = rows, .tabs = tabs}, .action_keys = keys});

  REQUIRE(moonclient::web::ParseJson(report.json).has_value());
  CHECK(report.label.find("Équipement") != std::string::npos);
  CHECK(Has(report, R"("tab":1,"tab_title":"Interface","filter":"","rows":4)"));
  CHECK(Has(report, R"("capturing":{"action":"equipment","key":"Alt+Q"})"));
  CHECK(Has(report, R"("robbed":"Alt+E a été reprise à « Carte »")"));
  // La ligne cachée et celle d'un autre onglet ne se montraient pas.
  CHECK(Has(report, R"("shown":[["inventory","Alt+E"],["equipment","Alt+Q"]],"shown_total":2,)"
                    R"("shown_cut":false)"));
}

TEST_CASE("Rapport de raccourcis : la recherche filtre comme la fenêtre", "[app][bugreport]") {
  const std::array rows = {
      HotkeyRowView{.action = "Inventaire", .key = "Alt+E", .tab = 1},
      HotkeyRowView{.action = "Carte", .key = "Ctrl+Tab", .tab = 1},
  };
  const std::array<std::string_view, 2> tabs = {"Barres", "Interface"};
  ro::ui::HotkeyWindowState window;
  std::snprintf(window.filter, sizeof(window.filter), "%s", "ctrl");
  // Sans clés stables, les lignes se nomment par leur libellé.
  const moonclient::BugReportContext report =
      HotkeyBugReport(HotkeyBugScene{.window = &window, .view = {.rows = rows, .tabs = tabs}});
  REQUIRE(moonclient::web::ParseJson(report.json).has_value());
  CHECK(Has(report, R"("tab":-1,"tab_title":"all","filter":"ctrl")"));
  CHECK(Has(report, R"("capturing":null)"));
  CHECK(Has(report, R"("shown":[["Carte","Ctrl+Tab"]],"shown_total":1)"));
  CHECK_FALSE(Has(report, "robbed"));
}

TEST_CASE("Rapport de raccourcis : une longue liste est coupée sous la limite du serveur",
          "[app][bugreport]") {
  std::vector<std::string> names;
  for (int rank = 0; rank < kManyRows; ++rank) {
    names.push_back("Barre 1, case " + std::to_string(rank));
  }
  std::vector<HotkeyRowView> rows;
  for (const std::string& name : names) {
    rows.push_back(HotkeyRowView{.action = name, .key = "Ctrl+Alt+Maj+F12"});
  }
  ro::ui::HotkeyWindowState window;
  const moonclient::BugReportContext report =
      HotkeyBugReport(HotkeyBugScene{.window = &window, .view = {.rows = rows}});
  REQUIRE(moonclient::web::ParseJson(report.json).has_value());
  CHECK(Has(report, R"("shown_total":200,"shown_cut":true)"));
  CHECK(report.json.size() < kServerContextBytes);
}
