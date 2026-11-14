// L'ARBRE DES COMPÉTENCES MONTÉ DE SES DEUX TABLES, comme le client le monte :
// `skill_info.yml` et `skill_layout.yml` posés dans un dossier client
// temporaire, lus par le VFS.

#include "game/skill_tables.h"

#include <span>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/vfs/vfs.h"

#include "support/temp_directory.h"

namespace {

using ro::test_support::TempDirectory;

constexpr const char* kInfoOnDisk = "data/yaml_files/skill_info.yml";
constexpr const char* kLayoutOnDisk = "data/yaml_files/skill_layout.yml";
constexpr std::uint16_t kBash = 5;
constexpr std::uint16_t kMagnum = 7;
constexpr std::uint32_t kSwordman = 1;

constexpr std::string_view kInfo = R"yaml(version: 1
skills: {
  5: {name: "SM_BASH", label: "Bash", sp: [8, 15], range: [1, 1], separate_levels: true, description: ["Bash - ID:5"]},
  7: {name: "SM_MAGNUM", label: "Magnum Break", sp: [30], scale: [[5, 5], [0, 0], [7, 7]]},
}
)yaml";

constexpr std::string_view kLayout = R"yaml(version: 1
grids:
  1: {10: 7, 3: 5}
inherit:
  1: 0
origins:
tab_names:
  0: ["1st", "2nd"]
)yaml";

}  // namespace

TEST_CASE("Tables des compétences : l'arbre et les descriptions se montent", "[app][grimoire]") {
  TempDirectory client;
  client.WriteText(kInfoOnDisk, kInfo);
  client.WriteText(kLayoutOnDisk, kLayout);
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path()});
  REQUIRE(vfs.has_value());
  auto tables = moonclient::LoadSkillTables(*vfs);
  REQUIRE(tables.has_value());
  CHECK_FALSE(tables->layout_error.has_value());

  const ro::sim::SkillNode* bash = tables->tree.Find(kBash);
  REQUIRE(bash != nullptr);
  CHECK(bash->idname == "SM_BASH");
  CHECK(bash->display_name == "Bash");
  CHECK(bash->sp_by_level == std::vector<std::uint16_t>{8, 15});
  CHECK(bash->range_cells_by_level == std::vector<std::uint16_t>{1, 1});
  CHECK(bash->level_matters);

  const ro::sim::SkillNode* magnum = tables->tree.Find(kMagnum);
  REQUIRE(magnum != nullptr);
  CHECK_FALSE(magnum->level_matters);
  CHECK(ro::sim::SkillScaleAt(*magnum, 3).x == 7);
  CHECK_FALSE(ro::sim::SkillScaleAt(*magnum, 2).Declared());

  const std::span<const ro::sim::SkillSlot> slots = tables->tree.SlotsOfJob(kSwordman);
  REQUIRE(slots.size() == 2);
  CHECK(slots[0].skill_id == kBash);
  CHECK(tables->tree.InheritanceOfJob(kSwordman).size() == 2);
  REQUIRE(tables->tree.TabNamesOfJob(kSwordman).size() == 2);

  REQUIRE(tables->words.Of(kBash).size() == 1);
  CHECK(tables->words.Of(kMagnum).empty());
}

TEST_CASE("Tables des compétences : sans disposition, les fiches tiennent", "[app][grimoire]") {
  // ⚠ LES DEUX TABLES ÉCHOUENT SÉPARÉMENT : sans grille, les noms, les icônes et
  // les descriptions servent encore aux raccourcis et aux messages.
  TempDirectory client;
  client.WriteText(kInfoOnDisk, kInfo);
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path()});
  REQUIRE(vfs.has_value());
  auto tables = moonclient::LoadSkillTables(*vfs);
  REQUIRE(tables.has_value());
  CHECK(tables->layout_error.has_value());
  CHECK(tables->tree.Find(kBash) != nullptr);
  CHECK(tables->tree.SlotsOfJob(kSwordman).empty());
  CHECK(tables->words.skill_count() == 1);
}

TEST_CASE("Tables des compétences : sans fiches, rien", "[app][grimoire]") {
  TempDirectory client;
  client.WriteText(kLayoutOnDisk, kLayout);
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path()});
  REQUIRE(vfs.has_value());
  CHECK_FALSE(moonclient::LoadSkillTables(*vfs).has_value());
}
