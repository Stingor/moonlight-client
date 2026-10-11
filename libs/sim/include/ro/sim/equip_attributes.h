#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace ro::sim {

// CE QU'APPORTE UNE PIÈCE, OU UN ENSEMBLE, qui entre ou sort de l'équipement.
//
// `ZC_MOONLIGHT_EQUIP_ATTRIBUTES 0x0F3D`, qui ne part qu'à un client ayant
// annoncé savoir le lire. Le serveur joue le script de la pièce — et ceux de ses
// cartes et de ses options aléatoires — en le RELEVANT au lieu de l'appliquer :
// chaque entrée est une commande `bonus`..`bonus5` telle qu'écrite, ou l'apport
// propre de la pièce (son ATK, sa DEF, son raffinage).
//
// ⚠ LES NUMÉROS SONT CEUX DU SERVEUR, à la lettre (`enum _sp`) : une entrée se
// relit contre le script qui l'a produite.

// Ce que l'entrée raconte.
enum class EquipAttributesKind : std::uint8_t {
  kPiece = 0,  // l'identifiant est celui de la pièce
  kCombo = 1,  // l'identifiant est celui de l'ensemble
};

// Le niveau d'une entrée qui n'est pas une commande bonus : l'apport propre de
// la pièce, hors script. `values[0]` en porte la valeur.
inline constexpr std::uint8_t kEquipBonusOwnLevel = 0;
// Le plus d'arguments qu'une commande porte : `bonus5`.
inline constexpr std::size_t kEquipBonusValueCount = 5;

struct EquipBonus {
  // 0 pour l'apport propre, 1 à 5 pour `bonus`..`bonus5`.
  std::uint8_t level = 0;
  // Le bType, voir `EquipBonusType`.
  std::uint16_t type = 0;
  // Les arguments, dans l'ordre du script ; un nom de skill y est déjà un
  // identifiant, et l'argument STATUT d'un bonus d'effet y est un EFST.
  std::array<std::int32_t, kEquipBonusValueCount> values = {};
};

struct EquipAttributes {
  // Vrai à la pose ou quand un ensemble se forme ; faux au retrait ou quand il
  // se rompt.
  bool equipped = false;
  EquipAttributesKind kind = EquipAttributesKind::kPiece;
  std::uint32_t id = 0;
  std::vector<EquipBonus> bonuses;
};

