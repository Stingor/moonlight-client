// LES MONSTRES QUE LE SERVEUR CONNAÎT — la table exportée de sa mob_db.
//
// 🔴 CE BANC TIENT LA DIFFÉRENCE ENTRE « ON NE SAIT PAS » ET « IL N'Y EN A
// PAS » : une table absente reste inconnue, une table illisible est une erreur
// plutôt qu'une table vide.

#include "ro/vfs/monster_list.h"

#include <cstdint>
#include <string>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "support/temp_directory.h"

using ro::test_support::TempDirectory;
using ro::vfs::ParseMonsterList;

namespace {

constexpr std::uint16_t kPoring = 1002;
constexpr std::uint16_t kBaphomet = 1039;
constexpr std::uint16_t kGoldenBug = 1086;
constexpr std::uint16_t kUnlisted = 1003;
constexpr std::uint8_t kPlantRace = 3;
constexpr std::uint8_t kDemonRace = 6;
constexpr std::uint8_t kDarkElement = 7;
constexpr std::uint8_t kLargeSize = 2;
constexpr std::uint16_t kBaphometLevel = 81;

// La forme que le script écrit : un en-tête de commentaires, des lignes dans
// le désordre, des noms entre guillemets.
constexpr std::string_view kTable = R"yaml(# Les monstres que le serveur connaît
version: 1
mode: Prerenewal
monsters: {
  1039: ["BAPHOMET", "Baphomet", 81, 6, 7, 3, 2, 2],
  1002: ["PORING", "Poring", 1, 3, 1, 1, 1, 0],
  1086: ["GOLDEN_BUG", "Golden Thief Bug: \"le\" roi", 64, 4, 3, 2, 2, 1]
}
)yaml";

constexpr std::string_view kTablePathOnDisk = "data/yaml_files/monster_list.yml";

}  // namespace

TEST_CASE("Liste des monstres : la table dit le nom, le niveau, la race et le rang",
          "[vfs][monster_list]") {
  const auto list = ParseMonsterList(kTable);
  REQUIRE(list.has_value());
  CHECK(list->known());
  REQUIRE(list->entries().size() == 3);
  // Rangées par identifiant, quel que soit l'ordre du fichier.
  CHECK(list->entries().front().monster_id == kPoring);
  CHECK(list->entries().back().monster_id == kGoldenBug);

  const ro::vfs::MonsterListEntry* baphomet = list->Find(kBaphomet);
  REQUIRE(baphomet != nullptr);
  CHECK(baphomet->aegis_name == "BAPHOMET");
  CHECK(baphomet->name == "Baphomet");
  CHECK(baphomet->level == kBaphometLevel);
  CHECK(baphomet->race == kDemonRace);
  CHECK(baphomet->element == kDarkElement);
  CHECK(baphomet->element_level == 3);
  CHECK(baphomet->size == kLargeSize);
  CHECK(baphomet->rank == ro::vfs::kMonsterMvpRank);

  CHECK(list->Find(kPoring)->race == kPlantRace);
  CHECK(list->Find(kPoring)->rank == ro::vfs::kMonsterOrdinaryRank);
  CHECK(list->Find(kGoldenBug)->name == "Golden Thief Bug: \"le\" roi");
  CHECK(list->Find(kGoldenBug)->rank == ro::vfs::kMonsterBossRank);
  CHECK(list->Find(kUnlisted) == nullptr);
}

TEST_CASE("Liste des monstres : sans table, on ne sait rien", "[vfs][monster_list]") {
  const ro::vfs::MonsterList unknown;
  CHECK_FALSE(unknown.known());
  CHECK(unknown.entries().empty());
  CHECK(unknown.Find(kPoring) == nullptr);
  // Une table lue mais vide, elle, est connue.
  const auto empty = ParseMonsterList("version: 1\nmonsters: {}\n");
  REQUIRE(empty.has_value());
  CHECK(empty->known());
}

TEST_CASE("Liste des monstres : un texte mal formé est refusé", "[vfs][monster_list]") {
  CHECK_FALSE(ParseMonsterList("version: 2\nmonsters: {}\n").has_value());
  CHECK_FALSE(ParseMonsterList("version: 1\nmonsters: [1002]\n").has_value());
  CHECK_FALSE(ParseMonsterList("version: 1\n").has_value());
  // Une ligne à sept champs, un rang inconnu, une race hors d'un octet, un
  // niveau qui n'est pas un nombre, un identifiant nul ou trop grand.
  CHECK_FALSE(
      ParseMonsterList("version: 1\nmonsters: {1002: [P, Poring, 1, 3, 1, 1, 1]}\n").has_value());
  CHECK_FALSE(ParseMonsterList("version: 1\nmonsters: {1002: [P, Poring, 1, 3, 1, 1, 1, 3]}\n")
                  .has_value());
  CHECK_FALSE(ParseMonsterList("version: 1\nmonsters: {1002: [P, Poring, 1, 300, 1, 1, 1, 0]}\n")
                  .has_value());
  CHECK_FALSE(ParseMonsterList("version: 1\nmonsters: {1002: [P, Poring, un, 3, 1, 1, 1, 0]}\n")
                  .has_value());
  CHECK_FALSE(
      ParseMonsterList("version: 1\nmonsters: {0: [P, Poring, 1, 3, 1, 1, 1, 0]}\n").has_value());
  CHECK_FALSE(ParseMonsterList("version: 1\nmonsters: {70000: [P, Poring, 1, 3, 1, 1, 1, 0]}\n")
                  .has_value());
  CHECK_FALSE(ParseMonsterList("[1002]\n").has_value());
  CHECK_FALSE(ParseMonsterList("monsters: {1002: [\n").has_value());
}

TEST_CASE("Liste des monstres : lue dans l'installation, absente elle reste inconnue",
          "[vfs][monster_list]") {
  TempDirectory client;
  client.WriteText("data/clientinfo.xml", "<clientinfo/>");
  {
    auto mounted = ro::vfs::Vfs::Mount({.root = client.path()});
    REQUIRE(mounted.has_value());
    const auto absent = ro::vfs::ReadMonsterList(*mounted);
    REQUIRE(absent.has_value());
    CHECK_FALSE(absent->known());
  }
  client.WriteText(std::string(kTablePathOnDisk), kTable);
  auto mounted = ro::vfs::Vfs::Mount({.root = client.path()});
  REQUIRE(mounted.has_value());
  const auto present = ro::vfs::ReadMonsterList(*mounted);
  REQUIRE(present.has_value());
  CHECK(present->known());
  CHECK(present->Find(kBaphomet) != nullptr);
}

TEST_CASE("Liste des monstres : une table illisible est une erreur, pas une table vide",
          "[vfs][monster_list]") {
  TempDirectory client;
  client.WriteText(std::string(kTablePathOnDisk), "version: 9\n");
  auto mounted = ro::vfs::Vfs::Mount({.root = client.path()});
  REQUIRE(mounted.has_value());
  CHECK_FALSE(ro::vfs::ReadMonsterList(*mounted).has_value());
}
