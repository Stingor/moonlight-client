// LES ITEMS QUE LE SERVEUR N'EMPILE PAS — la table exportée de son item_db.
//
// 🔴 CE BANC TIENT LA DIFFÉRENCE ENTRE « ON NE SAIT PAS » ET « ÇA S'EMPILE » :
// une table absente laisse tout item inconnu, jamais empilable, et une table
// illisible est une erreur plutôt qu'une table vide.

#include "ro/vfs/item_stacking.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "support/temp_directory.h"

using ro::test_support::TempDirectory;
using ro::vfs::ItemStacking;
using ro::vfs::ParseItemStacking;

namespace {

constexpr std::uint32_t kSword = 1101;
constexpr std::uint32_t kPoringEgg = 9001;
constexpr std::uint32_t kRedPotion = 501;

// La forme que le script écrit : un en-tête de commentaires, puis la liste.
constexpr std::string_view kTable = R"yaml(# Les items qui ne s'empilent pas
version: 1
mode: Renewal
non_stackable: [
  1101, 9001
]
)yaml";

constexpr std::string_view kTablePathOnDisk = "data/yaml_files/non_stackable_items.yml";

}  // namespace

TEST_CASE("Empilement : la table nomme ce qui ne s'empile pas", "[vfs][item_stacking]") {
  const auto stacking = ParseItemStacking(kTable);
  REQUIRE(stacking.has_value());
  CHECK(stacking->known());
  CHECK(stacking->non_stackable_count() == 2);
  CHECK(stacking->IsStackable(kSword) == false);
  CHECK(stacking->IsStackable(kPoringEgg) == false);
  // Un item qu'elle ne nomme pas s'empile.
  CHECK(stacking->IsStackable(kRedPotion) == true);
}

TEST_CASE("Empilement : sans table, on ne sait rien", "[vfs][item_stacking]") {
  const ItemStacking unknown;
  CHECK_FALSE(unknown.known());
  CHECK_FALSE(unknown.IsStackable(kSword).has_value());
  CHECK_FALSE(unknown.IsStackable(kRedPotion).has_value());
}

TEST_CASE("Empilement : un texte mal formé est refusé", "[vfs][item_stacking]") {
  CHECK_FALSE(ParseItemStacking("version: 2\nnon_stackable: [1101]\n").has_value());
  CHECK_FALSE(ParseItemStacking("version: 1\nnon_stackable: 1101\n").has_value());
  CHECK_FALSE(ParseItemStacking("version: 1\n").has_value());
  CHECK_FALSE(ParseItemStacking("version: 1\nnon_stackable: [épée]\n").has_value());
  CHECK_FALSE(ParseItemStacking("[1101, 9001]\n").has_value());
  CHECK_FALSE(ParseItemStacking("non_stackable: [1101\n").has_value());
}

TEST_CASE("Empilement : lue dans l'installation, absente elle reste inconnue",
          "[vfs][item_stacking]") {
  TempDirectory client;
  // Une installation sans la table : un dossier data\ qui porte autre chose.
  client.WriteText("data/clientinfo.xml", "<clientinfo/>");
  {
    auto mounted = ro::vfs::Vfs::Mount({.root = client.path()});
    REQUIRE(mounted.has_value());
    const auto absent = ro::vfs::ReadItemStacking(*mounted);
    REQUIRE(absent.has_value());
    CHECK_FALSE(absent->known());
  }
  client.WriteText(std::string(kTablePathOnDisk), kTable);
  auto mounted = ro::vfs::Vfs::Mount({.root = client.path()});
  REQUIRE(mounted.has_value());
  const auto present = ro::vfs::ReadItemStacking(*mounted);
  REQUIRE(present.has_value());
  CHECK(present->IsStackable(kSword) == false);
  CHECK(present->IsStackable(kRedPotion) == true);
}

TEST_CASE("Empilement : une table illisible est une erreur, pas une table vide",
          "[vfs][item_stacking]") {
  TempDirectory client;
  client.WriteText(std::string(kTablePathOnDisk), "version: 9\nnon_stackable: []\n");
  auto mounted = ro::vfs::Vfs::Mount({.root = client.path()});
  REQUIRE(mounted.has_value());
  CHECK_FALSE(ro::vfs::ReadItemStacking(*mounted).has_value());
}
