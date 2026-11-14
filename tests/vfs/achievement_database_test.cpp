// LA BASE DES SUCCÈS DU SERVEUR, lue dans la table qu'exporte
// scripts/yaml_tables/achievements.py.

#include "ro/vfs/achievement_database.h"

#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/vfs/vfs.h"

#include "support/temp_directory.h"

using ro::test_support::TempDirectory;
using ro::vfs::AchievementDatabase;
using ro::vfs::ServerAchievement;
using ro::vfs::Vfs;

namespace {

constexpr const char* kDatabaseOnDisk = "data/yaml_files/achievements.yml";

constexpr std::string_view kServer = R"(
version: 1
mode: Prerenewal
achievements: {
  128001: {name: "Specialist", score: 20, targets: [[0, 10], [2, 3]],
           reward: {item: 22876, title: 1001, script: true}, dependents: [128000, 120001]},
  110001: {name: "A fan of this polarity", score: 10, targets: [],
           reward: {item: 0, title: 0, script: false}, dependents: []},
}
)";

[[nodiscard]] AchievementDatabase Read(std::string_view text) {
  auto database = AchievementDatabase::Parse(text);
  REQUIRE(database.has_value());
  return std::move(*database);
}

}  // namespace

TEST_CASE("Base des succès : une entrée se lit champ par champ", "[vfs][succes]") {
  const AchievementDatabase database = Read(kServer);
  const ServerAchievement* read = database.Find(128001);
  REQUIRE(read != nullptr);
  CHECK(read->name == "Specialist");
  CHECK(read->score == 20);
  CHECK(read->targets[0] == 10U);
  CHECK_FALSE(read->targets[1].has_value());
  CHECK(read->targets[2] == 3U);
  CHECK(read->reward_item == 22876);
  CHECK(read->reward_title == 1001);
  CHECK(read->reward_script);
  CHECK(read->dependents == std::vector<std::uint32_t>{128000, 120001});
}

TEST_CASE("Base des succès : sans récompense ni objectif, tout est nul", "[vfs][succes]") {
  const AchievementDatabase database = Read(kServer);
  const ServerAchievement* read = database.Find(110001);
  REQUIRE(read != nullptr);
  CHECK(read->reward_item == 0);
  CHECK(read->reward_title == 0);
  CHECK_FALSE(read->reward_script);
  for (const std::optional<std::uint32_t>& target : read->targets) {
    CHECK_FALSE(target.has_value());
  }
  CHECK(read->dependents.empty());
}

TEST_CASE("Base des succès : rangée par numéro, un inconnu ne rend rien", "[vfs][succes]") {
  const AchievementDatabase database = Read(kServer);
  REQUIRE(database.entries().size() == 2);
  CHECK(database.entries().front().achievement_id == 110001);
  CHECK(database.Find(424242) == nullptr);
}

TEST_CASE("Base des succès : un texte mal formé est refusé entier", "[vfs][succes]") {
  // Une autre version, un champ absent, un objectif hors de 0 à 9, un nombre
  // négatif : la table entière se refuse plutôt que de montrer un succès faux.
  CHECK_FALSE(AchievementDatabase::Parse("version: 2\nachievements: {}\n").has_value());
  CHECK_FALSE(AchievementDatabase::Parse("version: 1\n").has_value());
  CHECK_FALSE(AchievementDatabase::Parse(R"(version: 1
achievements: {1: {name: a, score: 0, targets: [], dependents: []}}
)")
                  .has_value());
  CHECK_FALSE(AchievementDatabase::Parse(R"(version: 1
achievements: {1: {name: a, score: 0, targets: [[10, 1]],
                   reward: {item: 0, title: 0, script: false}, dependents: []}}
)")
                  .has_value());
  CHECK_FALSE(AchievementDatabase::Parse(R"(version: 1
achievements: {1: {name: a, score: -5, targets: [],
                   reward: {item: 0, title: 0, script: false}, dependents: []}}
)")
                  .has_value());
  CHECK_FALSE(AchievementDatabase::Parse("[ouvert").has_value());
}

TEST_CASE("Base des succès : absente de l'installation, la lecture le dit", "[vfs][succes]") {
  TempDirectory client;
  // Une installation qui porte autre chose, mais pas la table.
  client.WriteText("data/yaml_files/pets.yml", "version: 1\npets: {}\n");
  {
    auto vfs = Vfs::Mount({.root = client.path()});
    REQUIRE(vfs.has_value());
    CHECK_FALSE(AchievementDatabase::Load(*vfs).has_value());
  }
  client.WriteText(kDatabaseOnDisk, kServer);
  auto vfs = Vfs::Mount({.root = client.path()});
  REQUIRE(vfs.has_value());
  auto database = AchievementDatabase::Load(*vfs);
  REQUIRE(database.has_value());
  CHECK(database->entries().size() == 2);
}
