#include "ro/formats/monster_skill_info.h"

#include <cstdint>
#include <span>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

using ro::formats::MonsterSkillInfo;

namespace {

MonsterSkillInfo Parsed(std::string_view text) {
  const std::span<const std::uint8_t> bytes(reinterpret_cast<const std::uint8_t*>(text.data()),
                                            text.size());
  return MonsterSkillInfo::Parse(bytes);
}

// La forme du fichier de l'installation, réduite à ce qui compte : une espèce à
// deux compétences, un geste qui tourne, un commentaire et des blancs de fin.
constexpr std::string_view kTable = R"(<?xml version="1.0" encoding="euc-kr"?>
<!-- <ignore><_1><action>9</action></_1></ignore> -->
<Monster_Action_Table>
	<bigfoot>
		<_194>
			<action>1</action>
			<loopType>0</loopType>
		</_194>
		<_179>
			<action>7</action>
			<loopType>0</loopType>
		</_179>
	</bigfoot>
	<high_orc>
		<_195>
			<action>6</action>
			<loopType>1</loopType>
		</_195>
	</high_orc>
	<bigfoot>
		<_28>
			<action>5</action>
			<loopType>0</loopType>
		</_28>
	</bigfoot>
	<stainer>
		<_201>
			<loopType>1</loopType>
		</_201>
		<_201>
			<action>2</action>
		</_201>
	</stainer>
</Monster_Action_Table>
)";

}  // namespace

TEST_CASE("Gestes de créature : l'espèce et la compétence nomment l'action", "[formats]") {
  const MonsterSkillInfo table = Parsed(kTable);
  const auto gesture = table.Find("bigfoot", 179);
  REQUIRE(gesture.has_value());
  CHECK(gesture->action == 7);
  CHECK(gesture->loop_type == 0);
  const auto looping = table.Find("high_orc", 195);
  REQUIRE(looping.has_value());
  CHECK(looping->action == 6);
  CHECK(looping->loop_type == 1);
  CHECK_FALSE(table.Find("bigfoot", 1).has_value());
  CHECK_FALSE(table.Find("poring", 194).has_value());
}

TEST_CASE("Gestes de créature : le nom se cherche en minuscules", "[formats]") {
  const MonsterSkillInfo table = Parsed(kTable);
  CHECK(table.Find("BIGFOOT", 194).has_value());
  CHECK(table.Find("High_Orc", 195).has_value());
}

TEST_CASE("Gestes de créature : le premier bloc et la première entrée gagnent", "[formats]") {
  const MonsterSkillInfo table = Parsed(kTable);
  // 🔴 LE SECOND BLOC D'UNE ESPÈCE NE SE LIT PAS : le client s'arrête au premier.
  CHECK_FALSE(table.Find("bigfoot", 28).has_value());
  // Et une entrée sans action masque celle qui la suit sous la même balise.
  CHECK_FALSE(table.Find("stainer", 201).has_value());
}

TEST_CASE("Gestes de créature : un fichier illisible donne une table vide", "[formats]") {
  CHECK(Parsed("").size() == 0);
  CHECK(Parsed("<autre><bigfoot><_1><action>2</action></_1></bigfoot></autre>").size() == 0);
  CHECK_FALSE(
      Parsed("<Monster_Action_Table><bigfoot><_1><action>2").Find("bigfoot", 1).has_value());
}
