// Les lignes de la case « Attributs d'équipement » : une par bonus, avec les
// libellés de la fiche, « (C) » devant celles d'un ensemble, « (P) » devant
// celles du familier, vertes à la pose et rouges au retrait.

#include "character/equip_attribute_lines.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/equip_attributes.h"

#include "support/monster_words_fixture.h"

using moonclient::ComposeEquipAttributeLines;
using moonclient::EquipAttributeNames;
using moonclient::EquipAttributesColor;
using moonclient::EquipBonusText;
using ro::sim::EquipAttributes;
using ro::sim::EquipAttributesKind;
using ro::sim::EquipBonus;
using ro::sim::EquipBonusType;

namespace {

constexpr std::int32_t kDragonRace = 9;
constexpr std::int32_t kSkillId = 5;

[[nodiscard]] EquipBonus BonusOf(std::uint8_t level, EquipBonusType type, std::int32_t first,
                                 std::int32_t second = 0) {
  return EquipBonus{
      .level = level, .type = static_cast<std::uint16_t>(type), .values = {first, second, 0, 0, 0}};
}

}  // namespace

TEST_CASE("Attributs : une pièce posée, une ligne par bonus", "[app][equip]") {
  const ro::vfs::ItemWords words = ro::test_support::EnglishMonsterWords();
  const EquipAttributeNames names{.words = &words};
  EquipAttributes told;
  told.equipped = true;
  told.kind = EquipAttributesKind::kPiece;
  told.bonuses = {BonusOf(0, EquipBonusType::kAtk1, 120), BonusOf(1, EquipBonusType::kStr, 5),
                  BonusOf(2, EquipBonusType::kAddRace, kDragonRace, 10),
                  BonusOf(1, EquipBonusType::kUnbreakableWeapon, 0),
                  // Une valeur nulle ne dit rien : pas de ligne.
                  BonusOf(1, EquipBonusType::kAgi, 0)};

  const std::vector<std::string> lines = ComposeEquipAttributeLines(told, names);
  REQUIRE(lines.size() == 4);
  CHECK(lines[0] == "ATK +120");
  CHECK(lines[1] == "STR +5");
  // Le libellé de la fiche, mot pour mot.
  CHECK(lines[2] == "Dégâts vs Dragon +10%");
  CHECK(lines[3] == "Arme incassable");
  CHECK(EquipAttributesColor(told.equipped) == moonclient::kEquipAttributesOnColor);
}

TEST_CASE("Attributs : un ensemble rompu, préfixé et rouge", "[app][equip]") {
  EquipAttributes told;
  told.equipped = false;
  told.kind = EquipAttributesKind::kCombo;
  told.bonuses = {BonusOf(1, EquipBonusType::kMaxHp, 300),
                  BonusOf(1, EquipBonusType::kVarCastRate, -10)};

  const std::vector<std::string> lines = ComposeEquipAttributeLines(told, EquipAttributeNames{});
  REQUIRE(lines.size() == 2);
  CHECK(lines[0] == "(C)PV max +300");
  // Une réduction s'écrit avec son signe.
  CHECK(lines[1] == "(C)Cast var. -10%");
  CHECK(EquipAttributesColor(told.equipped) == moonclient::kEquipAttributesOffColor);
}

TEST_CASE("Attributs : le familier préfixe ses lignes de « (P) »", "[app][equip]") {
  EquipAttributes told;
  told.equipped = false;
  told.kind = EquipAttributesKind::kPet;
  told.bonuses = {BonusOf(1, EquipBonusType::kLuk, 3), BonusOf(1, EquipBonusType::kMaxHp, 150)};
  const std::vector<std::string> lines = ComposeEquipAttributeLines(told, EquipAttributeNames{});
  REQUIRE(lines.size() == 2);
  CHECK(lines[0] == "(P)LUK +3");
  CHECK(lines[1] == "(P)PV max +150");
  CHECK(EquipAttributesColor(told.equipped) == moonclient::kEquipAttributesOffColor);
}