// LES bTYPES QUE LE CLIENT SAIT DIRE, miroir de `enum _sp` du serveur
// (src/map/map.hpp, et src/custom/sp_enum.inc pour la plage 2500). Un type
// absent d'ici se dit quand même, par son numéro.
enum class EquipBonusType : std::uint16_t {
  kMaxHp = 6,
  kMaxSp = 8,
  kStr = 13,
  kAgi = 14,
  kVit = 15,
  kInt = 16,
  kDex = 17,
  kLuk = 18,
  kAtk1 = 41,
  kAtk2 = 42,
  kMatk1 = 43,
  kDef1 = 45,
  kDef2 = 46,
  kMdef1 = 47,
  kMdef2 = 48,
  kHit = 49,
  kFlee1 = 50,
  kFlee2 = 51,
  kCritical = 52,
  kAspd = 53,
  kUnbreakable = 60,
  kAttackRange = 1000,
  kAtkEle = 1001,
  kDefEle = 1002,
  kCastRate = 1003,
  kMaxHpRate = 1004,
  kMaxSpRate = 1005,
  kSpRate = 1006,
  kAddEle = 1007,
  kAddRace = 1008,
  kAddSize = 1009,
  kSubEle = 1010,
  kSubRace = 1011,
  kAddEff = 1012,
  kResEff = 1013,
  kBaseAtk = 1014,
  kAspdRate = 1015,
  kHpRecovRate = 1016,
  kSpRecovRate = 1017,
  kSpeedRate = 1018,
  kCriticalDef = 1019,
  kNearAtkDef = 1020,
  kLongAtkDef = 1021,
  kDoubleRate = 1022,
  kDoubleAddRate = 1023,
  kSkillHeal = 1024,
  kMatkRate = 1025,
  kIgnoreDefEle = 1026,
  kIgnoreDefRace = 1027,
  kAtkRate = 1028,
  kSpeedAddRate = 1029,
  kSpRegenRate = 1030,
  kMagicAtkDef = 1031,
  kMiscAtkDef = 1032,
  kIgnoreMdefEle = 1033,
  kIgnoreMdefRace = 1034,
  kMagicAddEle = 1035,
  kMagicAddRace = 1036,
  kMagicAddSize = 1037,
  kPerfectHitRate = 1038,
  kPerfectHitAddRate = 1039,
  kCriticalRate = 1040,
  kGetZenyNum = 1041,
  kAddGetZenyNum = 1042,
  kAddDamageClass = 1043,
  kAddMagicDamageClass = 1044,
  kAddDefMonster = 1045,
  kAddMdefMonster = 1046,
  kAddMonsterDropItem = 1047,
  kDefRatioAtkEle = 1048,
  kDefRatioAtkRace = 1049,
  kUnbreakableGarment = 1050,
  kHitRate = 1051,
  kFleeRate = 1052,
  kFlee2Rate = 1053,
  kDefRate = 1054,
  kDef2Rate = 1055,
  kMdefRate = 1056,
  kMdef2Rate = 1057,
  kSplashRange = 1058,
  kSplashAddRange = 1059,
  kAutoSpell = 1060,
  kHpDrainRate = 1061,
  kSpDrainRate = 1062,
  kShortWeaponDamageReturn = 1063,
  kLongWeaponDamageReturn = 1064,
  kWeaponComaEle = 1065,
  kWeaponComaRace = 1066,
  kAddEff2 = 1067,
  kBreakWeaponRate = 1068,
  kBreakArmorRate = 1069,
  kAddStealRate = 1070,
  kMagicDamageReturn = 1071,
  kAllStats = 1073,
  kAgiVit = 1074,
  kAgiDexStr = 1075,
  kPerfectHide = 1076,
  kNoKnockback = 1077,
  kClassChange = 1078,
  kHpDrainValue = 1079,
  kSpDrainValue = 1080,
  kWeaponAtk = 1081,
  kWeaponDamageRate = 1082,
  kDelayRate = 1083,
  kHpDrainValueRace = 1084,
  kSpDrainValueRace = 1085,
  kIgnoreMdefRaceRate = 1086,
  kIgnoreDefRaceRate = 1087,
  kSkillHeal2 = 1088,
  kAddEffOnSkill = 1089,
  kAddHealRate = 1090,
  kAddHeal2Rate = 1091,
  kRestartFullRecover = 2000,
  kNoCastCancel = 2001,
  kNoSizeFix = 2002,
  kNoMagicDamage = 2003,
  kNoWeaponDamage = 2004,
  kNoGemstone = 2005,
  kNoCastCancel2 = 2006,
  kNoMiscDamage = 2007,
  kUnbreakableWeapon = 2008,
  kUnbreakableArmor = 2009,
  kUnbreakableHelm = 2010,
  kUnbreakableShield = 2011,
  kLongAtkRate = 2012,
  kCritAtkRate = 2013,
  kCriticalAddRace = 2014,
  kNoRegen = 2015,
  kAddEffWhenHit = 2016,
  kAutoSpellWhenHit = 2017,
  kSkillAtk = 2018,
  kUnstripable = 2019,
  kAutoSpellOnSkill = 2020,
  kSpGainValue = 2021,
  kHpRegenRate = 2022,
  kHpLossRate = 2023,
  kAddRace2 = 2024,
  kHpGainValue = 2025,
  kSubSize = 2026,
  kHpDrainValueClass = 2027,
  kAddItemHealRate = 2028,
  kSpDrainValueClass = 2029,
  kExpAddRace = 2030,
  kSpGainRace = 2031,
  kSubRace2 = 2032,
  kUnbreakableShoes = 2033,
  kUnstripableWeapon = 2034,
  kUnstripableArmor = 2035,
  kUnstripableHelm = 2036,
  kUnstripableShield = 2037,
  kIntravision = 2038,
  kAddMonsterDropItemGroup = 2039,
  kSpLossRate = 2040,
  kAddSkillBlow = 2041,
  kSpVanishRate = 2042,
  kMagicSpGainValue = 2043,
  kMagicHpGainValue = 2044,
  kAddMonsterIdDropItem = 2045,
  kEmatk = 2046,
  kComaClass = 2047,
  kComaRace = 2048,
  kSkillUseSpRate = 2049,
  kSkillCooldown = 2050,
  kSkillFixedCast = 2051,
  kSkillVariableCast = 2052,
  kFixCastRate = 2053,
  kVarCastRate = 2054,
  kSkillUseSp = 2055,
  kMagicAtkEle = 2056,
  kAddFixedCast = 2057,
  kAddVariableCast = 2058,
  kSetDefRace = 2059,
  kSetMdefRace = 2060,
  kHpVanishRate = 2061,
  kIgnoreDefClass = 2062,
  kDefRatioAtkClass = 2063,
  kAddClass = 2064,
  kSubClass = 2065,
  kMagicAddClass = 2066,
  kWeaponComaClass = 2067,
  kIgnoreMdefClassRate = 2068,
  kExpAddClass = 2069,
  kAddClassDropItem = 2070,
  kAddClassDropItemGroup = 2071,
  kAddMaxWeight = 2072,
  kAddItemGroupHealRate = 2073,
  kHpVanishRaceRate = 2074,
  kSpVanishRaceRate = 2075,
  kAbsorbDmgMaxHp = 2076,
  kSubSkill = 2077,
  kSubDefEle = 2078,
  kStateNoRecoverRace = 2079,
  kCriticalRangeAtk = 2080,
  kMagicAddRace2 = 2081,
  kIgnoreMdefRace2Rate = 2082,
  kWeaponAtkRate = 2083,
  kWeaponMatkRate = 2084,
  kDropAddRace = 2085,
  kDropAddClass = 2086,
  kNoMadoFuel = 2087,
  kIgnoreDefClassRate = 2088,
  kRegenPercentHp = 2089,
  kRegenPercentSp = 2090,
  kSkillDelay = 2091,
  kNoWalkDelay = 2092,
  kLongSpGainValue = 2093,
  kLongHpGainValue = 2094,
  kShortAtkRate = 2095,
  kMagicSubSize = 2096,
  kCritDefRate = 2097,
  kMagicSubDefEle = 2098,
  kReduceDamageReturn = 2099,
  kAddItemSpHealRate = 2100,
  kAddItemGroupSpHealRate = 2101,
  kWeaponSubSize = 2102,
  kAbsorbDmgMaxHp2 = 2103,
  kIgnoreResRaceRate = 2104,
  kIgnoreMresRaceRate = 2105,
  kNoAmmo = 2500,
  kNoBottle = 2501,
  kNoItem = 2502,
  kNoZeny = 2503,
  kDelayRate2 = 2504,
  kAspd2 = 2505,
  kMagnusAllRace = 2506,
};

}  // namespace ro::sim
