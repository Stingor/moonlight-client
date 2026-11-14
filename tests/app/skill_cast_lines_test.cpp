// Les lignes du grimoire : le nom affiché d'un skill, ses phrases, et leur case.

#include "character/skill_cast_lines.h"

#include <cstdint>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/error.h"
#include "ro/sim/chat_line.h"
#include "ro/sim/skill_book.h"
#include "ro/sim/skill_tree.h"
#include "ro/sim/world.h"

namespace {

constexpr std::uint16_t kAcidTerror = 230;
constexpr std::uint32_t kSaidMs = 1000;

[[nodiscard]] ro::sim::KnownSkill AcidTerror(std::uint16_t targeting) {
  ro::sim::KnownSkill skill;
  skill.id = kAcidTerror;
  skill.level = 1;
  skill.targeting = targeting;
  skill.name = "AM_ACIDTERROR";
  return skill;
}

}  // namespace

TEST_CASE("Grimoire : le nom affiché passe avant le nom interne", "[app][grimoire]") {
  const ro::sim::KnownSkill skill = AcidTerror(ro::sim::kSkillOnEnemy);
  ro::sim::SkillNode node;
  node.id = kAcidTerror;
  node.idname = "AM_ACIDTERROR";
  node.display_name = "Acid Terror";
  CHECK(moonclient::SkillShownName(&node, skill) == "Acid Terror");

  // Sans tables, ou sans libellé : le nom interne, puis le numéro.
  CHECK(moonclient::SkillShownName(nullptr, skill) == "AM_ACIDTERROR");
  node.display_name.clear();
  CHECK(moonclient::SkillShownName(&node, skill) == "AM_ACIDTERROR");
  ro::sim::KnownSkill nameless = skill;
  nameless.name.clear();
  CHECK(moonclient::SkillShownName(nullptr, nameless) == "230");
}

TEST_CASE("Grimoire : la phrase d'armement dit ce qu'il faut désigner", "[app][grimoire]") {
  CHECK(moonclient::SkillArmedLine("Acid Terror", AcidTerror(ro::sim::kSkillOnEnemy)) ==
        "Acid Terror : choisissez une cible.");
  CHECK(moonclient::SkillArmedLine("Bomb", AcidTerror(ro::sim::kSkillOnGround)) ==
        "Bomb : choisissez une case.");
  CHECK(moonclient::SkillArmedLine("Remove Trap", AcidTerror(ro::sim::kSkillTrap)) ==
        "Remove Trap : choisissez un piège.");
  CHECK(moonclient::SkillLaunchedLine("Demonstration") == "Demonstration lancé.");
}

TEST_CASE("Grimoire : un échec d'envoi porte le nom et le motif", "[app][grimoire]") {
  const ro::core::Error why{.code = ro::core::ErrorCode::kInvalidArgument, .detail = "paquet"};
  const std::string reason = ro::core::Describe(why);
  CHECK(moonclient::SkillNotSentLine("Acid Terror", why) ==
        "Acid Terror n'est pas parti : " + reason);
  CHECK(moonclient::SkillApproachNotSentLine("Acid Terror", why) ==
        "Acid Terror : l'approche n'est pas partie : " + reason);
}

TEST_CASE("Grimoire : l'armement et le lancer vont dans la case Mes skills", "[app][grimoire]") {
  ro::sim::World world;
  moonclient::RecordSkillUse(world, "Acid Terror lancé.", kSaidMs);
  REQUIRE(world.chat_lines().size() == 1);
  const ro::sim::ChatLine& line = world.chat_lines()[0];
  CHECK(line.type == ro::sim::ChatType::kSkillUse);
  CHECK(line.kind == ro::sim::ChatKind::kServer);
  CHECK(line.speaker_id == 0);
  CHECK(line.text == "Acid Terror lancé.");
}
