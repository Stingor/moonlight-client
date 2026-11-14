// LA FICHE DE CHAQUE COMPÉTENCE, lue dans la forme stricte de `skill_info.yml`,
// écrite ici même : aucune archive, aucun asset.

#include "ro/vfs/skill_info.h"

#include <string>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

namespace {

// La forme que `scripts/yaml_tables/skill_info.py` écrit : en-tête de
// commentaires, une ligne par compétence, des champs omis à leur défaut.
constexpr std::string_view kTable = R"yaml(# Un en-tête de commentaires.
#  skills : compétence -> une ligne
version: 1
skills: {
  5: {name: "SM_BASH", label: "Bash", sp: [8, 8, 8, 8, 8, 15, 15, 15, 15, 15], range: [1, 1], separate_levels: true, description: ["Bash - ID:5", "^777777\"Fort\"é^000000"]},
  7: {name: "SM_MAGNUM", label: "Magnum Break", sp: [30], scale: [[5, 5], [0, 0], [7, 7]]},
  40: {description: ["Sans fiche"]},
}
)yaml";

[[nodiscard]] std::string Refusal(std::string_view text) {
  auto read = ro::vfs::ParseSkillInfo(text);
  REQUIRE_FALSE(read.has_value());
  return read.error().detail;
}

}  // namespace

TEST_CASE("Fiches de compétence : chaque champ se lit, les absents à leur défaut",
          "[vfs][grimoire]") {
  auto read = ro::vfs::ParseSkillInfo(kTable);
  REQUIRE(read.has_value());
  REQUIRE(read->size() == 3);

  const ro::vfs::SkillInfoRecord& bash = (*read)[0];
  CHECK(bash.skill_id == 5);
  CHECK(bash.name == "SM_BASH");
  CHECK(bash.label == "Bash");
  REQUIRE(bash.sp_by_level.size() == 10);
  CHECK(bash.sp_by_level[5] == 15);
  CHECK(bash.range_by_level == std::vector<std::uint16_t>{1, 1});
  CHECK(bash.separate_levels);
  REQUIRE(bash.description.size() == 2);
  // Les échappements de JSON se résolvent, l'UTF-8 compris.
  CHECK(bash.description[1] == "^777777\"Fort\"\xC3\xA9^000000");

  const ro::vfs::SkillInfoRecord& magnum = (*read)[1];
  CHECK_FALSE(magnum.separate_levels);
  CHECK(magnum.range_by_level.empty());
  REQUIRE(magnum.scale_by_level.size() == 3);
  // Un rang qui ne déclare pas de zone garde sa place : le suivant ne glisse pas.
  CHECK(magnum.scale_by_level[1] == ro::vfs::SkillInfoScale{});
  CHECK(magnum.scale_by_level[2] == ro::vfs::SkillInfoScale{.x = 7, .y = 7});

  // Une description sans fiche : ni nom ni libellé.
  const ro::vfs::SkillInfoRecord& orphan = (*read)[2];
  CHECK(orphan.name.empty());
  CHECK(orphan.description == std::vector<std::string>{"Sans fiche"});
}

TEST_CASE("Fiches de compétence : toute autre forme se refuse, en disant sa ligne",
          "[vfs][grimoire]") {
  // Un champ que la forme ne connaît pas.
  CHECK(Refusal("version: 1\nskills: {\n  5: {nom: \"SM_BASH\"},\n}\n").find("ligne 3") !=
        std::string::npos);
  // Un texte sans guillemets : du YAML valide, mais pas la forme du script.
  CHECK_FALSE(ro::vfs::ParseSkillInfo("version: 1\nskills: {\n  5: {name: SM_BASH},\n}\n"));
  // Une zone qui n'est pas une paire.
  CHECK_FALSE(ro::vfs::ParseSkillInfo("version: 1\nskills: {\n  5: {scale: [[5]]},\n}\n"));
  // Un rang au-delà de seize bits.
  CHECK_FALSE(ro::vfs::ParseSkillInfo("version: 1\nskills: {\n  5: {sp: [70000]},\n}\n"));
  // Un booléen qui n'en est pas un.
  CHECK_FALSE(ro::vfs::ParseSkillInfo("version: 1\nskills: {\n  5: {separate_levels: oui},\n}\n"));
  // Une compétence répétée : la seconde fiche contredirait la première.
  CHECK(Refusal("version: 1\nskills: {\n  5: {},\n  5: {},\n}\n").find("répétée") !=
        std::string::npos);
  // Le numéro zéro n'est pas une compétence.
  CHECK_FALSE(ro::vfs::ParseSkillInfo("version: 1\nskills: {\n  0: {},\n}\n"));
  // Une autre version.
  CHECK_FALSE(ro::vfs::ParseSkillInfo("version: 2\nskills: {\n}\n"));
  // Du texte après la fermeture.
  CHECK_FALSE(ro::vfs::ParseSkillInfo("version: 1\nskills: {\n}\nautre: 1\n"));
}

TEST_CASE("Fiches de compétence : une table vide se lit vide", "[vfs][grimoire]") {
  auto read = ro::vfs::ParseSkillInfo("version: 1\nskills: {\n}\n");
  REQUIRE(read.has_value());
  CHECK(read->empty());
}
