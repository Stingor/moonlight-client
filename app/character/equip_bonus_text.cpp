// LE TEXTE D'UN BONUS D'ÉQUIPEMENT, tel que le serveur l'a relevé dans un
// script : un bType (`enum _sp`), son niveau (`bonus`..`bonus5`) et ses
// arguments.
//
// 🔴 LA FICHE D'ABORD. Un bonus qui a sa rangée dans la fiche du personnage —
// un apport plat, une cible, une compétence, un objet — y est REJOUÉ, seul, et
// la rangée que la fiche en tire donne la ligne : même libellé, même unité. Ce
// fichier ne nomme lui-même que ce que la fiche ne montre pas — les six
// caractéristiques et les dérivées, que la fiche lit ailleurs, et les drapeaux.
//
// ⚠ UN TYPE INCONNU SE DIT QUAND MÊME, par son numéro et sa dernière valeur :
// le taire ferait disparaître un attribut que la pièce porte vraiment.

#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/i18n.h"
#include "ro/sim/player_bonuses.h"
#include "ro/ui/character_window.h"

#include "character/character_bonus_rows.h"
#include "character/equip_attribute_lines.h"
#include "world/entity_traits.h"

namespace moonclient {
namespace {

using Type = ro::sim::EquipBonusType;
using Unit = ro::ui::BonusUnit;
using ro::sim::ConditionalBonusKind;
using ro::sim::ItemBonusKind;
using ro::sim::PlayerBonuses;
using ro::sim::SkillBonusKind;

// La place d'une valeur écrite, et celle d'un libellé composé.
constexpr std::size_t kValueRoom = 32;
constexpr std::size_t kLabelRoom = 160;
// Ce que vaut un bonus « tout ou rien » rangé parmi les pour cent : la fiche
// replie de même le masque d'« ignore DEF ».
constexpr std::int32_t kWholePercent = 100;
// Le niveau de `bonus`, la commande à un seul argument.
constexpr std::uint8_t kSingleArgumentLevel = 1;
// Le poids que `bAddMaxWeight` ajoute se compte en dixièmes.
constexpr std::int32_t kWeightTenths = 10;

// « libellé +12 % », ou rien pour une valeur nulle.
[[nodiscard]] std::string Valued(std::string_view label, std::int32_t value, Unit unit) {
  if (value == 0) {
    return {};
  }
  const ro::ui::CharacterBonusRow row{.label = label, .value = value, .unit = unit};
  char written[kValueRoom] = {};
  ro::ui::WriteBonusValue(written, sizeof(written), row);
  return std::string(label) + " " + written;
}

[[nodiscard]] std::string FirstRow(const std::vector<ro::ui::CharacterBonusRow>& rows) {
  if (rows.empty()) {
    return {};
  }
  return Valued(rows.front().label, rows.front().value, rows.front().unit);
}

// ── LA FICHE, REJOUÉE SUR UN SEUL BONUS ─────────────────────────────────────

template <typename Set>
[[nodiscard]] std::string Sheet(Set set) {
  PlayerBonuses one;
  one.told = true;
  set(one);
  std::vector<ro::ui::CharacterBonusRow> rows;
  BuildBonusRows(one, rows);
  return FirstRow(rows);
}

[[nodiscard]] const ro::vfs::ItemWords& WordsOf(const EquipAttributeNames& names) {
  static const ro::vfs::ItemWords kNoWords;
  return names.words != nullptr ? *names.words : kNoWords;
}

[[nodiscard]] std::string Conditional(const EquipAttributeNames& names, ConditionalBonusKind kind,
                                      std::int32_t index, std::int32_t value) {
  PlayerBonuses one;
  one.told = true;
  one.conditional.push_back(ro::sim::ConditionalBonus{.code = static_cast<std::uint16_t>(kind),
                                                      .index = static_cast<std::int16_t>(index),
                                                      .value = value});
  std::vector<std::string> labels;
  std::vector<ro::ui::CharacterBonusRow> rows;
  BuildConditionalRows(one, WordsOf(names), labels, rows);
  return FirstRow(rows);
}

[[nodiscard]] std::string Skill(const EquipAttributeNames& names, ro::sim::SkillBonus entry) {
  PlayerBonuses one;
  one.told = true;
  one.skills.push_back(entry);
  std::vector<std::string> labels;
  std::vector<ro::ui::CharacterBonusRow> rows;
  BuildSkillRows(one, names.skills, names.status_of, labels, rows);
  return FirstRow(rows);
}

[[nodiscard]] std::string SkillOf(const EquipAttributeNames& names, SkillBonusKind kind,
                                  std::int32_t skill_id, std::int32_t value) {
  return Skill(names, ro::sim::SkillBonus{.code = static_cast<std::uint16_t>(kind),
                                          .skill_id = static_cast<std::uint16_t>(skill_id),
                                          .value = value});
}

[[nodiscard]] std::string AutoSpell(const EquipAttributeNames& names, SkillBonusKind kind,
                                    std::int32_t skill_id, std::int32_t level, std::int32_t rate,
                                    std::int32_t trigger) {
  return Skill(names,
               ro::sim::SkillBonus{.code = static_cast<std::uint16_t>(kind),
                                   .skill_id = static_cast<std::uint16_t>(skill_id),
                                   .level = static_cast<std::int16_t>(level),
                                   .value = rate,
                                   .triggered_by_skill = static_cast<std::uint16_t>(trigger)});
}

[[nodiscard]] std::string Drop(const EquipAttributeNames& names, ItemBonusKind kind,
                               std::int32_t item_id, std::int32_t rate) {
  PlayerBonuses one;
  one.told = true;
  one.items.push_back(ro::sim::ItemBonus{.code = static_cast<std::uint16_t>(kind),
                                         .item_id = static_cast<std::uint32_t>(item_id),
                                         .rate = rate});
  std::vector<std::string> labels;
  std::vector<ro::ui::CharacterBonusRow> rows;
  BuildItemRows(one, names.item_of, labels, rows);
  return FirstRow(rows);
}

// ── CE QUE LA FICHE NE NOMME PAS ────────────────────────────────────────────

[[nodiscard]] std::string StatusName(const EquipAttributeNames& names, std::int32_t effect_id) {
  if (names.status_of) {
    const std::string_view named = names.status_of(static_cast<std::uint32_t>(effect_id));
    if (!named.empty()) {
      return std::string(named);
    }
  }
  char numbered[kValueRoom] = {};
  std::snprintf(numbered, sizeof(numbered), ro::core::Tr("statut #%d"), effect_id);
  return numbered;
}

[[nodiscard]] std::string SkillName(const EquipAttributeNames& names, std::int32_t skill_id) {
  if (names.skills != nullptr) {
    const ro::sim::SkillNode* node = names.skills->Find(static_cast<std::uint16_t>(skill_id));
    if (node != nullptr && !node->display_name.empty()) {
      return node->display_name;
    }
  }
  char numbered[kValueRoom] = {};
  std::snprintf(numbered, sizeof(numbered), ro::core::Tr("skill #%d"), skill_id);
  return numbered;
}

[[nodiscard]] std::string ElementOf(const EquipAttributeNames& names, std::int32_t index) {
  return std::string(ElementName(WordsOf(names), static_cast<std::int16_t>(index)));
}

[[nodiscard]] std::string RaceOf(const EquipAttributeNames& names, std::int32_t index) {
  return std::string(RaceName(WordsOf(names), static_cast<std::int16_t>(index)));
}

[[nodiscard]] std::string ClassOf(std::int32_t index) {
  return std::string(MonsterClassName(static_cast<std::int16_t>(index)));
}

// « verbe cible ».
[[nodiscard]] std::string Against(std::string_view verb, const std::string& target) {
  return std::string(verb) + " " + target;
}

// Un libellé composé autour d'un nombre ou d'un nom.
[[nodiscard]] std::string Composed(const char* format, std::int32_t number) {
  char label[kLabelRoom] = {};
  std::snprintf(label, sizeof(label), format, number);
  return label;
}

[[nodiscard]] std::string Composed(const char* format, const std::string& name) {
  char label[kLabelRoom] = {};
  std::snprintf(label, sizeof(label), format, name.c_str());
  return label;
}

// Le numéro d'un type que ce client ne sait pas dire, avec sa dernière valeur.
[[nodiscard]] std::string Unknown(const ro::sim::EquipBonus& bonus) {
  const std::int32_t last = bonus.level == 0 ? bonus.values[0] : bonus.values[bonus.level - 1];
  return Valued(Composed(ro::core::Tr("Bonus #%d"), static_cast<std::int32_t>(bonus.type)), last,
                Unit::kFlat);
}

// L'apport propre de la pièce, hors script.
[[nodiscard]] std::string Own(const ro::sim::EquipBonus& bonus) {
  const std::int32_t value = bonus.values[0];
  switch (static_cast<Type>(bonus.type)) {
    case Type::kAtk1:
      return Valued(ro::core::Tr("ATK"), value, Unit::kFlat);
    case Type::kAtk2:
      return Valued(ro::core::Tr("ATK (raffinage)"), value, Unit::kFlat);
    case Type::kMatk1:
      return Valued(ro::core::Tr("MATK"), value, Unit::kFlat);
    case Type::kDef1:
      return Valued(ro::core::Tr("DEF"), value, Unit::kFlat);
    case Type::kDef2:
      return Valued(ro::core::Tr("DEF (raffinage)"), value, Unit::kFlat);
    default:
      break;
  }
  return Unknown(bonus);
}

// `bonus bType,n` : ce que la fiche montre en apport plat.
[[nodiscard]] std::string OneOnSheet(const ro::sim::EquipBonus& bonus) {
  const std::int32_t n = bonus.values[0];
  switch (static_cast<Type>(bonus.type)) {
    case Type::kMaxHp:
      return Sheet([n](PlayerBonuses& one) { one.hp_add = n; });
    case Type::kMaxSp:
      return Sheet([n](PlayerBonuses& one) { one.sp_add = n; });
    case Type::kAspd:
      return Sheet([n](PlayerBonuses& one) { one.aspd_add = n; });
    case Type::kVarCastRate:
    case Type::kCastRate:
      return Sheet([n](PlayerBonuses& one) { one.vcast_pct = n; });
    case Type::kFixCastRate:
      return Sheet([n](PlayerBonuses& one) { one.fcast_pct = n; });
    case Type::kAtkRate:
      return Sheet([n](PlayerBonuses& one) { one.atk_pct = n; });
    case Type::kMatkRate:
      return Sheet([n](PlayerBonuses& one) { one.matk_pct = n; });
    case Type::kShortWeaponDamageReturn:
      return Sheet([n](PlayerBonuses& one) { one.dmg_ret_melee = n; });
    case Type::kLongWeaponDamageReturn:
      return Sheet([n](PlayerBonuses& one) { one.dmg_ret_ranged = n; });
    case Type::kMagicDamageReturn:
      return Sheet([n](PlayerBonuses& one) { one.dmg_ret_magic = n; });
    case Type::kDoubleRate:
    case Type::kDoubleAddRate:
      return Sheet([n](PlayerBonuses& one) { one.double_pct = n; });
    case Type::kPerfectHitRate:
    case Type::kPerfectHitAddRate:
      return Sheet([n](PlayerBonuses& one) { one.perfect_hit = n; });
    case Type::kMaxHpRate:
      return Sheet([n](PlayerBonuses& one) { one.hp_pct = n; });
    case Type::kMaxSpRate:
      return Sheet([n](PlayerBonuses& one) { one.sp_pct = n; });
    case Type::kHpRecovRate:
      return Sheet([n](PlayerBonuses& one) { one.hp_regen_pct = n; });
    case Type::kSpRecovRate:
      return Sheet([n](PlayerBonuses& one) { one.sp_regen_pct = n; });
    case Type::kCriticalDef:
    case Type::kCritDefRate:
      return Sheet([n](PlayerBonuses& one) { one.crit_def_pct = n; });
    case Type::kHpGainValue:
      return Sheet([n](PlayerBonuses& one) { one.hp_on_kill = n; });
    case Type::kSpGainValue:
      return Sheet([n](PlayerBonuses& one) { one.sp_on_kill = n; });
    case Type::kUnbreakable:
      return Sheet([n](PlayerBonuses& one) { one.unbreak_pct = n; });
    case Type::kAddHealRate:
      return Sheet([n](PlayerBonuses& one) { one.heal_up_pct = n; });
    case Type::kDelayRate:
    case Type::kDelayRate2:
      return Sheet([n](PlayerBonuses& one) { one.delay_pct = n; });
    case Type::kAddVariableCast:
      return Sheet([n](PlayerBonuses& one) { one.add_vcast_ms = n; });
    case Type::kAddFixedCast:
      return Sheet([n](PlayerBonuses& one) { one.add_fcast_ms = n; });
    case Type::kAddStealRate:
      return Sheet([n](PlayerBonuses& one) { one.steal_pct = n; });
    case Type::kNearAtkDef:
      return Sheet([n](PlayerBonuses& one) { one.def_melee_pct = n; });
    case Type::kLongAtkDef:
      return Sheet([n](PlayerBonuses& one) { one.def_ranged_pct = n; });
    case Type::kMagicAtkDef:
      return Sheet([n](PlayerBonuses& one) { one.def_magic_pct = n; });
    case Type::kMiscAtkDef:
      return Sheet([n](PlayerBonuses& one) { one.def_misc_pct = n; });
    case Type::kSplashRange:
      return Sheet([n](PlayerBonuses& one) { one.splash = n; });
    case Type::kSplashAddRange:
      return Sheet([n](PlayerBonuses& one) { one.splash_add = n; });
    case Type::kBreakWeaponRate:
      return Sheet([n](PlayerBonuses& one) { one.break_weapon_pct = n; });
    case Type::kBreakArmorRate:
      return Sheet([n](PlayerBonuses& one) { one.break_armor_pct = n; });
    case Type::kClassChange:
      return Sheet([n](PlayerBonuses& one) { one.classchange_pct = n; });
    case Type::kReduceDamageReturn:
      return Sheet([n](PlayerBonuses& one) { one.dmg_ret_reduce = n; });
    case Type::kMagicHpGainValue:
      return Sheet([n](PlayerBonuses& one) { one.magic_hp_gain = n; });
    case Type::kMagicSpGainValue:
      return Sheet([n](PlayerBonuses& one) { one.magic_sp_gain = n; });
    case Type::kShortAtkRate:
      return Sheet([n](PlayerBonuses& one) { one.melee_pct = n; });
    case Type::kLongAtkRate:
      return Sheet([n](PlayerBonuses& one) { one.ranged_pct = n; });
    case Type::kCritAtkRate:
      return Sheet([n](PlayerBonuses& one) { one.crit_dmg_pct = n; });
    default:
      break;
  }
  return {};
}

// `bonus bType` sans valeur qui compte : un drapeau, qui se dit par son nom.
[[nodiscard]] std::string_view FlagOf(Type type) {
  switch (type) {
    case Type::kRestartFullRecover:
      return ro::core::Tr("Résurrection : PV et SP pleins");
    case Type::kNoCastCancel:
    case Type::kNoCastCancel2:
      return ro::core::Tr("Cast ininterruptible");
    case Type::kNoSizeFix:
      return ro::core::Tr("Dégâts sans malus de taille");
    case Type::kNoGemstone:
      return ro::core::Tr("Sans gemme");
    case Type::kUnbreakableWeapon:
      return ro::core::Tr("Arme incassable");
    case Type::kUnbreakableArmor:
      return ro::core::Tr("Armure incassable");
    case Type::kUnbreakableHelm:
      return ro::core::Tr("Coiffe incassable");
    case Type::kUnbreakableShield:
      return ro::core::Tr("Bouclier incassable");
    case Type::kUnbreakableGarment:
      return ro::core::Tr("Cape incassable");
    case Type::kUnbreakableShoes:
      return ro::core::Tr("Chaussures incassables");
    case Type::kUnstripable:
    case Type::kUnstripableWeapon:
    case Type::kUnstripableArmor:
    case Type::kUnstripableHelm:
    case Type::kUnstripableShield:
      return ro::core::Tr("Équipement inarrachable");
    case Type::kNoKnockback:
      return ro::core::Tr("Insensible au recul");
    case Type::kIntravision:
      return ro::core::Tr("Voit les cachés");
    case Type::kPerfectHide:
      return ro::core::Tr("Caché aux monstres qui détectent");
    case Type::kNoRegen:
      return ro::core::Tr("Sans régénération naturelle");
    case Type::kNoWalkDelay:
      return ro::core::Tr("Sans délai de marche aux coups");
    case Type::kNoMadoFuel:
      return ro::core::Tr("Mado Gear sans carburant");
    case Type::kCriticalRangeAtk:
      return ro::core::Tr("Critiques à distance");
    case Type::kNoAmmo:
      return ro::core::Tr("Sans munitions");
    case Type::kNoBottle:
      return ro::core::Tr("Sans bouteilles");
    case Type::kNoItem:
      return ro::core::Tr("Sans consommables");
    case Type::kNoZeny:
      return ro::core::Tr("Sans Zeny");
    case Type::kAspd2:
      return ro::core::Tr("ASPD alternative");
    case Type::kMagnusAllRace:
      return ro::core::Tr("Magnus Exorcismus sur toutes les races");
    default:
      break;
  }
  return {};
}

// `bonus bType,n`.
[[nodiscard]] std::string One(const ro::sim::EquipBonus& bonus, const EquipAttributeNames& names) {
  const std::int32_t n = bonus.values[0];
  const Type type = static_cast<Type>(bonus.type);
  switch (type) {
    case Type::kStr:
      return Valued(ro::core::Tr("STR"), n, Unit::kFlat);
    case Type::kAgi:
      return Valued(ro::core::Tr("AGI"), n, Unit::kFlat);
    case Type::kVit:
      return Valued(ro::core::Tr("VIT"), n, Unit::kFlat);
    case Type::kInt:
      return Valued(ro::core::Tr("INT"), n, Unit::kFlat);
    case Type::kDex:
      return Valued(ro::core::Tr("DEX"), n, Unit::kFlat);
    case Type::kLuk:
      return Valued(ro::core::Tr("LUK"), n, Unit::kFlat);
    case Type::kAllStats:
      return Valued(ro::core::Tr("Toutes les stats"), n, Unit::kFlat);
    case Type::kAgiVit:
      return Valued(ro::core::Tr("AGI, VIT"), n, Unit::kFlat);
    case Type::kAgiDexStr:
      return Valued(ro::core::Tr("AGI, DEX, STR"), n, Unit::kFlat);
    case Type::kAtk1:
    case Type::kAtk2:
    case Type::kBaseAtk:
    case Type::kWeaponAtk:
      return Valued(ro::core::Tr("ATK"), n, Unit::kFlat);
    case Type::kEmatk:
    case Type::kMatk1:
      return Valued(ro::core::Tr("MATK"), n, Unit::kFlat);
    case Type::kDef1:
    case Type::kDef2:
      return Valued(ro::core::Tr("DEF"), n, Unit::kFlat);
    case Type::kMdef1:
    case Type::kMdef2:
      return Valued(ro::core::Tr("MDEF"), n, Unit::kFlat);
    case Type::kHit:
      return Valued(ro::core::Tr("HIT"), n, Unit::kFlat);
    case Type::kFlee1:
      return Valued(ro::core::Tr("FLEE"), n, Unit::kFlat);
    case Type::kFlee2:
      return Valued(ro::core::Tr("Esq.P"), n, Unit::kFlat);
    case Type::kCritical:
      return Valued(ro::core::Tr("CRI"), n, Unit::kFlat);
    case Type::kHitRate:
      return Valued(ro::core::Tr("HIT"), n, Unit::kPercent);
    case Type::kFleeRate:
      return Valued(ro::core::Tr("FLEE"), n, Unit::kPercent);
    case Type::kFlee2Rate:
      return Valued(ro::core::Tr("Esq.P"), n, Unit::kPercent);
    case Type::kCriticalRate:
      return Valued(ro::core::Tr("CRI"), n, Unit::kPercent);
    case Type::kDefRate:
    case Type::kDef2Rate:
      return Valued(ro::core::Tr("DEF"), n, Unit::kPercent);
    case Type::kMdefRate:
    case Type::kMdef2Rate:
      return Valued(ro::core::Tr("MDEF"), n, Unit::kPercent);
    case Type::kAspdRate:
      return Valued(ro::core::Tr("ASPD"), n, Unit::kPercent);
    case Type::kWeaponAtkRate:
      return Valued(ro::core::Tr("ATK arme"), n, Unit::kPercent);
    case Type::kWeaponMatkRate:
      return Valued(ro::core::Tr("MATK arme"), n, Unit::kPercent);
    case Type::kWeaponDamageRate:
      return Valued(ro::core::Tr("Dégâts arme"), n, Unit::kPercent);
    case Type::kSpeedRate:
    case Type::kSpeedAddRate:
      return Valued(ro::core::Tr("Vitesse"), n, Unit::kPercent);
    case Type::kAttackRange:
      return Valued(ro::core::Tr("Portée"), n, Unit::kFlat);
    case Type::kSpRate:
      return Valued(ro::core::Tr("Coût SP"), n, Unit::kPercent);
    case Type::kAddHeal2Rate:
      return Valued(ro::core::Tr("Soin reçu"), n, Unit::kPercent);
    case Type::kHpDrainValue:
      return Valued(ro::core::Tr("PV par coup"), n, Unit::kFlat);
    case Type::kSpDrainValue:
      return Valued(ro::core::Tr("SP par coup"), n, Unit::kFlat);
    case Type::kLongHpGainValue:
      return Valued(ro::core::Tr("PV/kill à distance"), n, Unit::kFlat);
    case Type::kLongSpGainValue:
      return Valued(ro::core::Tr("SP/kill à distance"), n, Unit::kFlat);
    case Type::kAddMaxWeight:
      return Valued(ro::core::Tr("Poids max"), n / kWeightTenths, Unit::kFlat);
    case Type::kNoMagicDamage:
      return Valued(ro::core::Tr("Immunité magique"), n, Unit::kPercent);
    case Type::kNoWeaponDamage:
      return Valued(ro::core::Tr("Immunité physique"), n, Unit::kPercent);
    case Type::kNoMiscDamage:
      return Valued(ro::core::Tr("Immunité divers"), n, Unit::kPercent);
    case Type::kAtkEle:
      return Against(ro::core::Tr("Élément d'arme :"), ElementOf(names, n));
    case Type::kDefEle:
      return Against(ro::core::Tr("Élément d'armure :"), ElementOf(names, n));
    case Type::kIgnoreDefRace:
      return Conditional(names, ConditionalBonusKind::kIgnoreDefenseRace, n, kWholePercent);
    case Type::kIgnoreMdefRace:
      return Conditional(names, ConditionalBonusKind::kIgnoreMagicDefenseRace, n, kWholePercent);
    case Type::kIgnoreDefClass:
      return Conditional(names, ConditionalBonusKind::kIgnoreDefenseClass, n, kWholePercent);
    case Type::kIgnoreDefEle:
      return Against(ro::core::Tr("Ignore DEF vs"), ElementOf(names, n));
    case Type::kIgnoreMdefEle:
      return Against(ro::core::Tr("Ignore MDEF vs"), ElementOf(names, n));
    case Type::kDefRatioAtkEle:
      return Against(ro::core::Tr("Dégâts selon DEF vs"), ElementOf(names, n));
    case Type::kDefRatioAtkRace:
      return Against(ro::core::Tr("Dégâts selon DEF vs"), RaceOf(names, n));
    case Type::kDefRatioAtkClass:
      return Against(ro::core::Tr("Dégâts selon DEF vs"), ClassOf(n));
    default:
      break;
  }
  if (std::string sheet = OneOnSheet(bonus); !sheet.empty()) {
    return sheet;
  }
  if (const std::string_view flag = FlagOf(type); !flag.empty()) {
    return std::string(flag);
  }
  return Unknown(bonus);
}

// Une cible et sa magnitude : `bonus2 bType,cible,n`, ou un `bonus3` dont la
// magnitude est au milieu (`bSubEle,e,n,drapeau`).
[[nodiscard]] std::string Targeted(const ro::sim::EquipBonus& bonus,
                                   const EquipAttributeNames& names) {
  const std::int32_t target = bonus.values[0];
  const std::int32_t n = bonus.values[1];
  switch (static_cast<Type>(bonus.type)) {
    case Type::kAddEle:
      return Conditional(names, ConditionalBonusKind::kAddElement, target, n);
    case Type::kAddRace:
      return Conditional(names, ConditionalBonusKind::kAddRace, target, n);
    case Type::kAddSize:
      return Conditional(names, ConditionalBonusKind::kAddSize, target, n);
    case Type::kSubEle:
      return Conditional(names, ConditionalBonusKind::kSubElement, target, n);
    case Type::kSubRace:
      return Conditional(names, ConditionalBonusKind::kSubRace, target, n);
    case Type::kSubSize:
    case Type::kWeaponSubSize:
      return Conditional(names, ConditionalBonusKind::kSubSize, target, n);
    case Type::kMagicAddEle:
      return Conditional(names, ConditionalBonusKind::kMagicAddElement, target, n);
    case Type::kMagicAddRace:
      return Conditional(names, ConditionalBonusKind::kMagicAddRace, target, n);
    case Type::kMagicAddSize:
      return Conditional(names, ConditionalBonusKind::kMagicAddSize, target, n);
    case Type::kCriticalAddRace:
      return Conditional(names, ConditionalBonusKind::kCriticalRace, target, n);
    case Type::kIgnoreDefRaceRate:
      return Conditional(names, ConditionalBonusKind::kIgnoreDefenseRace, target, n);
    case Type::kIgnoreMdefRaceRate:
      return Conditional(names, ConditionalBonusKind::kIgnoreMagicDefenseRace, target, n);
    case Type::kSubDefEle:
      return Conditional(names, ConditionalBonusKind::kSubDefenseElement, target, n);
    case Type::kSubClass:
      return Conditional(names, ConditionalBonusKind::kSubClass, target, n);
    case Type::kSubRace2:
      return Conditional(names, ConditionalBonusKind::kSubRaceGroup, target, n);
    case Type::kExpAddRace:
      return Conditional(names, ConditionalBonusKind::kExperienceRace, target, n);
    case Type::kExpAddClass:
      return Conditional(names, ConditionalBonusKind::kExperienceClass, target, n);
    case Type::kDropAddRace:
      return Conditional(names, ConditionalBonusKind::kDropRace, target, n);
    case Type::kDropAddClass:
      return Conditional(names, ConditionalBonusKind::kDropClass, target, n);
    case Type::kComaRace:
      return Conditional(names, ConditionalBonusKind::kComaRace, target, n);
    case Type::kComaClass:
      return Conditional(names, ConditionalBonusKind::kComaClass, target, n);
    case Type::kIgnoreResRaceRate:
      return Conditional(names, ConditionalBonusKind::kIgnoreResistanceRace, target, n);
    case Type::kIgnoreMresRaceRate:
      return Conditional(names, ConditionalBonusKind::kIgnoreMagicResistanceRace, target, n);
    case Type::kMagicAddRace2:
      return Conditional(names, ConditionalBonusKind::kMagicAddRaceGroup, target, n);
    case Type::kIgnoreMdefRace2Rate:
      return Conditional(names, ConditionalBonusKind::kIgnoreMagicDefenseRaceGroup, target, n);
    case Type::kSpGainRace:
      return Conditional(names, ConditionalBonusKind::kSpGainRace, target, n);
    case Type::kIgnoreDefClassRate:
      return Conditional(names, ConditionalBonusKind::kIgnoreDefenseClass, target, n);
    case Type::kIgnoreMdefClassRate:
      return Conditional(names, ConditionalBonusKind::kIgnoreMagicDefenseClass, target, n);
    case Type::kAddClass:
      return Valued(Against(ro::core::Tr("Dégâts vs"), ClassOf(target)), n, Unit::kPercent);
    case Type::kMagicAddClass:
      return Valued(Against(ro::core::Tr("Dég. mag. vs"), ClassOf(target)), n, Unit::kPercent);
    case Type::kAddRace2:
      return Valued(
          Against(ro::core::Tr("Dégâts vs"), RaceGroupName(static_cast<std::int16_t>(target))), n,
          Unit::kPercent);
    case Type::kMagicSubSize:
      return Valued(
          Against(ro::core::Tr("Résist. mag. vs"),
                  std::string(SizeName(WordsOf(names), static_cast<std::int16_t>(target)))),
          n, Unit::kPercent);
    case Type::kMagicSubDefEle:
      return Valued(Against(ro::core::Tr("Résist. mag. vs"), ElementOf(names, target)), n,
                    Unit::kPercent);
    case Type::kMagicAtkEle:
      return Valued(Against(ro::core::Tr("Dég. mag. élément"), ElementOf(names, target)), n,
                    Unit::kPercent);
    case Type::kWeaponComaEle:
      return Valued(Against(ro::core::Tr("Coma vs"), ElementOf(names, target)), n,
                    Unit::kCentiPercent);
    case Type::kWeaponComaRace:
      return Valued(Against(ro::core::Tr("Coma vs"), RaceOf(names, target)), n,
                    Unit::kCentiPercent);
    case Type::kWeaponComaClass:
      return Valued(Against(ro::core::Tr("Coma vs"), ClassOf(target)), n, Unit::kCentiPercent);
    case Type::kHpDrainValueRace:
      return Valued(Against(ro::core::Tr("PV par coup vs"), RaceOf(names, target)), n, Unit::kFlat);
    case Type::kSpDrainValueRace:
      return Valued(Against(ro::core::Tr("SP par coup vs"), RaceOf(names, target)), n, Unit::kFlat);
    case Type::kHpDrainValueClass:
      return Valued(Against(ro::core::Tr("PV par coup vs"), ClassOf(target)), n, Unit::kFlat);
    case Type::kSpDrainValueClass:
      return Valued(Against(ro::core::Tr("SP par coup vs"), ClassOf(target)), n, Unit::kFlat);
    case Type::kAddDamageClass:
      return Valued(Composed(ro::core::Tr("Dégâts vs monstre #%d"), target), n, Unit::kPercent);
    case Type::kAddMagicDamageClass:
      return Valued(Composed(ro::core::Tr("Dég. mag. vs monstre #%d"), target), n, Unit::kPercent);
    case Type::kAddDefMonster:
      return Valued(Composed(ro::core::Tr("Réduc. vs monstre #%d"), target), n, Unit::kPercent);
    case Type::kAddMdefMonster:
      return Valued(Composed(ro::core::Tr("Réduc. mag. vs monstre #%d"), target), n,
                    Unit::kPercent);
    case Type::kAddItemHealRate:
    case Type::kAddItemSpHealRate:
      return Valued(Composed(ro::core::Tr("Soin de %s"),
                             names.item_of ? names.item_of(static_cast<std::uint32_t>(target))
                                           : std::string()),
                    n, Unit::kPercent);
    case Type::kAddItemGroupHealRate:
    case Type::kAddItemGroupSpHealRate:
      return Valued(Composed(ro::core::Tr("Soin du groupe #%d"), target), n, Unit::kPercent);
    case Type::kResEff:
      return Skill(names, ro::sim::SkillBonus{
                              .code = static_cast<std::uint16_t>(SkillBonusKind::kResistEffect),
                              .skill_id = static_cast<std::uint16_t>(target),
                              .value = n});
    case Type::kAddEff:
      return Skill(
          names, ro::sim::SkillBonus{.code = static_cast<std::uint16_t>(SkillBonusKind::kAddEffect),
                                     .skill_id = static_cast<std::uint16_t>(target),
                                     .value = n});
    case Type::kAddEffWhenHit:
      return Skill(names, ro::sim::SkillBonus{
                              .code = static_cast<std::uint16_t>(SkillBonusKind::kAddEffectWhenHit),
                              .skill_id = static_cast<std::uint16_t>(target),
                              .value = n});
    case Type::kAddEff2:
      return Valued(Composed(ro::core::Tr("Statut sur soi : %s"), StatusName(names, target)), n,
                    Unit::kCentiPercent);
    case Type::kSkillAtk:
      return SkillOf(names, SkillBonusKind::kSkillAttack, target, n);
    case Type::kSubSkill:
      return SkillOf(names, SkillBonusKind::kSubSkill, target, n);
    case Type::kSkillUseSpRate:
      return SkillOf(names, SkillBonusKind::kSkillSpRate, target, n);
    case Type::kSkillUseSp:
      return SkillOf(names, SkillBonusKind::kSkillSpCost, target, n);
    case Type::kVarCastRate:
    case Type::kCastRate:
      return SkillOf(names, SkillBonusKind::kSkillVariableCastRate, target, n);
    case Type::kFixCastRate:
      return SkillOf(names, SkillBonusKind::kSkillFixedCastRate, target, n);
    case Type::kSkillVariableCast:
      return SkillOf(names, SkillBonusKind::kSkillVariableCastMs, target, n);
    case Type::kSkillFixedCast:
      return SkillOf(names, SkillBonusKind::kSkillFixedCastMs, target, n);
    case Type::kSkillCooldown:
      return SkillOf(names, SkillBonusKind::kSkillCooldownMs, target, n);
    case Type::kSkillDelay:
      return SkillOf(names, SkillBonusKind::kSkillDelayRate, target, n);
    case Type::kSkillHeal:
      return SkillOf(names, SkillBonusKind::kSkillHealGiven, target, n);
    case Type::kSkillHeal2:
      return SkillOf(names, SkillBonusKind::kSkillHealTaken, target, n);
    case Type::kAddSkillBlow:
      return SkillOf(names, SkillBonusKind::kSkillKnockback, target, n);
    case Type::kAddMonsterDropItem:
      return Drop(names, ItemBonusKind::kAddDrop, target, bonus.values[bonus.level - 1]);
    case Type::kAddMonsterIdDropItem:
    case Type::kAddClassDropItem:
      return Drop(names, ItemBonusKind::kAddDrop, target, bonus.values[bonus.level - 1]);
    case Type::kAddMonsterDropItemGroup:
    case Type::kAddClassDropItemGroup:
      return Drop(names, ItemBonusKind::kAddDropGroup, target, bonus.values[bonus.level - 1]);
    default:
      break;
  }
  return {};
}

// Ce qui ne se range ni comme une cible ni comme un apport : les auto-sorts, les
// vols et les pertes à intervalle, les défenses fixées.
[[nodiscard]] std::string Several(const ro::sim::EquipBonus& bonus,
                                  const EquipAttributeNames& names) {
  const auto& v = bonus.values;
  switch (static_cast<Type>(bonus.type)) {
    case Type::kAutoSpell:
      return AutoSpell(names, SkillBonusKind::kAutoSpell, v[0], v[1], v[2], 0);
    case Type::kAutoSpellWhenHit:
      return AutoSpell(names, SkillBonusKind::kAutoSpellWhenHit, v[0], v[1], v[2], 0);
    case Type::kAutoSpellOnSkill:
      return AutoSpell(names, SkillBonusKind::kAutoSpellOnSkill, v[1], v[2], v[3], v[0]);
    case Type::kAddEffOnSkill:
      return Valued(Composed(ro::core::Tr("Inflige %s"), StatusName(names, v[1])) + " " +
                        Composed(ro::core::Tr("par %s"), SkillName(names, v[0])),
                    v[2], Unit::kCentiPercent);
    case Type::kHpDrainRate:
      return Sheet([&v](PlayerBonuses& one) { one.hp_drain_pct = v[1]; });
    case Type::kSpDrainRate:
      return Sheet([&v](PlayerBonuses& one) { one.sp_drain_pct = v[1]; });
    case Type::kGetZenyNum:
    case Type::kAddGetZenyNum:
      return Sheet([&v](PlayerBonuses& one) { one.zeny_bonus_pct = v[0]; });
    case Type::kSetDefRace:
      return Conditional(names, ConditionalBonusKind::kSetDefenseRace, v[0], v[1]);
    case Type::kSetMdefRace:
      return Conditional(names, ConditionalBonusKind::kSetMagicDefenseRace, v[0], v[1]);
    case Type::kHpVanishRaceRate:
      return Conditional(names, ConditionalBonusKind::kVanishHpRace, v[0], v[2]);
    case Type::kSpVanishRaceRate:
      return Conditional(names, ConditionalBonusKind::kVanishSpRace, v[0], v[2]);
    case Type::kHpVanishRate:
      return Valued(ro::core::Tr("Vanish PV"), v[1], Unit::kPercent);
    case Type::kSpVanishRate:
      return Valued(ro::core::Tr("Vanish SP"), v[1], Unit::kPercent);
    case Type::kHpRegenRate:
    case Type::kRegenPercentHp:
      return Valued(Composed(ro::core::Tr("Régén. PV toutes les %d ms"), v[1]), v[0], Unit::kFlat);
    case Type::kSpRegenRate:
    case Type::kRegenPercentSp:
      return Valued(Composed(ro::core::Tr("Régén. SP toutes les %d ms"), v[1]), v[0], Unit::kFlat);
    case Type::kHpLossRate:
      return Valued(Composed(ro::core::Tr("Perte PV toutes les %d ms"), v[1]), v[0], Unit::kFlat);
    case Type::kSpLossRate:
      return Valued(Composed(ro::core::Tr("Perte SP toutes les %d ms"), v[1]), v[0], Unit::kFlat);
    case Type::kAbsorbDmgMaxHp:
    case Type::kAbsorbDmgMaxHp2:
      return Valued(ro::core::Tr("Dégâts plafonnés en PV max"), v[0], Unit::kPercent);
    case Type::kStateNoRecoverRace:
      return Valued(Against(ro::core::Tr("Empêche le soin vs"), RaceOf(names, v[0])), v[1],
                    Unit::kCentiPercent);
    default:
      break;
  }
  return Targeted(bonus, names);
}

}  // namespace

std::string EquipBonusText(const ro::sim::EquipBonus& bonus, const EquipAttributeNames& names) {
  if (bonus.level == ro::sim::kEquipBonusOwnLevel) {
    return Own(bonus);
  }
  if (bonus.level > ro::sim::kEquipBonusValueCount) {
    return Unknown(ro::sim::EquipBonus{.level = 0, .type = bonus.type, .values = bonus.values});
  }
  if (bonus.level == kSingleArgumentLevel) {
    return One(bonus, names);
  }
  if (std::string text = Several(bonus, names); !text.empty()) {
    return text;
  }
  return Unknown(bonus);
}

}  // namespace moonclient
