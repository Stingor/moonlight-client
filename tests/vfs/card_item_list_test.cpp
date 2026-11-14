#include "ro/vfs/card_item_list.h"

#include <string>
#include <string_view>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "ro/vfs/vfs.h"

#include "support/grf_builder.h"
#include "support/temp_directory.h"

using ro::test_support::BuildGrf;
using ro::test_support::Bytes;
using ro::test_support::TempDirectory;
using ro::vfs::CardItemList;
using ro::vfs::ParseCardItemList;
using ro::vfs::ReadCardItemList;
using ro::vfs::Vfs;

namespace {

constexpr std::string_view kArchiveName = "donnees.grf";

// Ce que la conversion tire du fichier que lisait l'ancien lecteur : un
// commentaire, une ligne vide, « carte# », « 4003 » sans dièse, « 4x04# »,
// puis « 4005# », « 4001# » répété, « 4001#Poring Card# » et « 31026# » sans
// fin de ligne. Seules les têtes numériques suivies d'un dièse comptent.
constexpr std::string_view kConverted = "version: 1\ncards:\n- 4001\n- 4005\n- 31026\n";

}  // namespace

TEST_CASE("Liste des cartes : une entrée nomme un identifiant d'objet", "[vfs][cartes]") {
  auto cards = ParseCardItemList(kConverted);
  REQUIRE(cards.has_value());
  CHECK(cards->size() == 3);
  CHECK(cards->Contains(4001));
  CHECK(cards->Contains(4005));
  CHECK(cards->Contains(31026));
  CHECK_FALSE(cards->Contains(4003));
  CHECK_FALSE(cards->Contains(617));
}

TEST_CASE("Liste des cartes : un identifiant répété ne compte qu'une fois", "[vfs][cartes]") {
  auto cards = ParseCardItemList("version: 1\ncards: [4001, 4001]\n");
  REQUIRE(cards.has_value());
  CHECK(cards->size() == 1);
}

TEST_CASE("Liste des cartes : une table mal formée est refusée", "[vfs][cartes]") {
  CHECK_FALSE(ParseCardItemList("version: 1\ncards: [carte]\n").has_value());
  CHECK_FALSE(ParseCardItemList("version: 1\ncards: [4294967296]\n").has_value());
  CHECK_FALSE(ParseCardItemList("version: 1\n").has_value());
}

TEST_CASE("Liste des cartes : la table se lit dans l'installation", "[vfs][cartes]") {
  const TempDirectory client;
  client.WriteFile(std::string(kArchiveName),
                   BuildGrf({{"data\\yaml_files\\card_items.yml", Bytes(kConverted)}}));
  auto vfs = Vfs::Mount({.root = client.path(), .archives = {std::string(kArchiveName)}});
  REQUIRE(vfs.has_value());
  auto cards = ReadCardItemList(*vfs);
  REQUIRE(cards.has_value());
  CHECK(cards->Contains(31026));
}

TEST_CASE("Liste des cartes : une table absente n'est pas une erreur", "[vfs][cartes]") {
  const TempDirectory client;
  client.WriteFile(std::string(kArchiveName), BuildGrf({{"data\\autre.txt", Bytes("rien")}}));
  auto vfs = Vfs::Mount({.root = client.path(), .archives = {std::string(kArchiveName)}});
  REQUIRE(vfs.has_value());

  auto cards = ReadCardItemList(*vfs);
  REQUIRE(cards.has_value());
  CHECK(cards->empty());
}
