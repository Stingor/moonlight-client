// LE RAPPORT D'UN BUG DE LA FICHE : un JSON valable, qui porte toujours le
// métier et les niveaux, puis ce que l'onglet montré montrait.

#include "game/character_bug_report.h"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "web/json_value.h"

using moonclient::CharacterBugReport;
using moonclient::CharacterBugScene;
using ro::ui::CharacterTab;
using ro::ui::EquipSlot;

namespace {

// Assez de presets pour dépasser à coup sûr le plafond de la liste.
constexpr int kManyPresets = 100;
// Ce que le serveur garde du contexte.
constexpr std::size_t kServerContextBytes = 1024;

[[nodiscard]] bool Has(const moonclient::BugReportContext& report, std::string_view piece) {
  return report.json.find(piece) != std::string::npos;
}

[[nodiscard]] ro::ui::CharacterHeaderView Header() {
  return ro::ui::CharacterHeaderView{.name = "Moi",
                                     .job = "Lord \"Knight\"",
                                     .base_level = 99,
                                     .job_level = 50,
                                     .hp = 1200,
                                     .max_hp = 9000,
                                     .sp = 30,
                                     .max_sp = 400};
}

}  // namespace

TEST_CASE("Rapport de la fiche : l'équipement, ses pièces et ses nombres", "[app][bugreport]") {
  std::vector<ro::ui::EquipSlotView> slots(ro::ui::kEquipSlotCount);
  slots[static_cast<std::size_t>(EquipSlot::kWeapon)] =
      ro::ui::EquipSlotView{.client_slot = 5, .item_id = 1201, .refine_level = 7};
  ro::ui::CharacterStatsView stats;
  stats.attributes[0] = {.base = 90, .bonus = 12};
  stats.atk1 = 300;
  const ro::ui::CharacterHeaderView header = Header();
  ro::ui::CharacterWindowState window;
  window.tab = CharacterTab::kEquipment;
  window.stats_shown = true;
  const moonclient::BugReportContext report = CharacterBugReport(
      CharacterBugScene{.window = &window, .header = &header, .slots = slots, .stats = &stats});

  REQUIRE(moonclient::web::ParseJson(report.json).has_value());
  CHECK(report.label.find("Équipement") != std::string::npos);
  CHECK(Has(report, R"("tab":"equipment")"));
  CHECK(Has(report, R"("job":"Lord \"Knight\"","base_level":99,"job_level":50)"));
  CHECK(Has(report, R"("equipment":{"stats_shown":true,"slots":{"weapon":[1201,7]})"));
  CHECK(Has(report, R"("attributes":[[90,12],)"));
  CHECK(Has(report, R"("atk":[300,0])"));
  // Le nom du personnage ne part pas : le serveur sait qui écrit.
  CHECK_FALSE(Has(report, "Moi"));
}

TEST_CASE("Rapport de la fiche : le grimoire et ses réservations", "[app][bugreport]") {
  const std::array cells = {
      ro::ui::SkillCellView{.skill_id = 5, .learned = 3, .reserved = 6},
      ro::ui::SkillCellView{.skill_id = 7, .learned = 1},
  };
  const std::array groups = {ro::ui::SkillGroupView{.label = "Épéiste", .cells = cells}};
  ro::ui::CharacterWindowState window;
  window.tab = CharacterTab::kGrimoire;
  const moonclient::BugReportContext report = CharacterBugReport(
      CharacterBugScene{.window = &window, .groups = groups, .points = {.left = 4, .reserved = 3}});
  REQUIRE(moonclient::web::ParseJson(report.json).has_value());
  CHECK(Has(report, R"("grimoire":{"group":0,"group_label":"Épéiste","groups":1)"));
  CHECK(Has(report, R"("points_left":4,"points_reserved":3)"));
  CHECK(Has(report, R"("reserved":[[5,3,6]],"reserved_cut":false)"));
  CHECK_FALSE(Has(report, "job_level"));
}

TEST_CASE("Rapport de la fiche : de nombreux presets restent sous la limite du serveur",
          "[app][bugreport]") {
  std::vector<std::string> names;
  for (int rank = 0; rank < kManyPresets; ++rank) {
    names.push_back("Preset de chasse " + std::to_string(rank));
  }
  std::vector<ro::ui::PresetRowView> rows;
  for (const std::string& name : names) {
    rows.push_back(ro::ui::PresetRowView{.name = name, .hotkey = "Ctrl+F5"});
  }
  const ro::ui::PresetTabView presets{.presets = rows, .capacity = 120};
  const ro::ui::CharacterHeaderView header = Header();
  ro::ui::CharacterWindowState window;
  window.tab = CharacterTab::kPresets;
  const moonclient::BugReportContext report = CharacterBugReport(
      CharacterBugScene{.window = &window, .header = &header, .presets = &presets});
  REQUIRE(moonclient::web::ParseJson(report.json).has_value());
  CHECK(Has(report, R"("presets":{"capturing":-1,"refusal":"","status":"","capacity":120)"));
  CHECK(Has(report, R"("presets_cut":true)"));
  CHECK(report.json.size() < kServerContextBytes);
}

TEST_CASE("Rapport de la fiche : l'onglet du mercenaire", "[app][bugreport]") {
  ro::ui::CompanionTabsView companions;
  companions.mercenary.present = true;
  companions.mercenary.kind = "Archer";
  companions.mercenary.level = 40;
  companions.mercenary.contract_left_s = 1800;
  ro::ui::CharacterWindowState window;
  window.tab = CharacterTab::kMercenary;
  window.mercenary.status = "Le contrat s'achève";
  const moonclient::BugReportContext report =
      CharacterBugReport(CharacterBugScene{.window = &window, .companions = &companions});
  REQUIRE(moonclient::web::ParseJson(report.json).has_value());
  CHECK(Has(report, R"("mercenary":{"status":"Le contrat s'achève","kind":"Archer","level":40)"));
  CHECK(Has(report, R"("contract_left_s":1800)"));
}
