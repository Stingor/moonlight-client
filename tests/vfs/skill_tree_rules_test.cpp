// LES ARBRES DE COMPÉTENCES DU SERVEUR, lus dans la table qu'exporte
// scripts/yaml_tables/skill_trees.py.

#include "ro/vfs/skill_tree_rules.h"

#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/vfs/vfs.h"

#include "support/temp_directory.h"

using ro::test_support::TempDirectory;
using ro::vfs::ServerSkillRule;
using ro::vfs::SkillTreeRules;
using ro::vfs::Vfs;

namespace {

constexpr const char* kRulesOnDisk = "data/yaml_files/skill_trees.yml";

constexpr std::uint32_t kSwordman = 1;
constexpr std::uint32_t kNovice = 0;

// Les métiers dans le désordre : la lecture les range.
constexpr std::string_view kServer = R"(
version: 1
mode: Prerenewal
skill_free: true
jobs:
  1:
    - {id: 3, name: SM_TWOHAND, max: 10, needs: [[2, 1]], base_level: 50, job_level: 40}
    - {id: 142, name: NV_FIRSTAID, max: 1, by_points: false}
  0:
    - {id: 1, name: NV_BASIC, max: 9}
guild:
  - {id: 10000, name: GD_APPROVAL, max: 1}
  - {id: 10010, name: GD_BATTLEORDER, max: 1, needs: [[10000, 1], [10004, 2]]}
)";

[[nodiscard]] SkillTreeRules Read(std::string_view text) {
  auto rules = SkillTreeRules::Parse(text);
  REQUIRE(rules.has_value());
  return std::move(*rules);
}

}  // namespace

TEST_CASE("Arbres du serveur : une compétence se lit champ par champ", "[vfs][grimoire]") {
  const SkillTreeRules rules = Read(kServer);
  CHECK(rules.skill_free());
  REQUIRE(rules.jobs().size() == 2);
  CHECK(rules.jobs()[0].job == kNovice);
  CHECK(rules.jobs()[1].job == kSwordman);

  const ServerSkillRule& two_hand = rules.jobs()[1].skills[0];
  CHECK(two_hand.skill_id == 3);
  CHECK(two_hand.max_level == 10);
  REQUIRE(two_hand.needs.size() == 1);
  CHECK(two_hand.needs[0].first == 2);
  CHECK(two_hand.needs[0].second == 1);
  CHECK(two_hand.base_level == 50);
  CHECK(two_hand.job_level == 40);
  CHECK(two_hand.by_points);
  CHECK_FALSE(rules.jobs()[1].skills[1].by_points);

  REQUIRE(rules.guild().size() == 2);
  CHECK(rules.guild()[1].needs.size() == 2);
}

TEST_CASE("Arbres du serveur : un champ omis prend son défaut", "[vfs][grimoire]") {
  // ⚠ L'EXPORT N'ÉCRIT QUE CE QUI DIFFÈRE DU DÉFAUT. Lire un champ absent comme
  // une erreur refuserait la table entière ; le lire autrement que le défaut
  // fermerait l'apprentissage par points à presque tout.
  const SkillTreeRules rules = Read(kServer);
  const ServerSkillRule& basic = rules.jobs()[0].skills[0];
  CHECK(basic.needs.empty());
  CHECK(basic.base_level == 0);
  CHECK(basic.job_level == 0);
  CHECK(basic.by_points);
}

TEST_CASE("Arbres du serveur : un texte mal formé est refusé entier", "[vfs][grimoire]") {
  CHECK_FALSE(
      SkillTreeRules::Parse("version: 2\nskill_free: false\njobs: {}\nguild: []\n").has_value());
  // Sans `skill_free`, on ne saurait pas si les prérequis comptent.
  CHECK_FALSE(SkillTreeRules::Parse("version: 1\njobs: {}\nguild: []\n").has_value());
  CHECK_FALSE(SkillTreeRules::Parse("version: 1\nskill_free: false\nguild: []\n").has_value());
  // Un rang maximal absent, un prérequis qui n'est pas une paire, un nombre
  // négatif.
  CHECK_FALSE(SkillTreeRules::Parse(R"(version: 1
skill_free: false
jobs: {1: [{id: 3}]}
guild: []
)")
                  .has_value());
  CHECK_FALSE(SkillTreeRules::Parse(R"(version: 1
skill_free: false
jobs: {1: [{id: 3, max: 10, needs: [[2]]}]}
guild: []
)")
                  .has_value());
  CHECK_FALSE(SkillTreeRules::Parse(R"(version: 1
skill_free: false
jobs: {1: [{id: 3, max: -1}]}
guild: []
)")
                  .has_value());
  CHECK_FALSE(SkillTreeRules::Parse("[ouvert").has_value());
}

TEST_CASE("Arbres du serveur : absents de l'installation, la lecture le dit", "[vfs][grimoire]") {
  TempDirectory client;
  client.WriteText("data/yaml_files/pets.yml", "version: 1\npets: {}\n");
  {
    auto vfs = Vfs::Mount({.root = client.path()});
    REQUIRE(vfs.has_value());
    CHECK_FALSE(SkillTreeRules::Load(*vfs).has_value());
  }
  client.WriteText(kRulesOnDisk, kServer);
  auto vfs = Vfs::Mount({.root = client.path()});
  REQUIRE(vfs.has_value());
  auto rules = SkillTreeRules::Load(*vfs);
  REQUIRE(rules.has_value());
  CHECK(rules->jobs().size() == 2);
}
