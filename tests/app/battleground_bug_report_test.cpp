// LE RAPPORT D'UN BUG DE CHAMP DE BATAILLE : un JSON valable, qui dit la file,
// le terrain choisi sous ses deux noms et, en bataille, le camp et le score.

#include "game/battleground_bug_report.h"

#include <string>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "web/json_value.h"

using moonclient::BattlegroundBugReport;
using moonclient::BattlegroundBugScene;
using moonclient::BattlegroundField;
using moonclient::BugReportContext;

namespace {

constexpr std::uint32_t kAppliedMs = 1000;
constexpr std::uint32_t kNowMs = 5000;
constexpr std::uint8_t kAccepted = 1;
constexpr std::uint32_t kOwnId = 2000001;
constexpr std::uint32_t kEnemyId = 2000002;
constexpr std::uint16_t kOwnCamp = 3;
constexpr std::uint16_t kEnemyCamp = 4;

[[nodiscard]] bool Has(const BugReportContext& report, std::string_view piece) {
  return report.json.find(piece) != std::string::npos;
}

[[nodiscard]] BattlegroundField Field() {
  BattlegroundField field;
  field.server_name = "Flavius";
  field.name.by_language = {{"fr", "Flavius \"cristal\""}};
  field.team_min = 3;
  field.team_max = 3;
  field.solo_apply = true;
  field.level_min = 80;
  return field;
}

}  // namespace

TEST_CASE("Rapport de champ : la file et le terrain choisi, sous ses deux noms",
          "[app][bugreport]") {
  ro::sim::EntryQueue queue;
  queue.NoteApplied("Flavius", ro::sim::EntryQueueApplyKind::kSolo, kAppliedMs);
  queue.TellApplyResult(kAccepted, "Flavius", kAppliedMs);
  const BattlegroundField field = Field();
  const BugReportContext report = BattlegroundBugReport(BattlegroundBugScene{.queue = &queue,
                                                                             .field = &field,
                                                                             .table_loaded = true,
                                                                             .table_fields = 3,
                                                                             .map_name = "prontera",
                                                                             .link_ready = true,
                                                                             .now_ms = kNowMs});

  REQUIRE(moonclient::web::ParseJson(report.json).has_value());
  CHECK(report.label.find("Flavius") != std::string::npos);
  CHECK(Has(report, R"("phase":"queued","field":"Flavius")"));
  CHECK(Has(report, R"("server_name":"Flavius","display_name":"Flavius \"cristal\"")"));
  CHECK(Has(report, R"("team":{"min":3,"max":3})"));
  CHECK(Has(report, R"("level":{"min":80,"max":0})"));
  CHECK(Has(report, R"("table":{"loaded":true,"fields":3})"));
  // Hors bataille, rien du combat.
  CHECK_FALSE(Has(report, R"("battle":)"));
}

TEST_CASE("Rapport de champ : en bataille, le camp, le côté et le score", "[app][bugreport]") {
  ro::sim::EntryQueue queue;
  ro::sim::Battlefield battlefield;
  battlefield.TellCamp(kOwnId, kOwnCamp, kOwnId);
  battlefield.TellCamp(kEnemyId, kEnemyCamp, kOwnId);
  battlefield.TellScore(2, 5);
  const BugReportContext report =
      BattlegroundBugReport(BattlegroundBugScene{.queue = &queue,
                                                 .battlefield = &battlefield,
                                                 .map_name = "bat_b01",
                                                 .on_battlefield = true,
                                                 .now_ms = kNowMs});

  REQUIRE(moonclient::web::ParseJson(report.json).has_value());
  CHECK(report.label.find("bat_b01") != std::string::npos);
  CHECK(Has(report, R"("on_battlefield":true)"));
  CHECK(Has(report, R"("own_camp":3,"side":"lion","score":{"lion":2,"eagle":5})"));
  CHECK(Has(report, R"("phase":"idle")"));
  // Aucun terrain choisi : pas de fiche de terrain.
  CHECK_FALSE(Has(report, R"("field":{)"));
}

TEST_CASE("Rapport de champ : table illisible, un rapport quand même", "[app][bugreport]") {
  const BugReportContext report = BattlegroundBugReport(BattlegroundBugScene{});
  REQUIRE(moonclient::web::ParseJson(report.json).has_value());
  CHECK(Has(report, R"("table":{"loaded":false,"fields":0})"));
  CHECK_FALSE(report.label.empty());
}
