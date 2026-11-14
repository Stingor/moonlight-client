#include "ro/vfs/pet_tables.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/vfs/vfs.h"

#include "support/temp_directory.h"

using ro::test_support::TempDirectory;
using ro::vfs::ParsePetDatabase;
using ro::vfs::ParsePetInfo;
using ro::vfs::PetTables;
using ro::vfs::ReadPetTables;
using ro::vfs::Vfs;

namespace {

constexpr const char* kPetInfoOnDisk = "data/yaml_files/pet_info.yml";
constexpr const char* kPetDatabaseOnDisk = "data/yaml_files/pets.yml";

constexpr std::uint32_t kPoring = 1002;
constexpr std::uint32_t kMastering = 1090;
constexpr std::uint32_t kStrange = 2057;
constexpr std::uint32_t kOtherStrange = 2081;
constexpr std::uint32_t kPoringEgg = 9001;
constexpr std::uint32_t kMasteringEgg = 9069;
constexpr std::uint32_t kSharedBall = 6221;
constexpr std::uint32_t kAppleJuice = 531;
constexpr std::uint32_t kYggdrasilLeaf = 610;
constexpr std::uint32_t kUnripeApple = 619;
constexpr std::uint32_t kPoringBag = 10013;
constexpr std::uint32_t kChonchonMask = 10002;

// La forme que le script écrit : un en-tête de commentaires, des familiers dans
// le désordre, un œuf que deux familiers partagent.
constexpr std::string_view kDatabase = R"yaml(# Les familiers que le serveur connaît
version: 1
mode: Prerenewal
pets: {
  2081: {egg: 6221, food: 0, tame: 0, accessory: 0, auto_feed: false, evolutions: []},
  1090: {egg: 9069, food: 531, tame: 619, accessory: 0, auto_feed: true, evolutions: []},
  1002: {egg: 9001, food: 531, tame: 619, accessory: 10013, auto_feed: false, evolutions: [{egg: 9069, materials: [[610, 10], [619, 3]]}]},
  2057: {egg: 6221, food: 0, tame: 0, accessory: 0, auto_feed: false, evolutions: []}
}
)yaml";

// La forme que `pet_info.py` écrit : un .act peut porter un nom coréen.
constexpr std::string_view kPetInfo = R"yaml(# Le nom d'espèce des familiers
version: 1
mode: Prerenewal
species:
  1002: "Poring"
  1090: "Mastering"
accessories:
  10002: "chocho_방독면.act"
  10013: "poring_bag.act"
)yaml";

[[nodiscard]] PetTables Read(const TempDirectory& client) {
  auto vfs = Vfs::Mount({.root = client.path()});
  REQUIRE(vfs.has_value());
  return ReadPetTables(*vfs);
}

}  // namespace

TEST_CASE("Tables du pet : l'accessoire nomme le .act qui remplace le corps", "[vfs][pet]") {
  PetTables tables;
  REQUIRE(ParsePetInfo(kPetInfo, tables).has_value());
  CHECK(tables.IsPetAccessory(kPoringBag));
  CHECK(tables.AccessoryActOf(kPoringBag) == "poring_bag.act");
  CHECK(tables.AccessoryActOf(kChonchonMask) == "chocho_방독면.act");
  CHECK(tables.SpeciesOf(kPoring) == "Poring");
  CHECK(tables.SpeciesOf(kMastering) == "Mastering");
}

TEST_CASE("Tables du pet : un item que la table ignore n'est pas un accessoire", "[vfs][pet]") {
  PetTables tables;
  REQUIRE(ParsePetInfo(kPetInfo, tables).has_value());
  CHECK_FALSE(tables.IsPetAccessory(kPoringBag + 1));
  CHECK(tables.AccessoryActOf(kPoringBag + 1).empty());
  CHECK_FALSE(tables.IsPetAccessory(0));
  CHECK(tables.SpeciesOf(kStrange).empty());
}

TEST_CASE("Tables du pet : des noms mal formés sont refusés sans rien toucher", "[vfs][pet]") {
  for (const std::string_view broken : {
           std::string_view("version: 2\nspecies: {}\n"),
           std::string_view("version: 1\nspecies: [1, 2]\n"),
           std::string_view("version: 1\naccessories: 3\n"),
           std::string_view("version: 1\nspecies: {-1: \"Moins\"}\n"),
           std::string_view("version: 1\nspecies: {poring: \"Poring\"}\n"),
           std::string_view("version: 1\naccessories: {10013: [\"a.act\"]}\n"),
           std::string_view("pas: [du yaml"),
       }) {
    PetTables tables;
    tables.species_by_class[kPoring] = "Poring";
    CHECK_FALSE(ParsePetInfo(broken, tables).has_value());
    // Refusée, la table ne laisse rien à moitié.
    CHECK(tables.SpeciesOf(kPoring) == "Poring");
    CHECK(tables.accessory_act_by_item.empty());
  }
}

