// CE QUE REND UN CONTENEUR — la table `item_groups.yml`, exportée du serveur.
//
// 🔴 CE BANC TIENT LES RÈGLES DE LECTURE : un tirage garanti sort à cent pour
// cent sans rien diviser, un tirage au sort divise par son TOTAL écrit (qui peut
// dépasser la somme des poids), les tirages se numérotent à partir de 1 dans
// l'ordre du fichier, et un texte refusé ne rend rien de partiel.

#include "ro/vfs/package_items.h"

#include <string>
#include <string_view>
#include <utility>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/vfs/vfs.h"

#include "support/temp_directory.h"

using ro::test_support::TempDirectory;
using ro::vfs::PackageItems;
using ro::vfs::PackagePercent;
using ro::vfs::PackageTable;
using ro::vfs::ParsePackageItems;
using ro::vfs::Vfs;

namespace {

constexpr const char* kTableOnDisk = "data/yaml_files/item_groups.yml";

// La forme du fichier livré : l'en-tête de commentaires, puis la table.
constexpr std::string_view kSource = R"yaml(
# Ce que rend chaque conteneur.
version: 1
mode: Prerenewal
packages: {
  12612: [
    {rolls: 1, guaranteed: true, monsters: false, total: 0, entries: [[6417, 0, 3]]},
    {rolls: 2, guaranteed: false, monsters: false, total: 12, entries: [[4001, 10, 1], [4121, 1, 1]]}
  ],
  604: [
    {rolls: 1, guaranteed: false, monsters: true, total: 4, entries: [[1002, 3, 1], [1039, 1, 1]]}
  ]
}
)yaml";

[[nodiscard]] PackageItems Parse(std::string_view text = kSource) {
  auto items = ParsePackageItems(text);
  REQUIRE(items.has_value());
  return std::move(*items);
}

}  // namespace

TEST_CASE("Tables de tirage : chaque tirage garde son rang, son total et ses répétitions",
          "[vfs][tirage]") {
  const PackageItems items = Parse();
  const PackageTable* bag = items.Find(12612);
  REQUIRE(bag != nullptr);
  REQUIRE(bag->groups.size() == 2);
  CHECK(bag->groups[0].id == 1);
  CHECK(bag->groups[0].guaranteed);
  REQUIRE(bag->groups[0].entries.size() == 1);
  CHECK(bag->groups[0].entries[0].id == 6417);
  CHECK(bag->groups[0].entries[0].amount == 3);
  CHECK(bag->groups[1].id == 2);
  CHECK_FALSE(bag->groups[1].guaranteed);
  CHECK(bag->groups[1].rolls == 2);
  // Le total écrit fait foi, même au-delà de la somme des poids.
  CHECK(bag->groups[1].total == 12);
  REQUIRE(bag->groups[1].entries.size() == 2);
  CHECK(bag->groups[1].entries[1].id == 4121);
  CHECK(bag->groups[1].entries[1].weight == 1);
  CHECK(items.Find(603) == nullptr);
}

TEST_CASE("Tables de tirage : un tirage de monstres marque chacune de ses entrées",
          "[vfs][tirage]") {
  const PackageItems items = Parse();
  const PackageTable* branch = items.Find(604);
  REQUIRE(branch != nullptr);
  REQUIRE(branch->groups.size() == 1);
  for (const ro::vfs::PackageEntry& entry : branch->groups[0].entries) {
    CHECK(entry.is_monster);
  }
  const PackageTable* bag = items.Find(12612);
  REQUIRE(bag != nullptr);
  CHECK_FALSE(bag->groups[1].entries[0].is_monster);
}

TEST_CASE("Tables de tirage : la chance s'arrondit comme le client l'affiche", "[vfs][tirage]") {
  // 1 sur 11 = 9,0909… % : quatre décimales, le demi vers le haut.
  CHECK(PackagePercent(false, 1, 11) == Catch::Approx(9.0909));
  CHECK(PackagePercent(false, 10, 11) == Catch::Approx(90.9091));
  // Un tirage garanti ne divise rien.
  CHECK(PackagePercent(true, 0, 0) == Catch::Approx(100.0));
}

TEST_CASE("Tables de tirage : un texte refusé ne rend rien de partiel", "[vfs][tirage]") {
  // Une version inconnue, un tirage sans total, une entrée qui n'est pas un
  // triplet, un poids négatif : tout le texte est refusé.
  CHECK_FALSE(ParsePackageItems("version: 2\npackages: {}\n").has_value());
  CHECK_FALSE(ParsePackageItems("version: 1\npackages: {603: [{rolls: 1, guaranteed: false, "
                                "monsters: false, entries: []}]}\n")
                  .has_value());
  CHECK_FALSE(ParsePackageItems("version: 1\npackages: {603: [{rolls: 1, guaranteed: false, "
                                "monsters: false, total: 1, entries: [[501, 1]]}]}\n")
                  .has_value());
  CHECK_FALSE(ParsePackageItems("version: 1\npackages: {603: [{rolls: 1, guaranteed: false, "
                                "monsters: false, total: 1, entries: [[501, -1, 1]]}]}\n")
                  .has_value());
  CHECK_FALSE(ParsePackageItems("pas: [une table").has_value());
  // Une table sans conteneur est valide, et vide.
  const PackageItems empty = Parse("version: 1\npackages: {}\n");
  CHECK(empty.empty());
}

TEST_CASE("Tables de tirage : une installation sans la table rend une table vide",
          "[vfs][tirage]") {
  // Une installation montable, à qui seule cette table manque.
  TempDirectory client;
  client.WriteText("data/yaml_files/pets.yml", "version: 1\npets: {}\n");
  auto vfs = Vfs::Mount({.root = client.path()});
  REQUIRE(vfs.has_value());
  auto items = ro::vfs::ReadPackageItems(*vfs);
  REQUIRE(items.has_value());
  CHECK(items->empty());
}

TEST_CASE("Tables de tirage : la table se lit sous data, et illisible elle est refusée",
          "[vfs][tirage]") {
  TempDirectory client;
  client.WriteText(kTableOnDisk, std::string(kSource));
  auto vfs = Vfs::Mount({.root = client.path()});
  REQUIRE(vfs.has_value());
  auto items = ro::vfs::ReadPackageItems(*vfs);
  REQUIRE(items.has_value());
  CHECK(items->Find(604) != nullptr);

  client.WriteText(kTableOnDisk, "version: 1\npackages: [pas, une, table]\n");
  auto refused = Vfs::Mount({.root = client.path()});
  REQUIRE(refused.has_value());
  CHECK_FALSE(ro::vfs::ReadPackageItems(*refused).has_value());
}
