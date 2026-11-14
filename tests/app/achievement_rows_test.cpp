// LES RÈGLES DE LA FENÊTRE DES SUCCÈS : ce qu'un onglet montre et dans quel
// ordre, ce qu'il compte, et comment objectifs et lignes de suivi s'écrivent.

#include "game/achievement_rows.h"

#include <array>
#include <string_view>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "ro/vfs/achievement_database.h"

using moonclient::RowsOfTab;
using ro::sim::AchievementBook;
using ro::sim::AchievementRecord;
using ro::ui::AchievementFilter;
using ro::vfs::AchievementCatalog;

namespace {

constexpr std::string_view kTable = R"(
version: 1
achievements:
  100:
    {name: "A", score: 10, targets: [], reward: {item: 501, title: 0, script: false},
     dependents: [],
     window: {summary: s, details: d, gauge: bullets, group: EAT, major: 1, minor: 0,
              objectives: [{text: "Bring the food"}]}}
  101:
    {name: "B", score: 10, targets: [[0, 10]], reward: {item: 0, title: 0, script: false},
     dependents: [],
     window: {summary: s, details: d, gauge: counts, group: BATTLE, major: 1, minor: 1,
              objectives: [{text: "Eliminate the very long named Ferlock monsters"}]}}
  102:
    {name: "C", score: 20, targets: [[0, 3]], reward: {item: 0, title: 1001, script: false},
     dependents: [],
     window: {summary: s, details: d, gauge: counts, group: BATTLE, major: 1, minor: 1,
              objectives: [{text: "Hunt"}]}}
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

[[nodiscard]] AchievementRecord Done(std::uint32_t achievement_id, std::uint32_t at,
                                     bool rewarded) {
  AchievementRecord record{.achievement_id = achievement_id};
  record.progress.completed = true;
  record.progress.completed_at = at;
  record.progress.rewarded = rewarded;
  return record;
}

[[nodiscard]] AchievementBook Book() {
  AchievementBook book;
  AchievementRecord started{.achievement_id = 101};
  started.progress.counts[0] = 4;
  const std::array records = {Done(102, 2000, false), Done(100, 1000, true), started};
  book.ApplyList({}, records);
  return book;
}

[[nodiscard]] std::vector<std::uint32_t> Ids(
    const std::vector<const ro::vfs::AchievementInfo*>& rows) {
  std::vector<std::uint32_t> ids;
  for (const auto* info : rows) {
    ids.push_back(info->achievement_id);
  }
  return ids;
}

}  // namespace

TEST_CASE("Succès : un onglet montre ses achevés du plus ancien au plus récent, puis le reste",
          "[app][succes]") {
  const AchievementCatalog catalog = Catalog();
  const AchievementBook book = Book();
  CHECK(Ids(RowsOfTab(catalog, book, 2, -1, AchievementFilter::kAll)) ==
        std::vector<std::uint32_t>{100, 102, 101});
  CHECK(Ids(RowsOfTab(catalog, book, 2, 1, AchievementFilter::kAll)) ==
        std::vector<std::uint32_t>{102, 101});
}

TEST_CASE("Succès : « Tous » liste le catalogue entier, catégories et sous-catégories ignorées",
          "[app][succes]") {
  const AchievementCatalog catalog = Catalog();
  const AchievementBook book = Book();
  // Le sous-onglet demandé ne compte pas plus que la catégorie.
  CHECK(Ids(RowsOfTab(catalog, book, ro::ui::kAchievementAllTab, 1, AchievementFilter::kAll)) ==
        std::vector<std::uint32_t>{100, 102, 101, 200});
  const auto all = moonclient::TallyOfTab(catalog, book, ro::ui::kAchievementAllTab,
                                          ro::ui::kAchievementWholeTab);
  CHECK(all.done == 2);
  CHECK(all.total == 4);
}

TEST_CASE("Succès : le filtre « à réclamer » ne garde des achevés que ceux qui attendent",
          "[app][succes]") {
  const AchievementCatalog catalog = Catalog();
  const AchievementBook book = Book();
  CHECK(Ids(RowsOfTab(catalog, book, 2, -1, AchievementFilter::kUnclaimed)) ==
        std::vector<std::uint32_t>{102, 101});
}

TEST_CASE("Succès : les filtres « en cours » et « suivis »", "[app][succes]") {
  const AchievementCatalog catalog = Catalog();
  AchievementBook book = Book();
  using moonclient::AchievementQuery;
  CHECK(Ids(RowsOfTab(catalog, book,
                      AchievementQuery{.tab = 0, .filter = AchievementFilter::kInProgress})) ==
        std::vector<std::uint32_t>{101});
  static_cast<void>(book.ToggleTracking(200));
  static_cast<void>(book.ToggleTracking(100));
  CHECK(Ids(RowsOfTab(catalog, book,
                      AchievementQuery{.tab = 0, .filter = AchievementFilter::kTracked})) ==
        std::vector<std::uint32_t>{100, 200});
}