TEST_CASE("Attributs : une pièce retirée garde ses lignes, sans préfixe", "[app][equip]") {
  EquipAttributes told;
  told.equipped = false;
  told.kind = EquipAttributesKind::kPiece;
  told.bonuses = {BonusOf(0, EquipBonusType::kDef1, 4)};
  const std::vector<std::string> lines = ComposeEquipAttributeLines(told, EquipAttributeNames{});
  REQUIRE(lines.size() == 1);
  CHECK(lines[0] == "DEF +4");
}

TEST_CASE("Attributs : un bonus en pour cent ne dit son « % » qu'une fois", "[app][equip]") {
  const EquipAttributeNames names;
  constexpr std::int32_t kRatePercent = 5;
  // Le libellé de la fiche ne porte pas l'unité : la valeur la porte.
  CHECK(EquipBonusText(BonusOf(1, EquipBonusType::kMaxHpRate, kRatePercent), names) ==
        "PV max +5%");
  CHECK(EquipBonusText(BonusOf(1, EquipBonusType::kMaxSpRate, kRatePercent), names) ==
        "SP max +5%");
  CHECK(EquipBonusText(BonusOf(1, EquipBonusType::kAtkRate, kRatePercent), names) == "ATK +5%");
  CHECK(EquipBonusText(BonusOf(1, EquipBonusType::kMatkRate, kRatePercent), names) == "MATK +5%");
  // Et la case des attributs, qui passe par le même traducteur.
  EquipAttributes told;
  told.equipped = true;
  told.kind = EquipAttributesKind::kPiece;
  told.bonuses = {BonusOf(1, EquipBonusType::kMaxHp, 300),
                  BonusOf(1, EquipBonusType::kMaxHpRate, kRatePercent)};
  const std::vector<std::string> lines = ComposeEquipAttributeLines(told, names);
  REQUIRE(lines.size() == 2);
  CHECK(lines[0] == "PV max +300");
  CHECK(lines[1] == "PV max +5%");

  const EquipBonusType rates[] = {
      EquipBonusType::kMaxHpRate,     EquipBonusType::kMaxSpRate,     EquipBonusType::kAspdRate,
      EquipBonusType::kSpeedRate,     EquipBonusType::kHpRecovRate,   EquipBonusType::kSpRecovRate,
      EquipBonusType::kSpRate,        EquipBonusType::kAtkRate,       EquipBonusType::kMatkRate,
      EquipBonusType::kHitRate,       EquipBonusType::kFleeRate,      EquipBonusType::kCriticalRate,
      EquipBonusType::kDefRate,       EquipBonusType::kMdefRate,      EquipBonusType::kCastRate,
      EquipBonusType::kFixCastRate,   EquipBonusType::kDelayRate,     EquipBonusType::kAddHealRate,
      EquipBonusType::kAddHeal2Rate,  EquipBonusType::kCritAtkRate,   EquipBonusType::kShortAtkRate,
      EquipBonusType::kLongAtkRate,   EquipBonusType::kDoubleRate,    EquipBonusType::kCritDefRate,
      EquipBonusType::kWeaponAtkRate, EquipBonusType::kWeaponMatkRate};
  for (const EquipBonusType type : rates) {
    const std::string text = EquipBonusText(BonusOf(1, type, kRatePercent), names);
    INFO(text);
    CHECK(std::ranges::count(text, '%') == 1);
    CHECK(text.ends_with("+5%"));
  }
}

TEST_CASE("Attributs : compétences et types inconnus se disent par leur numéro", "[app][equip]") {
  const EquipAttributeNames names;
  // Sans arbre de compétences, le skill se nomme par son numéro.
  CHECK(EquipBonusText(BonusOf(2, EquipBonusType::kSkillAtk, kSkillId, 20), names) ==
        "Dégâts skill #5 +20%");
  // Un type que ce client ne connaît pas se montre quand même.
  constexpr std::uint16_t kUnheardOf = 9999;
  const EquipBonus odd{.level = 1, .type = kUnheardOf, .values = {3, 0, 0, 0, 0}};
  CHECK(EquipBonusText(odd, names) == "Bonus #9999 +3");
}