TEST_CASE("Tables du pet : les noms que le dépôt livre se lisent", "[vfs][pet]") {
  // Le dépôt, à deux dossiers au-dessus de cet essai : CMake compile chaque
  // source sous son chemin absolu.
  const std::filesystem::path repo = std::filesystem::path(__FILE__).parent_path() / ".." / "..";
  std::ifstream shipped(repo / "dist" / "data" / "yaml_files" / "pet_info.yml", std::ios::binary);
  REQUIRE(shipped.is_open());
  const std::string text((std::istreambuf_iterator<char>(shipped)),
                         std::istreambuf_iterator<char>());
  PetTables tables;
  REQUIRE(ParsePetInfo(text, tables).has_value());
  CHECK(tables.SpeciesOf(kPoring) == "Poring");
  CHECK(tables.IsPetAccessory(kPoringBag));
}

TEST_CASE("Tables du pet : la table du serveur que le dépôt livre se lit", "[vfs][pet]") {
  const std::filesystem::path repo = std::filesystem::path(__FILE__).parent_path() / ".." / "..";
  std::ifstream shipped(repo / "dist" / "data" / "yaml_files" / "pets.yml", std::ios::binary);
  REQUIRE(shipped.is_open());
  const std::string text((std::istreambuf_iterator<char>(shipped)),
                         std::istreambuf_iterator<char>());
  PetTables tables;
  REQUIRE(ParsePetDatabase(text, tables).has_value());
  // 628, « Well-Dried Bone », capture 1107, « Baby Desert Wolf », qui porte
  // 10003 ; 619, « Unripe Apple », capture le Poring.
  constexpr std::uint32_t kWellDriedBone = 628;
  constexpr std::uint32_t kBabyDesertWolf = 1107;
  constexpr std::uint32_t kHeadProtector = 10003;
  REQUIRE(tables.TamedBy(kWellDriedBone).size() == 1);
  CHECK(tables.TamedBy(kWellDriedBone)[0] == kBabyDesertWolf);
  REQUIRE_FALSE(tables.TamedBy(kUnripeApple).empty());
  CHECK(tables.TamedBy(kUnripeApple)[0] == kPoring);
  REQUIRE(tables.WornBy(kHeadProtector).size() == 1);
  CHECK(tables.WornBy(kHeadProtector)[0] == kBabyDesertWolf);
}

TEST_CASE("Tables du pet : sans pet_info.yml, ni espèce ni accessoire", "[vfs][pet]") {
  TempDirectory client;
  client.WriteText("data/marqueur.txt", "");

  const PetTables tables = Read(client);
  CHECK(tables.accessory_act_by_item.empty());
  CHECK(tables.species_by_class.empty());
}

TEST_CASE("Tables du pet : la table du serveur donne l'œuf, la nourriture et les évolutions",
          "[vfs][pet]") {
  PetTables tables;
  REQUIRE(ParsePetDatabase(kDatabase, tables).has_value());

  CHECK(tables.ClassOfEgg(kPoringEgg) == kPoring);
  CHECK(tables.FoodOf(kPoring) == kAppleJuice);
  CHECK(tables.FoodOf(kStrange) == 0);
  CHECK(tables.AutoFeedListed(kMasteringEgg));
  CHECK_FALSE(tables.AutoFeedListed(kPoringEgg));

  const ro::vfs::PetEvolutionRecipe* recipe = tables.RecipeOf(kPoringEgg, kMasteringEgg);
  REQUIRE(recipe != nullptr);
  REQUIRE(recipe->materials.size() == 2);
  // Les matériaux, dans l'ordre du fichier.
  CHECK(recipe->materials[0].item_id == kYggdrasilLeaf);
  CHECK(recipe->materials[0].amount == 10);
  CHECK(recipe->materials[1].item_id == kUnripeApple);
  CHECK(recipe->materials[1].amount == 3);
  // La cible se nomme par son œuf, et se retrouve par lui.
  CHECK(tables.ClassOfEgg(recipe->target_egg_id) == kMastering);
  CHECK(tables.EvolutionsOf(kMasteringEgg).empty());
}

TEST_CASE("Tables du pet : capture, nourriture et accessoire désignent leurs familiers",
          "[vfs][pet]") {
  PetTables tables;
  REQUIRE(ParsePetDatabase(kDatabase, tables).has_value());
  // Deux familiers du même objet, croissants quel que soit l'ordre du texte.
  const std::span<const std::uint32_t> tamed = tables.TamedBy(kUnripeApple);
  REQUIRE(tamed.size() == 2);
  CHECK(tamed[0] == kPoring);
  CHECK(tamed[1] == kMastering);
  // Une capture nulle n'est pas un objet de capture.
  CHECK(tables.TamedBy(0).empty());
  CHECK(tables.TamedBy(kAppleJuice).empty());
  CHECK(tables.FedBy(kAppleJuice) == std::vector<std::uint32_t>{kPoring, kMastering});
  // L'accessoire désigne qui le porte.
  const std::span<const std::uint32_t> worn = tables.WornBy(kPoringBag);
  REQUIRE(worn.size() == 1);
  CHECK(worn[0] == kPoring);
  CHECK(tables.WornBy(0).empty());
  // Ne rien manger n'est pas manger l'item zéro.
  CHECK(tables.FedBy(0).empty());
}

