// LE RAPPORT D'UN BUG D'OPTIONS : un JSON valable, qui nomme la page et
// l'onglet montrés et porte la valeur de chacun de leurs réglages.

#include "game/options_bug_report.h"

#include <array>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "web/json_value.h"

using moonclient::OptionsBugReport;
using moonclient::OptionsBugScene;
using ro::ui::OptionKind;
using ro::ui::OptionRowView;
using ro::ui::OptionSectionView;

namespace {

// Assez de rangées pour dépasser à coup sûr le plafond de la liste.
constexpr int kManyRows = 200;
// Ce que le serveur garde du contexte.
constexpr std::size_t kServerContextBytes = 1024;
// Une couleur quelconque, transparence comprise.
constexpr ImU32 kSomeColor = 0xFF00FF80U;

[[nodiscard]] bool Has(const moonclient::BugReportContext& report, std::string_view piece) {
  return report.json.find(piece) != std::string::npos;
}

}  // namespace

TEST_CASE("Rapport d'options : la page, l'onglet et les valeurs", "[app][bugreport]") {
  const std::array shared = {OptionRowView{.id = 7, .kind = OptionKind::kToggle, .on = true}};
  const std::array rows = {
      OptionRowView{.id = 12, .kind = OptionKind::kSlider, .value = 0.5F},
      OptionRowView{.id = 13, .kind = OptionKind::kChoice, .chosen = 2},
      OptionRowView{.id = 14, .kind = OptionKind::kAction},
      OptionRowView{.id = 15, .kind = OptionKind::kText, .text = "C:\\Users\\moi"},
      OptionRowView{.id = 16, .kind = OptionKind::kColor, .color = kSomeColor},
  };
  const std::array<const char*, 2> tabs = {"Barre 1", "Barre \"2\""};
  const std::array sections = {
      OptionSectionView{.title = "Son"},
      OptionSectionView{.title = "Barres", .rows = rows, .shared_rows = shared, .tabs = tabs},
  };
  ro::ui::OptionsWindowState window;
  window.section = 1;
  window.section_tab = 1;
  const moonclient::BugReportContext report =
      OptionsBugReport(OptionsBugScene{.window = &window, .sections = sections});

  REQUIRE(moonclient::web::ParseJson(report.json).has_value());
  CHECK(report.label.find("Barres") != std::string::npos);
  CHECK(report.label.find("Barre \"2\"") != std::string::npos);
  CHECK(Has(report, R"("section":1,"section_title":"Barres")"));
  CHECK(Has(report, R"("tab":1,"tab_title":"Barre \"2\"","tabs":2)"));
  // Les communs en tête ; ni le bouton ni le champ tapé.
  CHECK(Has(report, R"("values":[[7,1],[12,0.5],[13,2],[16,"FF00FF80"]],"values_cut":false)"));
  CHECK_FALSE(Has(report, "Users"));
  CHECK_FALSE(Has(report, "revert_seconds"));
}

TEST_CASE("Rapport d'options : une page sans onglet, et le filet d'affichage", "[app][bugreport]") {
  const std::array sections = {OptionSectionView{.title = "Affichage"}};
  ro::ui::OptionsWindowState window;
  window.revert_seconds = 9;
  const moonclient::BugReportContext report =
      OptionsBugReport(OptionsBugScene{.window = &window, .sections = sections});
  REQUIRE(moonclient::web::ParseJson(report.json).has_value());
  CHECK_FALSE(Has(report, "tab_title"));
  CHECK(Has(report, R"("revert_seconds":9)"));
}

TEST_CASE("Rapport d'options : une longue page est coupée sous la limite du serveur",
          "[app][bugreport]") {
  std::vector<OptionRowView> rows;
  for (int id = 0; id < kManyRows; ++id) {
    rows.push_back(OptionRowView{.id = id, .kind = OptionKind::kSlider, .value = 123.25F});
  }
  const std::array sections = {OptionSectionView{.title = "Entités", .rows = rows}};
  ro::ui::OptionsWindowState window;
  const moonclient::BugReportContext report =
      OptionsBugReport(OptionsBugScene{.window = &window, .sections = sections});
  REQUIRE(moonclient::web::ParseJson(report.json).has_value());
  CHECK(Has(report, R"("values_cut":true)"));
  CHECK(report.json.size() < kServerContextBytes);
}

TEST_CASE("Rapport d'options : une page hors de la liste se dit par son rang", "[app][bugreport]") {
  ro::ui::OptionsWindowState window;
  window.section = 4;
  const moonclient::BugReportContext report =
      OptionsBugReport(OptionsBugScene{.window = &window, .sections = {}});
  REQUIRE(moonclient::web::ParseJson(report.json).has_value());
  CHECK(Has(report, R"("options":{"section":4})"));
}
