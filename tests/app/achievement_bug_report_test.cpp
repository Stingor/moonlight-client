// LE RAPPORT D'UN BUG DE SUCCÈS : un JSON valable, qui décrit le succès choisi
// quand il y en a un, sinon la liste ou le résumé que le joueur regardait.

#include "game/achievement_bug_report.h"

#include <array>
#include <string>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "ro/vfs/achievement_database.h"

#include "web/json_value.h"

using moonclient::AchievementBugReport;
using moonclient::AchievementBugScene;
using ro::sim::AchievementBook;
using ro::sim::AchievementRecord;
using ro::vfs::AchievementCatalog;

namespace {

constexpr std::string_view kTable = R"(
version: 1
achievements:
  900001:
    {name: "Premier \"sacrifice\"", score: 10, targets: [[0, 5]],
     reward: {item: 12246, title: 0, script: false}, dependents: [],
     window: {summary: s, details: d, gauge: counts, group: GET_ITEM, major: 6, minor: 0,
              objectives: [{text: "Pochettes ouvertes"}]}}
  200:
    {name: "D", score: 5, targets: [], reward: {item: 0, title: 0, script: false},
     dependents: [],
     window: {summary: s, details: d, gauge: bullets, group: SEE, major: 2, minor: 0,
              objectives: []}}
)";

[[nodiscard]] AchievementCatalog Catalog() {
  auto database = ro::vfs::AchievementDatabase::Parse(kTable);
  REQUIRE(database.has_value());
  return AchievementCatalog::Of(*database);
}

[[nodiscard]] AchievementBook Book() {
  AchievementBook book;
  AchievementRecord started{.achievement_id = 900001};
  started.progress.counts[0] = 3;
  const std::array records = {started};
  book.ApplyList(ro::sim::AchievementStanding{.total_points = 340, .rank = 15}, records);
  static_cast<void>(book.ToggleTracking(900001));
  return book;
}

[[nodiscard]] bool Has(const moonclient::BugReportContext& report, std::string_view piece) {
  return report.json.find(piece) != std::string::npos;
}

}  // namespace

TEST_CASE("Rapport de succès : le succès choisi, avec sa progression", "[app][bugreport]") {
  const AchievementCatalog catalog = Catalog();
  const AchievementBook book = Book();
  ro::ui::AchievementWindowState window;
  window.tab = ro::ui::AchievementTabOfMajor(6);
  window.selected = 900001;
  const moonclient::BugReportContext report = AchievementBugReport(AchievementBugScene{
      .catalog = &catalog, .book = &book, .window = &window, .list_mode = true, .rows = 1});

  REQUIRE(moonclient::web::ParseJson(report.json).has_value());
  CHECK(report.label.find("Premier \"sacrifice\" (#900001)") != std::string::npos);
  CHECK(Has(report, R"("achievement_id":900001)"));
  CHECK(Has(report, R"("count":3,"target":5)"));
  CHECK(Has(report, R"("reward":{"item":12246,"title":null,"script":false,"buff":null})"));
  CHECK(Has(report, R"("known":true,"completed":false)"));
  CHECK(Has(report, R"("tracked":true)"));
  CHECK(Has(report, R"("total_points":340,"rank":15)"));
}

TEST_CASE("Rapport de succès : un succès que le serveur n'a pas cité se dit inconnu",
          "[app][bugreport]") {
  const AchievementCatalog catalog = Catalog();
  const AchievementBook book = Book();
  ro::ui::AchievementWindowState window;
  window.tab = ro::ui::AchievementTabOfMajor(2);
  window.selected = 200;
  const moonclient::BugReportContext report = AchievementBugReport(AchievementBugScene{
      .catalog = &catalog, .book = &book, .window = &window, .list_mode = true, .rows = 1});
  REQUIRE(moonclient::web::ParseJson(report.json).has_value());
  CHECK(Has(report, R"("progress":{"known":false})"));
  CHECK(Has(report, R"("objectives":[])"));
}

TEST_CASE("Rapport de succès : sans choix, la recherche ou le résumé", "[app][bugreport]") {
  const AchievementCatalog catalog = Catalog();
  const AchievementBook book = Book();
  ro::ui::AchievementWindowState window;
  window.search = {'p', 'o', 'c', 'h'};
  const moonclient::BugReportContext searched = AchievementBugReport(AchievementBugScene{
      .catalog = &catalog, .book = &book, .window = &window, .list_mode = true, .rows = 1});
  REQUIRE(moonclient::web::ParseJson(searched.json).has_value());
  CHECK(searched.label.find("poch") != std::string::npos);
  CHECK(Has(searched, R"("view":"list")"));
  CHECK_FALSE(Has(searched, "\"achievement\""));

  window.search = {};
  const moonclient::BugReportContext summary = AchievementBugReport(
      AchievementBugScene{.catalog = &catalog, .book = &book, .window = &window});
  REQUIRE(moonclient::web::ParseJson(summary.json).has_value());
  CHECK(Has(summary, R"("view":"summary")"));
  CHECK(Has(summary, R"("overall":{"done":0,"total":2})"));
}
