// LES RÉPLIQUES DES MONSTRES : la table lue, et les répliques d'une espèce.

#include "ro/vfs/monster_talk_table.h"

#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/vfs/vfs.h"

#include "support/temp_directory.h"

using ro::test_support::TempDirectory;
using ro::vfs::MonsterTalkTable;
using ro::vfs::ParseMonsterTalkTable;
using Lines = std::vector<std::string>;

namespace {

// La forme que le script écrit : un en-tête, deux espèces, du texte qui n'est
// pas de l'anglais.
constexpr std::string_view kTable = R"yaml(# Ce que disent les monstres
version: 1
species:
  poring:
    attack:
    - Boing !
    - "\tPoing"
  orc_warrior:
    attack:
    - 꺄
)yaml";

[[nodiscard]] Lines Said(const MonsterTalkTable& table, std::string_view species) {
  const auto lines = table.AttackLinesOf(species);
  return {lines.begin(), lines.end()};
}

}  // namespace

TEST_CASE("MonsterTalkTable : les répliques de coup d'une espèce, dans l'ordre",
          "[vfs][monstertalk]") {
  const auto table = ParseMonsterTalkTable(kTable);
  REQUIRE(table.has_value());
  CHECK(Said(*table, "poring") == Lines{"Boing !", "\tPoing"});
  CHECK(Said(*table, "orc_warrior") == Lines{"꺄"});
  // La casse compte, et une espèce absente ne dit rien.
  CHECK(Said(*table, "Poring").empty());
  CHECK(Said(*table, "lunatic").empty());
}

TEST_CASE("MonsterTalkTable : la table vide de l'installation se lit", "[vfs][monstertalk]") {
  const auto table = ParseMonsterTalkTable("version: 1\nspecies: {}\n");
  REQUIRE(table.has_value());
  CHECK(table->attack_lines_by_species.empty());
}

TEST_CASE("MonsterTalkTable : une table mal formée est refusée", "[vfs][monstertalk]") {
  CHECK_FALSE(ParseMonsterTalkTable("version: 2\nspecies: {}\n").has_value());
  CHECK_FALSE(ParseMonsterTalkTable("version: 1\n").has_value());
  CHECK_FALSE(ParseMonsterTalkTable("version: 1\nspecies: {poring: [a]}\n").has_value());
  CHECK_FALSE(ParseMonsterTalkTable("version: 1\nspecies: {poring: {hp50: [a]}}\n").has_value());
  CHECK_FALSE(ParseMonsterTalkTable("version: 1\nspecies: {poring: {attack: a}}\n").has_value());
  CHECK_FALSE(ParseMonsterTalkTable("[1, 2]").has_value());
}

TEST_CASE("MonsterTalkTable : la table se lit par le VFS, et son absence ne coûte rien",
          "[vfs][monstertalk]") {
  TempDirectory client;
  client.WriteText("data/yaml_files/pets.yml", "version: 1\npets: {}\n");
  {
    auto vfs = ro::vfs::Vfs::Mount({.root = client.path()});
    REQUIRE(vfs.has_value());
    CHECK(ro::vfs::ReadMonsterTalkTable(*vfs).attack_lines_by_species.empty());
  }
  client.WriteText("data/yaml_files/monster_talk.yml", std::string(kTable));
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path()});
  REQUIRE(vfs.has_value());
  CHECK(Said(ro::vfs::ReadMonsterTalkTable(*vfs), "poring").size() == 2);
}
