// LES PROPRIÉTÉS D'ITEM DU SERVEUR — la table exportée de son item_db :
// empilement, drapeau de buying store, poids.
//
// 🔴 CE BANC TIENT LA DIFFÉRENCE ENTRE « ON NE SAIT PAS » ET « NON » : une
// table absente laisse toute question sans réponse, une table illisible est une
// erreur plutôt qu'une table vide, et l'ancienne table des items non
// empilables, seule, ne répond qu'à l'empilement.

#include "ro/vfs/item_properties.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "ro/vfs/item_stacking.h"

#include "support/temp_directory.h"

using ro::test_support::TempDirectory;
using ro::vfs::ItemProperties;
using ro::vfs::ParseItemProperties;

namespace {

constexpr std::uint32_t kSword = 1101;
constexpr std::uint32_t kRedPotion = 501;
constexpr std::uint32_t kJellopy = 909;
constexpr std::uint32_t kFeather = 916;

// La forme que le script écrit : un en-tête de commentaires, puis les listes.
constexpr std::string_view kTable = R"yaml(# Les propriétés d'item
version: 1
mode: Prerenewal
non_stackable: [
  1101
]
buying_store: [
  501, 909
]
weight: {
  501: 70, 909: 10,
  1101: 500
}
)yaml";

// L'ancienne table, qui ne porte que l'empilement.
constexpr std::string_view kStackingTable = "version: 1\nnon_stackable: [1101]\n";

constexpr std::string_view kTablePathOnDisk = "data/yaml_files/item_properties.yml";
constexpr std::string_view kStackingPathOnDisk = "data/yaml_files/non_stackable_items.yml";

}  // namespace

TEST_CASE("Propriétés d'item : la table dit l'empilement, le buying store et le poids",
          "[vfs][item_properties]") {
  const auto properties = ParseItemProperties(kTable);
  REQUIRE(properties.has_value());
  CHECK(properties->known());
  CHECK(properties->IsStackable(kSword) == false);
  CHECK(properties->IsStackable(kRedPotion) == true);
  CHECK(properties->BuyingStoreAllowed(kRedPotion) == true);
  CHECK(properties->BuyingStoreAllowed(kSword) == false);
  CHECK(properties->buying_store_count() == 2);
  CHECK(properties->WeightTenthsOf(kRedPotion) == 70U);
  CHECK(properties->WeightTenthsOf(kSword) == 500U);
  // Un item que la table ne pèse pas pèse 0.
  CHECK(properties->WeightTenthsOf(kFeather) == 0U);
  CHECK(properties->weighed_count() == 3);
}

TEST_CASE("Propriétés d'item : l'ancien lecteur lit la nouvelle table", "[vfs][item_properties]") {
  const auto stacking = ro::vfs::ParseItemStacking(kTable);
  REQUIRE(stacking.has_value());
  CHECK(stacking->IsStackable(kSword) == false);
  CHECK(stacking->IsStackable(kJellopy) == true);
}

TEST_CASE("Propriétés d'item : sans table, on ne sait rien", "[vfs][item_properties]") {
  const ItemProperties unknown;
  CHECK_FALSE(unknown.known());
  CHECK_FALSE(unknown.IsStackable(kSword).has_value());
  CHECK_FALSE(unknown.BuyingStoreAllowed(kRedPotion).has_value());
  CHECK_FALSE(unknown.WeightTenthsOf(kRedPotion).has_value());
}

TEST_CASE("Propriétés d'item : un texte mal formé est refusé", "[vfs][item_properties]") {
  CHECK_FALSE(ParseItemProperties("version: 2\nnon_stackable: []\nbuying_store: []\nweight: {}\n")
                  .has_value());
  // L'ancienne table seule n'est pas une table de propriétés.
  CHECK_FALSE(ParseItemProperties(kStackingTable).has_value());
  CHECK_FALSE(ParseItemProperties("version: 1\nnon_stackable: []\nbuying_store: 501\nweight: {}\n")
                  .has_value());
  CHECK_FALSE(ParseItemProperties("version: 1\nnon_stackable: []\nbuying_store: []\nweight: [70]\n")
                  .has_value());
  CHECK_FALSE(
      ParseItemProperties("version: 1\nnon_stackable: []\nbuying_store: []\nweight: {501: lourd}\n")
          .has_value());
  CHECK_FALSE(ParseItemProperties("[1101]\n").has_value());
  CHECK_FALSE(ParseItemProperties("weight: {501: 70\n").has_value());
}

TEST_CASE("Propriétés d'item : lue dans l'installation, absente elle reste inconnue",
          "[vfs][item_properties]") {
  TempDirectory client;
  client.WriteText("data/clientinfo.xml", "<clientinfo/>");
  {
    auto mounted = ro::vfs::Vfs::Mount({.root = client.path()});
    REQUIRE(mounted.has_value());
    const auto absent = ro::vfs::ReadItemProperties(*mounted);
    REQUIRE(absent.has_value());
    CHECK_FALSE(absent->known());
    CHECK_FALSE(absent->IsStackable(kSword).has_value());
  }
  // L'ANCIENNE TABLE SEULE répond à l'empilement, et à rien d'autre.
  client.WriteText(std::string(kStackingPathOnDisk), kStackingTable);
  {
    auto mounted = ro::vfs::Vfs::Mount({.root = client.path()});
    REQUIRE(mounted.has_value());
    const auto old_only = ro::vfs::ReadItemProperties(*mounted);
    REQUIRE(old_only.has_value());
    CHECK_FALSE(old_only->known());
    CHECK(old_only->IsStackable(kSword) == false);
    CHECK_FALSE(old_only->BuyingStoreAllowed(kRedPotion).has_value());
    CHECK_FALSE(old_only->WeightTenthsOf(kRedPotion).has_value());
  }
  // La nouvelle l'emporte.
  client.WriteText(std::string(kTablePathOnDisk), kTable);
  auto mounted = ro::vfs::Vfs::Mount({.root = client.path()});
  REQUIRE(mounted.has_value());
  const auto present = ro::vfs::ReadItemProperties(*mounted);
  REQUIRE(present.has_value());
  CHECK(present->known());
  CHECK(present->BuyingStoreAllowed(kJellopy) == true);
  CHECK(present->WeightTenthsOf(kJellopy) == 10U);
}

TEST_CASE("Propriétés d'item : une table illisible est une erreur, pas une table vide",
          "[vfs][item_properties]") {
  TempDirectory client;
  client.WriteText(std::string(kTablePathOnDisk), "version: 9\n");
  auto mounted = ro::vfs::Vfs::Mount({.root = client.path()});
  REQUIRE(mounted.has_value());
  CHECK_FALSE(ro::vfs::ReadItemProperties(*mounted).has_value());
}
