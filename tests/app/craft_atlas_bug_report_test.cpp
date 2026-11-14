// LE RAPPORT D'UN BUG DE L'ATLAS DES RECETTES : un JSON valable, qui cite la
// fiche de l'item choisi et ce qu'il manque de chaque ingrédient, borné quoi
// que la recette exige.

#include "game/craft_atlas_bug_report.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "web/json_value.h"

using moonclient::CraftAtlasBugReport;
using ro::ui::CraftAtlasRecipeView;
using ro::ui::CraftAtlasRow;
using ro::ui::CraftAtlasTab;
using ro::ui::CraftAtlasView;
using ro::ui::CraftAtlasWindowState;
using ro::ui::CraftAtlasYieldMode;

namespace {

constexpr std::size_t kComfortableBytes = 900;

constexpr std::uint32_t kSteelId = 999;
constexpr std::uint32_t kIronId = 998;
constexpr std::uint32_t kCoalId = 1003;
constexpr std::uint32_t kAnvilId = 986;

[[nodiscard]] bool Has(const moonclient::BugReportContext& report, std::string_view piece) {
  return report.json.find(piece) != std::string::npos;
}

}  // namespace

TEST_CASE("Rapport de l'Atlas : la fiche choisie et ce qui manque", "[app][bugreport]") {
  const std::array ingredients = {
      CraftAtlasRow{.item_id = kIronId, .name = "Iron", .owned = 2, .quantity = 5},
      CraftAtlasRow{.item_id = kCoalId, .name = "Coal", .owned = 1, .quantity = 1},
      CraftAtlasRow{.item_id = kAnvilId, .name = "Anvil", .owned = 0, .quantity = 1, .kept = true},
  };
  CraftAtlasView view;
  view.available = true;
  view.sheet.item = CraftAtlasRow{.item_id = kSteelId, .name = "Steel", .owned = 3};
  view.sheet.recipe = CraftAtlasRecipeView{.skill_id = 94,
                                           .skill_label = "Steel",
                                           .skill_level = 1,
                                           .yield_mode = CraftAtlasYieldMode::kBonusRoll,
                                           .yield_bonus_permille = 125,
                                           .craftable = 0,
                                           .ingredients = ingredients};
  CraftAtlasWindowState window;
  window.selected_id = kSteelId;
  window.tab = CraftAtlasTab::kMaterials;

  const moonclient::BugReportContext report = CraftAtlasBugReport(window, view);
  REQUIRE(moonclient::web::ParseJson(report.json).has_value());
  CHECK(Has(report, R"("tab":"materials")"));
  CHECK(Has(report, R"("selected_id":999)"));
  CHECK(Has(report, R"("sheet":{"item_id":999,"name":"Steel","owned":3,)"));
  CHECK(Has(report, R"("mode":"bonus_roll","least":1,"most":1,"bonus_permille":125})"));
  CHECK(Has(report, R"({"item_id":998,"need":5,"owned":2,"missing":3})"));
  CHECK(Has(report, R"({"item_id":986,"need":1,"owned":0,"missing":1,"kept":true})"));
  CHECK(Has(report, R"("more":0)"));
  CHECK_FALSE(Has(report, R"("staff_crafts")"));
  CHECK(report.label.find("Steel (#999)") != std::string::npos);
}

TEST_CASE("Rapport de l'Atlas : sans fiche, la recherche", "[app][bugreport]") {
  CraftAtlasView view;
  view.available = true;
  CraftAtlasWindowState window;
  window.filter = {'j', 'e', 'l', 'l'};

  const moonclient::BugReportContext report = CraftAtlasBugReport(window, view);
  REQUIRE(moonclient::web::ParseJson(report.json).has_value());
  CHECK(Has(report, R"("search":"jell")"));
  CHECK(Has(report, R"("free_bag_slots":null)"));
  CHECK_FALSE(Has(report, R"("sheet")"));
  CHECK(report.label.find("jell") != std::string::npos);
}

TEST_CASE("Rapport de l'Atlas : une longue recette reste bornée", "[app][bugreport]") {
  constexpr std::size_t kManyIngredients = 12;
  const std::string long_name(256, 'x');
  std::array<CraftAtlasRow, kManyIngredients> ingredients{};
  for (std::size_t index = 0; index < ingredients.size(); ++index) {
    ingredients[index] = CraftAtlasRow{.item_id = 100000U + static_cast<std::uint32_t>(index),
                                       .name = long_name,
                                       .owned = 0,
                                       .quantity = 9999};
  }
  CraftAtlasView view;
  view.available = true;
  view.staff_items = true;
  view.free_bag_slots = 100;
  view.sheet.item = CraftAtlasRow{.item_id = kSteelId, .name = long_name};
  view.sheet.recipe = CraftAtlasRecipeView{.skill_id = 2495,
                                           .skill_label = long_name,
                                           .yield_mode = CraftAtlasYieldMode::kBonusRoll,
                                           .ingredients = ingredients};
  CraftAtlasWindowState window;
  std::fill(window.filter.begin(), window.filter.end() - 1, '"');

  const moonclient::BugReportContext report = CraftAtlasBugReport(window, view);
  REQUIRE(moonclient::web::ParseJson(report.json).has_value());
  CHECK(report.json.size() < kComfortableBytes);
  CHECK(Has(report, R"("more":8)"));
  CHECK(Has(report, R"("staff_crafts":1)"));
}
