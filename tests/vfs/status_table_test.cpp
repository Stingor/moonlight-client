// LA TABLE DES ÉTATS : nom, description, couleurs, icône et seau de chaque
// état, lus dans `status_effects.yml`.

#include "ro/vfs/status_table.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "ro/vfs/vfs.h"

#include "support/temp_directory.h"

using ro::test_support::TempDirectory;
using ro::vfs::ParseStatusTable;
using ro::vfs::StatusColor;
using ro::vfs::StatusInfo;
using ro::vfs::StatusPriority;
using ro::vfs::StatusTable;

namespace {

constexpr std::uint32_t kProvoke = 0;
constexpr std::uint32_t kEndure = 1;
constexpr std::uint32_t kStone = 2;
constexpr std::uint32_t kBlessing = 10;
constexpr std::uint32_t kNameless = 12;
constexpr StatusColor kBuffColor = {155, 202, 155};
constexpr StatusColor kTimeColor = {255, 176, 98};

constexpr std::string_view kTablePathOnDisk = "data/yaml_files/status_effects.yml";

// La forme que le script écrit : un en-tête, un état complet, une icône seule
// et coréenne, un nom seul dont la durée se glisse entre deux lignes, un état
// sans nom ni icône.
constexpr std::string_view kTable = R"yaml(# Les états
version: 1
statuses:
  0:
    constant: EFST_PROVOKE
    icon: provoke.tga
  1:
    constant: EFST_ENDURE
    icon: Endure.tga
    priority: gold
    lines:
    - {text: Endure, color: [155, 202, 155]}
    - {text: '%s', color: [255, 176, 98]}
    - {text: Resists flinching}
  2:
    constant: EFST_STONE
    icon: 돌.TGA
    priority: red
  10:
    constant: EFST_BLESSING
    lines:
    - {text: Blessing}
    - {text: Increases STR}
    - {text: ''}
    - {text: '%s'}
    - {text: Increases INT}
  12:
    constant: EFST_NAMELESS
    lines:
    - {text: ''}
    - {text: Personne}
)yaml";

[[nodiscard]] StatusTable Parsed(std::string_view text) {
  auto table = ParseStatusTable(text);
  REQUIRE(table.has_value());
  return std::move(*table);
}

}  // namespace

TEST_CASE("États : la première ligne est le nom, avec sa couleur", "[vfs][statuts]") {
  const StatusTable table = Parsed(kTable);
  const StatusInfo* endure = table.Find(kEndure);
  REQUIRE(endure != nullptr);
  CHECK(endure->name == "Endure");
  CHECK(endure->name_color == kBuffColor);
  // Le nom n'est PAS dans la description : il a sa place à lui.
  REQUIRE(endure->description.size() == 1);
  CHECK(endure->description[0] == "Resists flinching");
  REQUIRE(endure->description_colors.size() == 1);
  CHECK_FALSE(endure->description_colors[0].has_value());
  CHECK(table.NameOf(kEndure) == "Endure");
}

TEST_CASE("États : la ligne « %s » est la PLACE de la durée, pas du texte", "[vfs][statuts]") {
  // 🔴 LA JETER PERDRAIT SA PLACE, LA GARDER ÉCRIRAIT « %s » À L'ÉCRAN : c'est
  // l'emplacement que le client remplit avec le temps restant.
  const StatusTable table = Parsed(kTable);
  const StatusInfo* endure = table.Find(kEndure);
  REQUIRE(endure != nullptr);
  CHECK(endure->time_line == 0);
  CHECK(endure->time_color == kTimeColor);

  // Le cas qui distingue un RANG d'un drapeau : la durée se glisse ENTRE deux
  // lignes, et la ligne vide qui la précède ne compte pas.
  const StatusInfo* blessing = table.Find(kBlessing);
  REQUIRE(blessing != nullptr);
  REQUIRE(blessing->description.size() == 2);
  CHECK(blessing->description[0] == "Increases STR");
  CHECK(blessing->description[1] == "Increases INT");
  CHECK(blessing->time_line == 1);
  CHECK_FALSE(blessing->time_color.has_value());
}

