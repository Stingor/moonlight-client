// L'EFFET DES ÉTATS QU'UN OBJET POSE, sans fenêtre : la status_db se lit avec
// la base, et le bloc de l'objet dit, après la durée, les bonus que le script
// de l'état donne aux valeurs que l'objet lui passe. Un état absent, un script
// qui ne se lit pas entier, ou une valeur tirée au sort ne disent rien. Les
// entrées sont celles de `dist/data/yaml_files/item_db.yml` et `status.yml`.

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/vfs/server_db.h"

#include "game/item_db_block.h"

using moonclient::ItemDbBlockOf;
using ro::vfs::ServerItem;

namespace {

constexpr std::string_view kItemDb = R"yaml(Header:
  Type: ITEM_DB
  Version: 3

Body:
  - Id: 501
    AegisName: Red_Potion
    Name: Red Potion
    Type: Healing
)yaml";
constexpr std::string_view kMobDb = R"yaml(Header:
  Type: MOB_DB
  Version: 5

Body:
  - Id: 1002
    AegisName: PORING
    Name: Poring
)yaml";
constexpr std::string_view kContext = R"yaml(version: 1
battle:
  base_exp_rate: 100
)yaml";
constexpr std::string_view kStatusDb = R"yaml(Header:
  Type: STATUS_DB
  Version: 4

Body:
  - Status: Increase_Maxhp
    Script: |
      bonus bMaxHPrate, getstatus(SC_INCREASE_MAXHP, 1);
      bonus bHPrecovRate, getstatus(SC_INCREASE_MAXHP, 2);
    Bonuses:
      - {Bonus: bMaxHPrate, Type: 1004, Val: 1}
      - {Bonus: bHPrecovRate, Type: 1016, Val: 2}
  - Status: Putti_Tails_Noodles
    Script: |
      bonus bLuk, getstatus(SC_PUTTI_TAILS_NOODLES, 1);
    Bonuses:
      - {Bonus: bLuk, Type: 18, Val: 1}
  - Status: Quest_Buff1
    Script: |
      .@val1 = getstatus(SC_QUEST_BUFF1, 1);
      bonus bMatk, .@val1;
      bonus bBaseAtk, .@val1;
  - Status: Spcost_Rate
    Script: |
      bonus bUseSPrate, -getstatus(SC_SPCOST_RATE, 1);
    Bonuses:
      - {Bonus: bUseSPrate, Type: 1006, Val: 1, Negated: true}
  - Status: Unnamed
    Script: |
      bonus bUnnamed, getstatus(SC_UNNAMED, 1);
    Bonuses:
      - {Bonus: bUnnamed, Type: 9999, Val: 1}
)yaml";

[[nodiscard]] ro::vfs::ServerDb Base() {
  auto db = ro::vfs::ParseServerDb(kItemDb, kMobDb, kContext, kStatusDb);
  REQUIRE(db.has_value());
  return std::move(*db);
}

[[nodiscard]] std::vector<std::string> BlockOf(const ro::vfs::ServerDb& db, std::string script) {
  const moonclient::ItemDbNames names{.db = &db};
  return ItemDbBlockOf(ServerItem{.type = "Usable", .script = std::move(script)}, names);
}

[[nodiscard]] bool HasEffect(const std::vector<std::string>& lines) {
  return std::ranges::any_of(
      lines, [](const std::string& line) { return line.starts_with("^0000CCEffet :"); });
}

}  // namespace

