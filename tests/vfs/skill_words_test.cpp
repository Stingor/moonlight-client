// CE QU'UNE COMPÉTENCE FAIT, lu dans une table écrite ici même : aucune archive,
// aucun asset. La forme est celle de `skill_info.yml`, balisage compris.

#include "ro/vfs/skill_words.h"

#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/vfs/skill_info.h"

namespace {

constexpr std::uint16_t kBasicSkill = 1;  // NV_BASIC
constexpr std::uint16_t kHeal = 28;       // AL_HEAL

constexpr std::string_view kTable = R"yaml(version: 1
skills: {
  1: {name: "NV_BASIC", description: ["Basic Skill - ID:1", "Max Level: 9", "Description: ^777777Basic interface skills.^000000"]},
  28: {name: "AL_HEAL", label: "Heal", description: ["Heal - ID:28", "Max Level: 10", "14^CC3399Requirement: Faith 10, Demon Bane 5^000000", "Description: ^777777Restores target's ^CC6600HP^777777.", "restored.^000000"]},
  999: {name: "SANS_TEXTE"},
}
)yaml";

[[nodiscard]] ro::vfs::SkillWords ReadWords(std::vector<ro::vfs::SkillInfoRecord>& records) {
  auto read = ro::vfs::ParseSkillInfo(kTable);
  REQUIRE(read.has_value());
  records = std::move(*read);
  return ro::vfs::SkillWords::FromRecords(records);
}

}  // namespace

TEST_CASE("Descriptions : chaque compétence garde SES lignes", "[vfs][grimoire]") {
  std::vector<ro::vfs::SkillInfoRecord> records;
  const ro::vfs::SkillWords words = ReadWords(records);

  const auto heal = words.Of(kHeal);
  REQUIRE(heal.size() == 5);
  CHECK(heal[0] == "Heal - ID:28");
  CHECK(heal[1] == "Max Level: 10");
  CHECK(heal[4] == "restored.^000000");

  // ⚠ ET CELLES DE SA VOISINE NE DÉBORDENT PAS : toutes les lignes vivent dans un
  // seul tampon, et une vue mal bornée montrerait la description d'à côté.
  const auto basic = words.Of(kBasicSkill);
  REQUIRE(basic.size() == 3);
  CHECK(basic[0] == "Basic Skill - ID:1");
  CHECK(basic[2] == "Description: ^777777Basic interface skills.^000000");

  // Les lignes ont QUITTÉ les fiches : un méga-octet et demi ne se copie pas.
  CHECK(records[1].description.empty());
}

TEST_CASE("Descriptions : le balisage du jeu est CONSERVÉ", "[vfs][grimoire]") {
  // 🔴 LE RETIRER ICI DÉCIDERAIT À LA PLACE DU DESSIN. Les couleurs et les liens
  // sont lus par `ro/ui/rich_text.h`, qui en fait une disposition.
  std::vector<ro::vfs::SkillInfoRecord> records;
  const ro::vfs::SkillWords words = ReadWords(records);
  const auto heal = words.Of(kHeal);
  REQUIRE(heal.size() > 2);
  CHECK(heal[2].find("^CC3399") != std::string::npos);
  CHECK(heal[2].find("^000000") != std::string::npos);
}

TEST_CASE("Descriptions : une compétence sans texte n'en a pas", "[vfs][grimoire]") {
  std::vector<ro::vfs::SkillInfoRecord> records;
  const ro::vfs::SkillWords words = ReadWords(records);
  // ⚠ UNE FICHE SANS LIGNE NE COMPTE PAS : elle ne dit rien de plus qu'une
  // absente, et la garder ferait croire à un texte perdu en chemin.
  CHECK(words.skill_count() == 2);
  CHECK(words.Of(999).empty());

  // Le serveur accorde des compétences que le client ne décrit pas : c'est un cas
  // NORMAL, et il rend une liste vide plutôt qu'un refus.
  CHECK(words.Of(4242).empty());
}