TEST_CASE("États : le seau dit le RANG d'affichage, et le nom de fichier reste nu",
          "[vfs][statuts]") {
  const StatusTable table = Parsed(kTable);
  const StatusInfo* endure = table.Find(kEndure);
  REQUIRE(endure != nullptr);
  CHECK(endure->priority == StatusPriority::kGold);

  const StatusInfo* stone = table.Find(kStone);
  REQUIRE(stone != nullptr);
  CHECK(stone->priority == StatusPriority::kRed);
  CHECK(stone->icon_file == "돌.TGA");
  CHECK(stone->name.empty());

  // Sans seau, l'icône se range au dernier.
  const StatusInfo* provoke = table.Find(kProvoke);
  REQUIRE(provoke != nullptr);
  CHECK(provoke->priority == StatusPriority::kWhite);
  CHECK(provoke->icon_file == "provoke.tga");
}

TEST_CASE("États : un nom sans icône, une icône sans nom, ni l'un ni l'autre", "[vfs][statuts]") {
  // ⚠ LES DEUX NE SE RECOUVRENT PAS : exiger les deux en perdrait des deux
  // côtés. Une première ligne vide ne nomme rien, et l'état sans nom ni icône
  // n'entre pas.
  const StatusTable table = Parsed(kTable);
  CHECK(table.size() == 4);
  CHECK(table.drawn() == 3);
  CHECK(table.Find(kNameless) == nullptr);
  CHECK(table.NameOf(kNameless).empty());
}

TEST_CASE("États : une forme inattendue est refusée", "[vfs][statuts]") {
  CHECK_FALSE(ParseStatusTable("version: 2\nstatuses: {}\n").has_value());
  CHECK_FALSE(ParseStatusTable("version: 1\n").has_value());
  CHECK_FALSE(ParseStatusTable("version: 1\nstatuses: {1: {priority: silver}}\n").has_value());
  CHECK_FALSE(ParseStatusTable("version: 1\nstatuses: {1: {icon: a.tga, size: 3}}\n").has_value());
  CHECK_FALSE(
      ParseStatusTable("version: 1\nstatuses: {1: {lines: [{color: [1, 2, 3]}]}}\n").has_value());
  CHECK_FALSE(ParseStatusTable("version: 1\nstatuses: {1: {lines: [{text: A, color: [1, 2]}]}}\n")
                  .has_value());
  CHECK_FALSE(
      ParseStatusTable("version: 1\nstatuses: {1: {lines: [{text: A, color: [1, 2, 300]}]}}\n")
          .has_value());
  CHECK_FALSE(ParseStatusTable("version: 1\nstatuses: {1: {lines: [").has_value());
}

TEST_CASE("États : la table se lit par le VFS, et son absence n'est pas une erreur",
          "[vfs][statuts]") {
  TempDirectory client;
  client.WriteText("data/marqueur.txt", "");
  {
    auto mounted = ro::vfs::Vfs::Mount({.root = client.path()});
    REQUIRE(mounted.has_value());
    // Un état sans nom se montre encore par son numéro.
    const auto absent = ro::vfs::ReadStatusTable(*mounted);
    REQUIRE(absent.has_value());
    CHECK(absent->empty());
  }
  client.WriteText(std::string(kTablePathOnDisk), std::string(kTable));
  auto mounted = ro::vfs::Vfs::Mount({.root = client.path()});
  REQUIRE(mounted.has_value());
  const auto present = ro::vfs::ReadStatusTable(*mounted);
  REQUIRE(present.has_value());
  CHECK(present->NameOf(kEndure) == "Endure");

  client.WriteText(std::string(kTablePathOnDisk), "version: 9\n");
  auto remounted = ro::vfs::Vfs::Mount({.root = client.path()});
  REQUIRE(remounted.has_value());
  CHECK_FALSE(ro::vfs::ReadStatusTable(*remounted).has_value());
}