TEST_CASE("Tables du pet : un œuf partagé sort la plus petite classe", "[vfs][pet]") {
  PetTables tables;
  REQUIRE(ParsePetDatabase(kDatabase, tables).has_value());
  CHECK(tables.ClassOfEgg(kSharedBall) == kStrange);
  CHECK(kStrange < kOtherStrange);
}

TEST_CASE("Tables du pet : une table du serveur mal formée est refusée sans rien toucher",
          "[vfs][pet]") {
  for (const std::string_view broken : {
           std::string_view("version: 2\npets: {}\n"),
           std::string_view("version: 1\npets: [1, 2]\n"),
           std::string_view(
               "version: 1\npets: {1002: {egg: 9001, food: 531, tame: 0, accessory: 0, auto_feed: "
               "false}}\n"),
           // Sans objet de capture, ou sans accessoire : un champ manque.
           std::string_view("version: 1\npets: {1002: {egg: 9001, food: 531, accessory: 0, "
                            "auto_feed: false, evolutions: []}}\n"),
           std::string_view("version: 1\npets: {1002: {egg: 9001, food: 531, tame: 0, "
                            "auto_feed: false, evolutions: []}}\n"),
           std::string_view("version: 1\npets: {1002: {egg: 0, food: 531, tame: 0, accessory: 0, "
                            "auto_feed: false, "
                            "evolutions: []}}\n"),
           std::string_view("version: 1\npets: {1002: {egg: 9001, food: 531, tame: 0, accessory: "
                            "0, auto_feed: false, "
                            "evolutions: [{egg: 9069, materials: [[610]]}]}}\n"),
           std::string_view("version: 1\npets: {1002: {egg: 9001, food: 531, tame: 0, accessory: "
                            "0, auto_feed: false, "
                            "evolutions: [{egg: 0, materials: []}]}}\n"),
           std::string_view("pas: [du yaml"),
       }) {
    PetTables tables;
    tables.food_by_class[kPoring] = kAppleJuice;
    CHECK_FALSE(ParsePetDatabase(broken, tables).has_value());
    // Refusée, la table ne laisse rien à moitié.
    CHECK(tables.FoodOf(kPoring) == kAppleJuice);
    CHECK(tables.class_by_egg.empty());
  }
}

TEST_CASE("Tables du pet : les deux sources se lisent ensemble, et séparément", "[vfs][pet]") {
  TempDirectory client;
  client.WriteText(kPetInfoOnDisk, std::string(kPetInfo));
  client.WriteText(kPetDatabaseOnDisk, std::string(kDatabase));

  const PetTables both = Read(client);
  CHECK(both.SpeciesOf(both.ClassOfEgg(kPoringEgg)) == "Poring");
  CHECK(both.FoodOf(kPoring) == kAppleJuice);
  CHECK(both.RecipeOf(kPoringEgg, kMasteringEgg) != nullptr);
  CHECK(both.IsPetAccessory(kPoringBag));

  // Une table du serveur illisible ne coûte pas les espèces.
  client.WriteText(kPetDatabaseOnDisk, "version: 1\npets: [\n");
  const PetTables species_only = Read(client);
  CHECK(species_only.SpeciesOf(kPoring) == "Poring");
  CHECK(species_only.FoodOf(kPoring) == 0);
  CHECK(species_only.EvolutionsOf(kPoringEgg).empty());

  // Des noms illisibles ne coûtent pas la table du serveur.
  client.WriteText(kPetDatabaseOnDisk, std::string(kDatabase));
  client.WriteText(kPetInfoOnDisk, "version: 1\nspecies: [\n");
  const PetTables database_only = Read(client);
  CHECK(database_only.SpeciesOf(kPoring).empty());
  CHECK_FALSE(database_only.IsPetAccessory(kPoringBag));
  CHECK(database_only.FoodOf(kPoring) == kAppleJuice);
}

TEST_CASE("Tables du pet : sans la table du serveur, ni nourriture ni évolution", "[vfs][pet]") {
  TempDirectory client;
  client.WriteText(kPetInfoOnDisk, std::string(kPetInfo));

  const PetTables tables = Read(client);
  CHECK(tables.SpeciesOf(kPoring) == "Poring");
  CHECK(tables.ClassOfEgg(kPoringEgg) == 0);
  CHECK(tables.FoodOf(kPoring) == 0);
  CHECK(tables.auto_feed_eggs.empty());
  CHECK(tables.recipes_by_egg.empty());
}
