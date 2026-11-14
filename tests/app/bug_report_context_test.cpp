// LE CONTEXTE D'UN RAPPORT DE BUG : un JSON que le serveur range tel quel, et
// qui doit donc être valable quoi que porte le nom qu'on y met.

#include "game/bug_report_context.h"

#include <array>
#include <cstdint>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/build_stamp.h"
#include "ro/net/bug_report.h"

#include "web/json_value.h"

using moonclient::AskBugReport;
using moonclient::BugReportContext;
using moonclient::GenericBugReport;
using moonclient::ItemBugReport;
using moonclient::MonsterBugReport;
using moonclient::MonsterBugScene;
using moonclient::NpcBugReport;

namespace {

void CheckValid(const BugReportContext& context) {
  const auto parsed = moonclient::web::ParseJson(context.json);
  REQUIRE(parsed.has_value());
  // 🔴 LA RÉVISION DANS CHAQUE CONTEXTE : sans elle, on ne sait pas si le
  // défaut signalé est déjà corrigé.
  CHECK(context.json.find(std::string(ro::core::kBuildCommit)) != std::string::npos);
}

}  // namespace

TEST_CASE("Rapport de bug : un nom à guillemets reste un JSON valable", "[app][bugreport]") {
  const BugReportContext item =
      ItemBugReport(1201, "+7 \"Knife\" \\ [3]", 7, std::array<std::uint32_t, 4>{4001, 0, 0, 0});
  CheckValid(item);
  CHECK(item.category == static_cast<std::uint8_t>(ro::net::BugReportCategory::kItem));
  CHECK(item.json.find("\"refine\":7") != std::string::npos);
  CHECK(item.json.find("\"cards\":[4001,0,0,0]") != std::string::npos);
  CHECK(item.label.find("1201") != std::string::npos);
}

TEST_CASE("Rapport de bug : le PNJ porte npc_gid, la clé que le serveur cherche",
          "[app][bugreport]") {
  const BugReportContext npc = NpcBugReport(110000123, "");
  CheckValid(npc);
  CHECK(npc.json.find("\"npc_gid\":110000123") != std::string::npos);
  // Un nom vide se lit « ? », jamais « PNJ :  (GID …) ».
  CHECK(npc.label.find('?') != std::string::npos);
  CheckValid(GenericBugReport());
}

TEST_CASE("Rapport de bug : le monstre d'une fiche, sa carte quand on l'a vu en jeu",
          "[app][bugreport]") {
  constexpr std::uint32_t kBaphomet = 1039;
  constexpr std::uint8_t kMvpRank = 2;
  const BugReportContext seen = MonsterBugReport(MonsterBugScene{.monster_id = kBaphomet,
                                                                 .by_sprite_class = true,
                                                                 .name = "Baphomet \"MVP\"",
                                                                 .aegis_name = "BAPHOMET",
                                                                 .boss_rank = kMvpRank,
                                                                 .seen_map = "prt_maze03"});
  CheckValid(seen);
  // Le serveur n'a pas de catégorie « monstre » : le rapport est général.
  CHECK(seen.category == static_cast<std::uint8_t>(ro::net::BugReportCategory::kGeneric));
  CHECK(seen.label.find("Baphomet") != std::string::npos);
  CHECK(seen.label.find("#1039") != std::string::npos);
  const auto parsed = moonclient::web::ParseJson(seen.json);
  REQUIRE(parsed.has_value());
  // Une classe de sprite se dit comme telle, jamais comme identifiant de base.
  CHECK(parsed->WholeAt("sprite_class") == kBaphomet);
  CHECK(parsed->Field("mob_id") == nullptr);
  CHECK(parsed->TextAt("name") == "Baphomet \"MVP\"");
  CHECK(parsed->TextAt("aegis_name") == "BAPHOMET");
  CHECK(parsed->WholeAt("boss_rank") == kMvpRank);
  CHECK(parsed->TextAt("seen_map") == "prt_maze03");

  // DEPUIS LA BASE : un identifiant de base, et aucune carte.
  const BugReportContext listed =
      MonsterBugReport(MonsterBugScene{.monster_id = kBaphomet, .name = "Baphomet"});
  CheckValid(listed);
  const auto from_base = moonclient::web::ParseJson(listed.json);
  REQUIRE(from_base.has_value());
  CHECK(from_base->WholeAt("mob_id") == kBaphomet);
  CHECK(from_base->Field("sprite_class") == nullptr);
  CHECK(from_base->Field("seen_map") == nullptr);
  CHECK(from_base->Field("aegis_name") == nullptr);
}

TEST_CASE("Rapport de bug : une boîte ouverte ne se remplace pas", "[app][bugreport]") {
  ro::ui::BugReportWindowState box;
  AskBugReport(box, NpcBugReport(1, "Kafra"));
  CHECK(box.asked);
  box.open = true;
  AskBugReport(box, GenericBugReport());
  CHECK(box.category == static_cast<std::uint8_t>(ro::net::BugReportCategory::kNpc));
}
