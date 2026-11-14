// LES RÉPLIQUES DES FAMILIERS : la table lue, et la réplique qu'un entier reçu
// désigne.

#include "ro/vfs/pet_talk_table.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/vfs/vfs.h"

#include "support/temp_directory.h"

using ro::test_support::TempDirectory;
using ro::vfs::ParsePetTalkTable;
using ro::vfs::PetTalkTable;
using Lines = std::vector<std::string>;

namespace {

constexpr std::uint32_t kPoring = 1002;
constexpr std::uint32_t kGolem = 1040;
constexpr std::uint32_t kMastering = 1090;
constexpr int kHungry = 0;
constexpr int kFull = 3;
constexpr int kFeeding = 0;
constexpr int kDanger = 2;
constexpr int kStand = 4;
constexpr int kConnect = 10;

// La forme que le script écrit : un en-tête, une classe nommée, une classe dont
// l'espèce n'a pas de répliques, et du texte qui n'est pas de l'anglais.
constexpr std::string_view kTable = R"yaml(# Ce que disent les familiers
version: 1
default_species: poring
classes:
  1040: GOLEM
  1090: mastering
species:
  poring:
    hungry:
      feeding:
      - Not bad, what else ya got?
      - Can I have some more please?
      danger:
      - If you die, what do you drop?
      connect:
      - Hey there you are, I've been starving!
    full:
      stand:
      - "\tB "
  GOLEM:
    noting:
      levelup:
      - 말
)yaml";

[[nodiscard]] Lines Said(const PetTalkTable& table, std::uint32_t pet_class, int hunger,
                         int occasion) {
  const auto lines = table.LinesOf(pet_class, hunger, occasion);
  return {lines.begin(), lines.end()};
}

}  // namespace

TEST_CASE("PetTalkTable : la réplique se trouve par classe, faim et occasion", "[vfs][pettalk]") {
  const auto table = ParsePetTalkTable(kTable);
  REQUIRE(table.has_value());
  CHECK(Said(*table, kPoring, kHungry, kFeeding) ==
        Lines{"Not bad, what else ya got?", "Can I have some more please?"});
  // Les blancs que le script a gardés restent.
  CHECK(Said(*table, kPoring, kFull, kStand) == Lines{"\tB "});
  CHECK(Said(*table, kGolem, 2, 6) == Lines{"말"});
  // Le salut, que seul le reste trois désigne.
  CHECK(Said(*table, kPoring, kHungry, kConnect) ==
        Lines{"Hey there you are, I've been starving!"});
}

TEST_CASE("PetTalkTable : une classe inconnue parle comme l'espèce par défaut", "[vfs][pettalk]") {
  const auto table = ParsePetTalkTable(kTable);
  REQUIRE(table.has_value());
  // Une classe que la correspondance ne nomme pas.
  CHECK(Said(*table, 90, 5, kDanger) == Lines{"If you die, what do you drop?"});
  // Une espèce sans répliques se tait, au lieu de parler comme le défaut.
  CHECK(Said(*table, kMastering, kHungry, kFeeding).empty());
  // Le Golem n'a rien à dire au repas.
  CHECK(Said(*table, kGolem, kHungry, kFeeding).empty());
}

TEST_CASE("PetTalkTable : un chiffre de faim hors des rangs parle comme l'affamé",
          "[vfs][pettalk]") {
  const auto table = ParsePetTalkTable(kTable);
  REQUIRE(table.has_value());
  CHECK(Said(*table, kPoring, 9, kFeeding).size() == 2);
  CHECK(Said(*table, kPoring, -1, kFeeding).size() == 2);
  // Une occasion hors des noms ne dit rien.
  CHECK(Said(*table, kPoring, kHungry, -3).empty());
  CHECK(Said(*table, kPoring, kHungry, 11).empty());
}

TEST_CASE("PetTalkTable : une table mal formée est refusée", "[vfs][pettalk]") {
  CHECK_FALSE(ParsePetTalkTable("version: 2\ndefault_species: poring\nspecies: {}\n").has_value());
  CHECK_FALSE(ParsePetTalkTable("version: 1\nspecies: {}\n").has_value());
  CHECK_FALSE(
      ParsePetTalkTable("version: 1\ndefault_species: poring\nspecies: {poring: {starving: {}}}\n")
          .has_value());
  CHECK_FALSE(ParsePetTalkTable(
                  "version: 1\ndefault_species: poring\nspecies: {poring: {hungry: {eat: [a]}}}\n")
                  .has_value());
  CHECK_FALSE(ParsePetTalkTable("[1, 2]").has_value());
}

TEST_CASE("PetTalkTable : la table se lit par le VFS, et son absence ne coûte rien",
          "[vfs][pettalk]") {
  TempDirectory client;
  client.WriteText("data/yaml_files/pets.yml", "version: 1\npets: {}\n");
  {
    auto vfs = ro::vfs::Vfs::Mount({.root = client.path()});
    REQUIRE(vfs.has_value());
    CHECK(ro::vfs::ReadPetTalkTable(*vfs).lines_by_species.empty());
  }
  client.WriteText("data/yaml_files/pet_talk.yml", std::string(kTable));
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path()});
  REQUIRE(vfs.has_value());
  const PetTalkTable table = ro::vfs::ReadPetTalkTable(*vfs);
  CHECK(Said(table, kPoring, kHungry, kDanger).size() == 1);
}
