// LA DISPOSITION DE LA FENÊTRE DES COMPÉTENCES, lue dans un `skill_layout.yml`
// écrit ici même : aucune archive, aucun asset.

#include "ro/vfs/skill_layout.h"

#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

namespace {

// La forme que `scripts/yaml_tables/skill_info.py` écrit, métiers dans le
// désordre : le lecteur les range.
constexpr std::string_view kLayout = R"yaml(version: 1
grids:
  7: {0: 60, 1: 61}
  1: {8: 3, 1: 2, 3: 5}
inherit:
  7: 1
  1: 0
origins:
  4060: 4054
tab_names:
  23: ["Novice", "2nd", "3rd"]
  0: ["1st", "2nd", "3rd", "4th"]
)yaml";

}  // namespace

TEST_CASE("Disposition : grilles, chaînes et onglets se lisent, rangés", "[vfs][grimoire]") {
  auto layout = ro::vfs::ParseSkillLayout(kLayout);
  REQUIRE(layout.has_value());

  REQUIRE(layout->grids.size() == 2);
  CHECK(layout->grids[0].job == 1);
  // Rangées par case : c'est l'ordre du dessin.
  CHECK(layout->grids[0].slots ==
        std::vector<ro::vfs::SkillLayoutSlot>{{.position = 1, .skill_id = 2},
                                              {.position = 3, .skill_id = 5},
                                              {.position = 8, .skill_id = 3}});
  CHECK(layout->grids[1].job == 7);

  REQUIRE(layout->inherit.size() == 2);
  CHECK(layout->inherit[0].job == 1);
  CHECK(layout->inherit[0].parent == 0);
  REQUIRE(layout->origins.size() == 1);
  CHECK(layout->origins[0].parent == 4054);

  REQUIRE(layout->tab_names.size() == 2);
  CHECK(layout->tab_names[0].job == 0);
  CHECK(layout->tab_names[1].names == std::vector<std::string>{"Novice", "2nd", "3rd"});
}

TEST_CASE("Disposition : une section vide se lit vide", "[vfs][grimoire]") {
  // Le script écrit l'en-tête de chaque section même sans rien dessous : YAML
  // le lit nul.
  auto layout = ro::vfs::ParseSkillLayout("version: 1\ngrids:\ninherit:\norigins:\ntab_names:\n");
  REQUIRE(layout.has_value());
  CHECK(layout->grids.empty());
  CHECK(layout->tab_names.empty());
}

TEST_CASE("Disposition : un texte mal formé est refusé entier", "[vfs][grimoire]") {
  // Une case sans compétence.
  CHECK_FALSE(ro::vfs::ParseSkillLayout("version: 1\ngrids:\n  1: {0: 0}\n"));
  // Un métier négatif.
  CHECK_FALSE(ro::vfs::ParseSkillLayout("version: 1\ninherit:\n  -1: 0\n"));
  // Une section qui n'est pas une table.
  CHECK_FALSE(ro::vfs::ParseSkillLayout("version: 1\ngrids: [1, 2]\n"));
  // Une autre version.
  CHECK_FALSE(ro::vfs::ParseSkillLayout("version: 2\n"));
}
