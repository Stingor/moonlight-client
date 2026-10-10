#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace ro::sim {

// CE QUE L'ÉQUIPEMENT ET LES CARTES APPORTENT, dans le détail.
//
// 🔴 CELA N'ARRIVE PAR AUCUNE TRAME ORDINAIRE. Le client natif ne montre que des
// TOTAUX — « ATK 214 » — et ne sait pas d'où ils viennent. Ce relevé-ci est
// compilé par `status_calc_pc` et poussé par `ZC_MOONLIGHT_STAT_BONUS 0x0F10`,
// qui ne part qu'à un client ayant donné sa poignée de main (ADR 0040).
//
// ⚠ LES NOMS SONT CEUX DU SERVEUR, à la lettre, pour la même raison que
// `PlayerStatus` : tout ce qui est ici se recoupe champ par champ contre
// l'écriture de ce paquet par le serveur, et un nom traduit obligerait à tenir une table de
// correspondance dans la tête de qui relit.

// Un bonus qui ne vaut QUE CONTRE QUELQU'UN : une catégorie, un index dans cette
// catégorie, une magnitude.
//
// ⚠ L'INDEX NE SE LIT PAS SANS LE CODE. Le même `2` est « Eau » sous
// `kSubElement` et « Insecte » sous `kSubRace` ; c'est la paire qui nomme.
struct ConditionalBonus {
  std::uint16_t code = 0;
  std::int16_t index = 0;
  std::int32_t value = 0;
};

// Un bonus attaché à une COMPÉTENCE — ou à un statut.
struct SkillBonus {
  std::uint16_t code = 0;
  // ⚠ CE N'EST PAS TOUJOURS UNE COMPÉTENCE : sous `kAddEffect`, `kAddEffectHit`
  // et `kResistEffect`, ce champ porte un EFST, c'est-à-dire l'icône d'un
  // statut. Le serveur le dit dans le même champ faute d'un second.
  std::uint16_t skill_id = 0;
  // Le niveau lancé par un auto-sort. Zéro partout ailleurs.
  std::int16_t level = 0;
  std::int32_t value = 0;
  // La compétence qui DÉCLENCHE l'auto-sort, pour `kAutoSpellOnSkill`. Zéro
  // partout ailleurs.
  std::uint16_t triggered_by_skill = 0;
};

// Un bonus attaché à un OBJET. Séparé des deux autres parce qu'un identifiant
// d'objet tient sur trente-deux bits, là où une compétence en prend seize.
struct ItemBonus {
  std::uint16_t code = 0;
  // ⚠ UN IDENTIFIANT DE GROUPE sous `kAddDropGroup`, et non d'objet : le
  // serveur réemploie le champ.
  std::uint32_t item_id = 0;
  std::int32_t rate = 0;
};

// Les catégories de `ConditionalBonus`, miroir exact des numéros du serveur.
enum class ConditionalBonusKind : std::uint16_t {
  kSubElement = 1,
  kSubRace = 2,
  kSubSize = 3,
  kAddElement = 4,
  kAddRace = 5,
  kAddSize = 6,
  kMagicAddElement = 7,
  kMagicAddRace = 8,
  kMagicAddSize = 9,
  kCriticalRace = 10,
  kIgnoreDefenseRace = 11,
  kIgnoreMagicDefenseRace = 12,
  kSubDefenseElement = 13,
  kSubClass = 14,
  kSubRaceGroup = 15,
  kExperienceRace = 16,
  kExperienceClass = 17,
  kDropRace = 18,
  kDropClass = 19,
  kSetDefenseRace = 20,
  kSetMagicDefenseRace = 21,
  kVanishHpRace = 22,
  kVanishSpRace = 23,
  kComaRace = 24,
  kComaClass = 25,
  kIgnoreResistanceRace = 26,
  kIgnoreMagicResistanceRace = 27,
  kMagicAddRaceGroup = 28,
  kIgnoreMagicDefenseRaceGroup = 29,
  kSpGainRace = 30,
  kIgnoreDefenseClass = 31,
  kIgnoreMagicDefenseClass = 32,
};

// Les catégories de `SkillBonus`, miroir exact des numéros du serveur.
enum class SkillBonusKind : std::uint16_t {
  kAutoSpell = 1,
  kAutoSpellWhenHit = 2,
  kSkillAttack = 3,
  kAddEffect = 4,
  kAddEffectWhenHit = 5,
  kResistEffect = 6,
  kSubSkill = 7,
  kAutoSpellOnSkill = 8,
  kSkillSpRate = 9,
  kSkillSpCost = 10,
  kSkillVariableCastRate = 11,
  kSkillFixedCastRate = 12,
  kSkillVariableCastMs = 13,
  kSkillFixedCastMs = 14,
  kSkillCooldownMs = 15,
  kSkillDelayRate = 16,
  kSkillHealGiven = 17,
  kSkillHealTaken = 18,
  kSkillKnockback = 19,
};