TEST_CASE("Succès : les tris à nous reprennent l'ordre du jeu pour les égaux", "[app][succes]") {
  const AchievementCatalog catalog = Catalog();
  const AchievementBook book = Book();
  using moonclient::AchievementQuery;
  using ro::ui::AchievementSort;
  CHECK(
      Ids(RowsOfTab(catalog, book, AchievementQuery{.tab = 0, .sort = AchievementSort::kScore})) ==
      std::vector<std::uint32_t>{102, 100, 101, 200});
  CHECK(Ids(RowsOfTab(catalog, book,
                      AchievementQuery{.tab = 0, .sort = AchievementSort::kProgress})) ==
        std::vector<std::uint32_t>{100, 102, 101, 200});
  CHECK(Ids(RowsOfTab(catalog, book, AchievementQuery{.tab = 0, .sort = AchievementSort::kName})) ==
        std::vector<std::uint32_t>{100, 101, 102, 200});
}

TEST_CASE("Succès : la recherche traverse les onglets, sans égard à la casse", "[app][succes]") {
  const AchievementCatalog catalog = Catalog();
  const AchievementBook book = Book();
  using moonclient::AchievementQuery;
  // « ferlock » n'est que dans un objectif ; l'onglet demandé ne compte plus.
  CHECK(Ids(RowsOfTab(catalog, book, AchievementQuery{.tab = 2, .search = "FERLOCK"})) ==
        std::vector<std::uint32_t>{101});
  CHECK(RowsOfTab(catalog, book, AchievementQuery{.tab = 1, .search = "absent"}).empty());
  CHECK(moonclient::AchievementProgressShare(*catalog.Find(101), book.Find(101)) == 0.4);
}

TEST_CASE("Succès : les récompenses à réclamer, tous onglets confondus", "[app][succes]") {
  const AchievementCatalog catalog = Catalog();
  const AchievementBook book = Book();
  // 100 est déjà réclamé ; 102 attend.
  CHECK(moonclient::ClaimableAchievements(catalog, book) == std::vector<std::uint32_t>{102});
}

TEST_CASE("Succès : les comptes d'un onglet et du résumé", "[app][succes]") {
  const AchievementCatalog catalog = Catalog();
  const AchievementBook book = Book();
  const auto whole = moonclient::TallyOfTab(catalog, book, 0, -1);
  CHECK(whole.done == 2);
  CHECK(whole.total == 4);
  const auto battle = moonclient::TallyOfTab(catalog, book, 2, 1);
  CHECK(battle.done == 1);
  CHECK(battle.total == 2);
}

TEST_CASE("Succès : les deux derniers obtenus, le plus récent d'abord", "[app][succes]") {
  const AchievementCatalog catalog = Catalog();
  const AchievementBook book = Book();
  CHECK(Ids(moonclient::MostRecentlyCompleted(catalog, book)) ==
        std::vector<std::uint32_t>{102, 100});
}

TEST_CASE("Succès : un objectif compté s'écrit avec son compte, et change de couleur",
          "[app][succes]") {
  const AchievementCatalog catalog = Catalog();
  const AchievementBook book = Book();
  const auto* counted = catalog.Find(101);
  REQUIRE(counted != nullptr);
  CHECK(moonclient::ObjectiveText(*counted, 0, book.Find(101)) ==
        "Eliminate the very long named Ferlock monsters ( 4 / 10 )");
  CHECK(moonclient::ObjectiveStateOf(*counted, 0, book.Find(101)) ==
        ro::ui::AchievementObjectiveState::kStarted);
  CHECK(moonclient::ObjectiveStateOf(*counted, 0, nullptr) ==
        ro::ui::AchievementObjectiveState::kUntouched);
  const auto* bullets = catalog.Find(100);
  REQUIRE(bullets != nullptr);
  CHECK(moonclient::ObjectiveText(*bullets, 0, nullptr) == "Bring the food");
}

TEST_CASE("Succès : la ligne de suivi tient en quarante octets", "[app][succes]") {
  const AchievementCatalog catalog = Catalog();
  const AchievementBook book = Book();
  CHECK(moonclient::TrackerLineText(*catalog.Find(102), 0, book.Find(102)) == "- Hunt (0/3)");
  CHECK(moonclient::TrackerLineText(*catalog.Find(101), 0, book.Find(101)) ==
        "- Eliminate the very long named Ferl...(4/10)");
  CHECK(moonclient::TrackerLineText(*catalog.Find(100), 0, nullptr) == "- Bring the food");
}

TEST_CASE("Succès : la date s'écrit sur deux colonnes complétées d'espaces", "[app][succes]") {
  CHECK(moonclient::AchievementDateText(0).empty());
  const std::string date = moonclient::AchievementDateText(1585008079);
  // L'heure locale décide du jour ; la forme, elle, ne bouge pas.
  REQUIRE(date.size() == std::string_view("2020. 3.24").size());
  CHECK(date.substr(0, 5) == "2020.");
}
