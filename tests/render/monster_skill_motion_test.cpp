#include "ro/render/monster_skill_motion.h"

#include <cstdint>
#include <span>
#include <string_view>
#include <utility>

#include <catch2/catch_test_macros.hpp>

using ro::render::IsMonsterOrNpcClass;
using ro::render::MonsterCastCadenceSteps;
using ro::render::SkillUsePosedByTable;

TEST_CASE("Geste de créature : les classes que la table concerne", "[render]") {
  CHECK_FALSE(IsMonsterOrNpcClass(1000));
  CHECK(IsMonsterOrNpcClass(1001));
  CHECK(IsMonsterOrNpcClass(3998));
  CHECK_FALSE(IsMonsterOrNpcClass(3999));
  CHECK_FALSE(IsMonsterOrNpcClass(4001));
  CHECK_FALSE(IsMonsterOrNpcClass(20020));
  CHECK(IsMonsterOrNpcClass(20021));
  CHECK(IsMonsterOrNpcClass(32042));
  CHECK_FALSE(IsMonsterOrNpcClass(32043));
}

TEST_CASE("Geste de créature : l'emploi de la neuvième passe par la table pour tous", "[render]") {
  constexpr std::uint32_t kNovice = 0;
  CHECK(SkillUsePosedByTable(kNovice, ro::render::kSkillPosedByTableForAll));
  CHECK_FALSE(SkillUsePosedByTable(kNovice, 28));
  CHECK(SkillUsePosedByTable(1002, 28));
}

TEST_CASE("Geste de créature : un métier joueur n'a pas de geste", "[render]") {
  ro::render::SpriteCatalog catalog;
  const std::string_view table =
      "<Monster_Action_Table><novice><_9><action>2</action></_9></novice></Monster_Action_Table>";
  catalog.monster_skill_actions = ro::formats::MonsterSkillInfo::Parse(
      std::span(reinterpret_cast<const std::uint8_t*>(table.data()), table.size()));
  CHECK_FALSE(ro::render::MonsterSkillMotionFor(catalog, 0, 9).has_value());
}

namespace {

std::span<const std::uint8_t> Bytes(std::string_view text) {
  return {reinterpret_cast<const std::uint8_t*>(text.data()), text.size()};
}

}  // namespace

TEST_CASE("Geste de créature : le nom de ressource trouve l'espèce, quelle que soit sa casse",
          "[render]") {
  // DES NOMS RÉELS de la table des métiers de l'installation — une ligne de
  // chaque casse qu'elle écrit — et les entrées de la table des gestes qui
  // leur répondent. Le client abaisse le nom avant de chercher : « Bigfoot »,
  // « HIGH_ORC » et « maya_puple » trouvent tous leur bloc.
  constexpr std::string_view kNoms =
      "version: 1\nclasses:\n  1032: {sprite: \"Vorit\"}\n  1060: {sprite: \"Bigfoot\"}\n"
      "  1213: {sprite: \"HIGH_ORC\"}\n  1289: {sprite: \"maya_puple\"}\n"
      "  20061: {sprite: \"BAPHOMET_I\"}\n";
  constexpr std::string_view kGestes =
      "<Monster_Action_Table>"
      "<bigfoot><_179><action>7</action><loopType>0</loopType></_179></bigfoot>"
      "<high_orc><_195><action>6</action><loopType>1</loopType></_195></high_orc>"
      "<maya_puple><_194><action>1</action><loopType>0</loopType></_194></maya_puple>"
      "<verit><_194><action>1</action><loopType>0</loopType></_194></verit>"
      "<baphomet_i><_661><action>6</action><loopType>0</loopType></_661></baphomet_i>"
      "</Monster_Action_Table>";
  ro::render::SpriteCatalog catalog;
  REQUIRE(
      ro::render::ParseSpriteTableText(ro::render::SpriteTableFile::kClasses, kNoms, catalog.tables)
          .has_value());
  catalog.monster_skill_actions = ro::formats::MonsterSkillInfo::Parse(Bytes(kGestes));

  CHECK(ro::render::NonPlayerResourceName(catalog, 1060) == "Bigfoot");
  const auto bigfoot = ro::render::MonsterSkillMotionFor(catalog, 1060, 179);
  REQUIRE(bigfoot.has_value());
  CHECK(bigfoot->animation == 6);
  CHECK_FALSE(bigfoot->loops);
  const auto orc = ro::render::MonsterSkillMotionFor(catalog, 1213, 195);
  REQUIRE(orc.has_value());
  CHECK(orc->loops);
  CHECK(ro::render::MonsterSkillMotionFor(catalog, 1289, 194).has_value());
  // La plage des classes étendues se nomme par la même table.
  CHECK(ro::render::MonsterSkillMotionFor(catalog, 20061, 661).has_value());
  // 🔴 UNE ENTRÉE MORTE CHEZ LE CLIENT LE RESTE ICI : l'espèce 1032 s'appelle
  // « Vorit », et le bloc « verit » ne répond à personne.
  CHECK_FALSE(ro::render::MonsterSkillMotionFor(catalog, 1032, 194).has_value());
}

TEST_CASE("Geste de créature : la métamorphose règle sa cadence sur l'incantation", "[render]") {
  CHECK_FALSE(MonsterCastCadenceSteps(ro::render::kMetamorphosisSkill, 299).has_value());
  REQUIRE(MonsterCastCadenceSteps(ro::render::kMetamorphosisSkill, 1'000).has_value());
  // Division ENTIÈRE : mille millisecondes font trois pas, et non 3,33.
  CHECK(*MonsterCastCadenceSteps(ro::render::kMetamorphosisSkill, 1'000) == 3.0F);
  CHECK_FALSE(MonsterCastCadenceSteps(194, 1'000).has_value());
}