TEST_CASE("Effet d'un état : la status_db se lit avec la base", "[app][item_db_effect]") {
  const ro::vfs::ServerDb db = Base();
  REQUIRE(db.known());
  const ro::vfs::ServerStatus* const putti = db.FindStatus("putti_tails_noodles");
  REQUIRE(putti != nullptr);
  CHECK(putti->constant == "PUTTI_TAILS_NOODLES");
  REQUIRE(putti->bonuses.size() == 1);
  CHECK(putti->bonuses[0].bonus_type == 18);
  CHECK(putti->bonuses[0].value_rank == 1);
  // Un script qui ne se lit pas entier n'a pas de bonus.
  REQUIRE(db.FindStatus("QUEST_BUFF1") != nullptr);
  CHECK(db.FindStatus("QUEST_BUFF1")->bonuses.empty());
  CHECK(db.FindStatus("INCHIT") == nullptr);
  // Sans status_db, la base reste connue.
  const auto bare = ro::vfs::ParseServerDb(kItemDb, kMobDb, kContext);
  REQUIRE(bare.has_value());
  CHECK(bare->known());
  CHECK(bare->FindStatus("PUTTI_TAILS_NOODLES") == nullptr);
}

TEST_CASE("Effet d'un état : un bonus, juste après la durée", "[app][item_db_effect]") {
  const ro::vfs::ServerDb db = Base();
  // 12434, « Petite Tail Noodles ».
  const std::vector<std::string> lines = BlockOf(db, "sc_start SC_PUTTI_TAILS_NOODLES,300000,20;");
  const auto duration = std::ranges::find(lines, "^0000CCDurée :^000000 5 minute(s)");
  REQUIRE(duration != lines.end());
  REQUIRE(duration + 1 != lines.end());
  CHECK(*(duration + 1) == "^0000CCEffet :^000000 LUK +20");
  // 12298, « SP Consumption Reduction Potion » : la valeur de signe opposé.
  const std::vector<std::string> potion = BlockOf(db, "sc_start SC_SPCOST_RATE,3600000,15;");
  CHECK(std::ranges::find(potion, "^0000CCEffet :^000000 Coût SP -15%") != potion.end());
}

TEST_CASE("Effet d'un état : plusieurs bonus, séparés par des virgules", "[app][item_db_effect]") {
  const ro::vfs::ServerDb db = Base();
  // 12274, « Daehwandan » : val1 et val2 de `sc_start2`.
  const std::vector<std::string> lines =
      BlockOf(db, "sc_start2 SC_INCREASE_MAXHP,3600000,5,10; percentheal 10,0;");
  // Les libellés sont ceux de la fiche du personnage.
  CHECK(std::ranges::find(lines, "^0000CCEffet :^000000 PV max +5%, Régén. PV +10%") !=
        lines.end());
}

TEST_CASE("Effet d'un état : ce qui ne se lit pas ne dit rien", "[app][item_db_effect]") {
  const ro::vfs::ServerDb db = Base();
  // Un script d'état qui ne se lit pas entier.
  CHECK_FALSE(HasEffect(BlockOf(db, "sc_start SC_QUEST_BUFF1,600000,10;")));
  // Un état absent de la status_db.
  CHECK_FALSE(HasEffect(BlockOf(db, "sc_start SC_INCHIT,600000,10;")));
  // Une valeur tirée au sort.
  CHECK_FALSE(HasEffect(BlockOf(db, "sc_start SC_PUTTI_TAILS_NOODLES,300000,rand(1,5);")));
  // Dans un bloc conditionnel.
  CHECK_FALSE(HasEffect(BlockOf(db, "if (Sex) { sc_start SC_PUTTI_TAILS_NOODLES,300000,20; }")));
  // Un bonus que le client ne sait pas nommer.
  CHECK_FALSE(HasEffect(BlockOf(db, "sc_start SC_UNNAMED,300000,20;")));
  // Deux états, dont un sans effet lisible : rien plutôt qu'une moitié.
  CHECK_FALSE(HasEffect(BlockOf(
      db, "sc_start SC_PUTTI_TAILS_NOODLES,300000,20; sc_start SC_QUEST_BUFF1,300000,10;")));
  // Sans base, rien.
  CHECK_FALSE(HasEffect(ItemDbBlockOf(
      ServerItem{.type = "Usable", .script = "sc_start SC_PUTTI_TAILS_NOODLES,300000,20;"})));
}
