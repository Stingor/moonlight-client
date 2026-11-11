#include <algorithm>
#include <array>
#include <cstdint>

#include "ro/core/sorted_table.h"
#include "ro/render/plain_hit_effect_table.h"
#include "ro/render/skill_effect_table.h"
#include "ro/render/skill_hit_effect.h"
#include "ro/render/skill_use_motion.h"

namespace ro::render {
namespace {

// LES SEPT SORTES DE DÉGÂTS QUE LE RÉPARTITEUR CONNAÎT, et les deux qu'il
// écarte. `e_damage_type` du serveur les numérote : 4 encaissé, 5 en gerbe,
// 6 simple, 8 et 9 à coups multiples, 13 multiple et critique, 14 sans nom chez
// le serveur.
//
// La gerbe et la quatorzième repartent avant le choix de l'effet ; les cinq
// autres le font.
constexpr std::array<std::uint8_t, 5> kDamageTypesWithHit = {4, 6, 8, 9, 13};

// LE PREMIER COUP D'UNE SÉRIE DE COMBO, EF_COMBOATTACK1, et les niveaux qui le
// remplacent par les coups suivants, EF_COMBOATTACK2 à 5.
constexpr std::int32_t kFirstComboHitEffect = 186;
constexpr std::int32_t kFirstFollowingComboLevel = 2;
constexpr std::int32_t kLastFollowingComboLevel = 5;

}  // namespace

std::int32_t SkillDamageMsgEffect(std::uint16_t skill_id, std::int32_t announced_level) {
  // ⚠ AUCUNE RÈGLE PAR DÉFAUT : l'absence vaut « rien à poser », et c'est ce que
  // fait le client — sa carte rend −1 pour un identifiant qu'elle ne nomme pas
  // comme pour un identifiant qu'elle met à −1.
  const SkillHitEffectEntry* found =
      core::RowWithKey(SkillDamageMsgEffectTable(), skill_id, &SkillHitEffectEntry::skill_id);
  const std::int32_t effect_id = found != nullptr ? found->effect_id : -1;
  if (effect_id == kFirstComboHitEffect && announced_level >= kFirstFollowingComboLevel &&
      announced_level <= kLastFollowingComboLevel) {
    return kFirstComboHitEffect + announced_level - 1;
  }
  return effect_id;
}

std::int32_t SkillHitEffect(std::uint16_t skill_id, bool any_damage, bool cast_by_me) {
  // CE QUI DÉPEND DU COUP D'ABORD : deux compétences ne se décident pas sur
  // leur seul numéro, et leur entrée porte les deux réponses.
  const std::span<const SkillHitEffectWhen> conditions = SkillHitEffectConditions();
  const auto selon = std::ranges::find(conditions, skill_id, &SkillHitEffectWhen::skill_id);
  if (selon != conditions.end()) {
    const bool remplie = selon->when == SkillHitWhen::kAnyDamage ? any_damage : !cast_by_me;
    return remplie ? selon->then_effect : selon->otherwise;
  }
  if (const SkillHitEffectEntry* found =
          core::RowWithKey(SkillHitEffectTable(), skill_id, &SkillHitEffectEntry::skill_id)) {
    return found->effect_id;
  }
  // LA RÈGLE PAR DÉFAUT, et elle n'est pas « rien » : la plupart des
  // compétences classiques posent la gerbe ordinaire sur ce qu'elles touchent.
  const SkillHitEffectDefault regle = SkillHitEffectDefaults();
  if ((skill_id >= regle.silent_from && skill_id <= regle.silent_to) ||
      skill_id >= regle.silent_beyond) {
    return -1;
  }
  return regle.otherwise;
}

std::int32_t SkillUnitHitEffect(std::uint16_t skill_id) {
  if (const SkillHitEffectEntry* found =
          core::RowWithKey(SkillUnitHitEffectTable(), skill_id, &SkillHitEffectEntry::skill_id)) {
    return found->effect_id;
  }
  const SkillHitEffectDefault regle = SkillUnitHitEffectDefaults();
  return skill_id >= regle.silent_beyond ? -1 : regle.otherwise;
}

bool DamageTypeShowsHitEffect(std::uint8_t damage_type) {
  return std::ranges::find(kDamageTypesWithHit, damage_type) != kDamageTypesWithHit.end();
}

bool DamageTypeShowsHitEffect(std::uint8_t damage_type, std::uint16_t skill_id) {
  if (DamageTypeShowsHitEffect(damage_type)) {
    return true;
  }
  // LES COMPÉTENCES QUI NE LAISSENT PAS L'ÉCLABOUSSURE À LA VOIE COURTE : leurs
  // victimes alentour suivent le chemin ordinaire, et reçoivent leur impact.
  if (skill_id != 0 &&
      (damage_type == kSplashDamageType || damage_type == kSplashEnduredDamageType) &&
      !SplashHitKeepsAction(skill_id, damage_type)) {
    return true;
  }
  constexpr std::uint8_t kDamageTypeBits = 16;
  if (damage_type >= kDamageTypeBits) {
    return false;
  }
  const SplashHitSkill* found =
      core::RowWithKey(SplashHitSkillTable(), skill_id, &SplashHitSkill::skill_id);
  return found != nullptr &&
         ((static_cast<std::uint32_t>(found->damage_types) >> damage_type) & 1U) != 0;
}

const SkillDamageEffectEntry* SkillDamageEffectOf(std::uint16_t skill_id) {
  return core::RowWithKey(SkillDamageEffectTable(), skill_id, &SkillDamageEffectEntry::skill_id);
}

const SkillAnnouncedEffectEntry* SkillAnnouncedEffectOf(std::uint16_t skill_id) {
  return core::RowWithKey(SkillAnnouncedEffectTable(), skill_id,
                          &SkillAnnouncedEffectEntry::skill_id);
}

std::int32_t PlainHitStrikeEffect(std::uint8_t damage_type) {
  for (const PlainHitByDamageType& entry : PlainHitStrikeEffectTable()) {
    if (entry.damage_type == damage_type) {
      return entry.effect_id;
    }
  }
  return PlainHitDefaults().strike;
}

std::int32_t PlainHitMsgEffect(std::uint16_t attacker_job, bool self_inflicted) {
  const PlainHitEffectDefaults defaults = PlainHitDefaults();
  if (self_inflicted) {
    // Le client met −2 dans le champ, et ce −2 ne désigne aucun effet : rien ne
    // se pose, et le son se tait par la même occasion.
    return defaults.self_inflicted;
  }
  for (const PlainHitByJob& entry : PlainHitMsgEffectTable()) {
    if (entry.job == attacker_job) {
      return entry.effect_id;
    }
  }
  return defaults.message;
}

}  // namespace ro::render