// Les catégories d'`ItemBonus`, miroir exact des numéros du serveur.
enum class ItemBonusKind : std::uint16_t {
  kAddDrop = 1,
  kAddDropGroup = 2,
};

// Combien de caractéristiques primaires le relevé détaille. Les six, dans
// l'ordre du client : force, agilité, constitution, intelligence, dextérité,
// chance.
inline constexpr std::size_t kPrimaryAttributes = 6;

struct PlayerBonuses {
  // ⚠ « PAS ENCORE DIT » N'EST PAS « TOUT À ZÉRO ». Un personnage nu a bien
  // quarante-neuf zéros ; un personnage dont le serveur n'a pas parlé n'a rien
  // à montrer du tout, et la fiche doit pouvoir faire la différence. Même règle
  // que `kUnknownStat` dans `PlayerStatus`, dite autrement parce qu'ici c'est
  // le RELEVÉ ENTIER qui manque, pas un champ.
  bool told = false;

  // Ce que l'ÉQUIPEMENT ajoute aux six, et ce que les CARTES y ajoutent. Le
  // serveur les compile séparément (`param_equip` / `param_bonus`) : c'est ce
  // qui permet d'écrire « 40 + 12 » plutôt qu'un total muet.
  std::array<std::int16_t, kPrimaryAttributes> param_equip = {};
  std::array<std::int16_t, kPrimaryAttributes> param_bonus = {};

  std::int32_t eatk = 0;          // ATK issu de l'équipement
  std::int32_t ematk = 0;         // MATK issu de l'équipement
  std::int32_t melee_pct = 0;     // % dégâts de mêlée à mains nues
  std::int32_t ranged_pct = 0;    // % dégâts à distance
  std::int32_t crit_dmg_pct = 0;  // % dégâts des critiques
  std::int32_t hp_add = 0;
  std::int32_t sp_add = 0;
  std::int32_t aspd_add = 0;
  std::int32_t vcast_pct = 0;  // ⚠ NÉGATIF = RÉDUCTION, pour les quatre « cast »
  std::int32_t fcast_pct = 0;

  std::int32_t atk_pct = 0;
  std::int32_t matk_pct = 0;
  std::int32_t dmg_ret_melee = 0;  // renvoi des dégâts reçus, en %
  std::int32_t dmg_ret_ranged = 0;
  std::int32_t dmg_ret_magic = 0;
  std::int32_t double_pct = 0;
  std::int32_t perfect_hit = 0;

  std::int32_t hp_pct = 0;
  std::int32_t sp_pct = 0;
  std::int32_t hp_regen_pct = 0;
  std::int32_t sp_regen_pct = 0;
  std::int32_t crit_def_pct = 0;
  std::int32_t hp_on_kill = 0;
  std::int32_t sp_on_kill = 0;
  std::int32_t unbreak_pct = 0;

  std::int32_t pot_hp_pct = 0;
  std::int32_t pot_sp_pct = 0;
  std::int32_t heal_up_pct = 0;
  std::int32_t delay_pct = 0;
  std::int32_t add_vcast_ms = 0;
  std::int32_t add_fcast_ms = 0;
  std::int32_t steal_pct = 0;

  std::int32_t def_melee_pct = 0;  // réduction des dégâts REÇUS, par type
  std::int32_t def_ranged_pct = 0;
  std::int32_t def_magic_pct = 0;
  std::int32_t def_misc_pct = 0;
  std::int32_t splash = 0;  // en cases
  std::int32_t splash_add = 0;

  std::int32_t hp_drain_pct = 0;  // vol de vie à l'attaque
  std::int32_t sp_drain_pct = 0;

  std::int32_t break_weapon_pct = 0;
  std::int32_t break_armor_pct = 0;
  std::int32_t zeny_bonus_pct = 0;
  std::int32_t classchange_pct = 0;
  std::int32_t dmg_ret_reduce = 0;
  std::int32_t magic_hp_gain = 0;
  std::int32_t magic_sp_gain = 0;

  // La part du RAFFINAGE dans l'attaque et la défense, pour écrire « dont X du
  // refine » là où le client natif ne montre qu'un total.
  std::int32_t refine_atk = 0;
  std::int32_t refine_def = 0;

  // ⚠ SEULES LES ENTRÉES NON NULLES VOYAGENT. Le serveur parcourt ses tableaux
  // entiers — une centaine de cases — et n'émet que ce qui vaut quelque chose ;
  // ces listes sont donc courtes, et leur ORDRE est celui de son parcours.
  std::vector<ConditionalBonus> conditional;
  std::vector<SkillBonus> skills;
  std::vector<ItemBonus> items;
};

}  // namespace ro::sim
