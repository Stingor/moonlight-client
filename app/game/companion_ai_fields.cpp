// LES CHAMPS DE LA FENÊTRE « IA DU COMPAGNON » : réglages, clés génériques,
// règles en nombres, et la raison d'une règle écartée. Voir
// `game/companion_ai_fields.h`.

#include "game/companion_ai_fields.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <limits>
#include <string>

#include "ro/core/i18n.h"

#include "game/companion_ai_yaml.h"

namespace moonclient {
namespace {

using ro::sim::CompanionAiSettings;
using ro::sim::CompanionRule;
using ro::sim::RuleCondition;
using ro::sim::TacticKeyKind;

// 🔴 LA FENÊTRE NE CONNAÎT PAS L'IA, mais elle en suit les listes et les bornes.
static_assert(ro::ui::kCompanionAiRuleMost == ro::sim::kCompanionRuleMost);
static_assert(ro::ui::kCompanionAiConditionMost == ro::sim::kRuleConditionMost);
static_assert(ro::ui::kCompanionAiFriendMost == ro::sim::kCompanionFriendMost);
static_assert(ro::ui::kCompanionAiPerTargetMost == ro::sim::kRulePerTargetMost);
static_assert(ro::ui::kCompanionAiRuleDelayMostMs == static_cast<int>(ro::sim::kRuleDelayMostMs));
static_assert(ro::ui::kCompanionAiCellsMost == companion_yaml::kSettingCellsMost);
static_assert(ro::ui::kCompanionAiPercentMost == companion_yaml::kSettingPercentMost);
static_assert(ro::ui::kCompanionAiDelayMostMs ==
              static_cast<int>(companion_yaml::kSettingDelayMostMs));
static_assert(ro::ui::kCompanionAiSpReserveMost == ro::sim::kTacticSpReserveMost);
static_assert(ro::ui::kTacticRankChoices == static_cast<int>(ro::sim::kTacticRankCount));
static_assert(ro::ui::kTacticTriggerChoices == static_cast<int>(ro::sim::kTacticTriggerCount));
static_assert(ro::ui::kOffensiveSkillChoices ==
              static_cast<int>(ro::sim::OffensiveSkills::kSkillsOnly) + 1);
static_assert(ro::ui::kOwnerSeatedChoices ==
              static_cast<int>(ro::sim::OwnerSeated::kStayBesidePassive) + 1);

// LES LIGNES GÉNÉRIQUES DE LA FENÊTRE, dans son ordre.
constexpr std::array<TacticKeyKind, ro::ui::kTacticGenericChoices> kGenericKinds = {
    TacticKeyKind::kAnyMonster, TacticKeyKind::kChest, TacticKeyKind::kStill,
    TacticKeyKind::kMinion};
// 🔴 L'ORDRE, PAS SEULEMENT LE COMPTE : une permutation donnerait des libellés
// faux sans rien casser.
static_assert(kGenericKinds[ro::ui::kTacticGenericAnyMonster] == TacticKeyKind::kAnyMonster);
static_assert(kGenericKinds[ro::ui::kTacticGenericChests] == TacticKeyKind::kChest);
static_assert(kGenericKinds[ro::ui::kTacticGenericStill] == TacticKeyKind::kStill);
static_assert(kGenericKinds[ro::ui::kTacticGenericMinions] == TacticKeyKind::kMinion);

constexpr std::size_t kLineBytes = 192;
// Un identifiant de skill, de statut ou de classe : seize bits.
constexpr int kIdMost = std::numeric_limits<std::uint16_t>::max();
// LA RÈGLE D'USINE : sous ce pour cent de ses PV, le compagnon revient.
constexpr int kDefaultRuleHpPercent = 30;

[[nodiscard]] std::uint16_t Id16(int value) {
  return static_cast<std::uint16_t>(std::clamp(value, 0, kIdMost));
}

[[nodiscard]] ro::ui::RuleConditionFields ConditionFieldsOf(const RuleCondition& condition) {
  return ro::ui::RuleConditionFields{.subject = static_cast<int>(condition.subject),
                                     .predicate = static_cast<int>(condition.predicate),
                                     .comparison = static_cast<int>(condition.comparison),
                                     .number = condition.number,
                                     .seconds = condition.seconds,
                                     .cells = condition.cells,
                                     .id = condition.id,
                                     .choice = condition.choice};
}

[[nodiscard]] RuleCondition ConditionOf(const ro::ui::RuleConditionFields& fields) {
  return RuleCondition{.subject = static_cast<ro::sim::RuleSubject>(fields.subject),
                       .predicate = static_cast<ro::sim::RulePredicate>(fields.predicate),
                       .comparison = static_cast<ro::sim::RuleComparison>(fields.comparison),
                       .number = fields.number,
                       .seconds = fields.seconds,
                       .cells = fields.cells,
                       .id = Id16(fields.id),
                       .choice = fields.choice};
}

}  // namespace

ro::ui::CompanionBaseFields BaseFieldsOf(const CompanionAiSettings& settings,
                                         std::span<const ro::ui::OffensiveSkillRow> offensive) {
  ro::ui::CompanionBaseFields fields{
      .follow_cells = settings.follow_distance_cells,
      .leash_still_cells = settings.leash_still_cells,
      .leash_walking_cells = settings.leash_walking_cells,
      .engage_still_cells = settings.engage_still_cells,
      .engage_walking_cells = settings.engage_walking_cells,
      .attack_on_own = settings.attack_on_own,
      .own_attack_min_hp_percent = settings.own_attack_min_hp_percent,
      .own_attack_min_sp_percent = settings.own_attack_min_sp_percent,
      .defend_owner = settings.defend_owner,
      .assist_owner = settings.assist_owner,
      .defend_partner = settings.defend_partner,
      .defend_self = settings.defend_self,
      .spare_others_monsters = settings.spare_others_monsters,
      .chase = settings.chase,
      .retarget_while_chasing = settings.retarget_while_chasing,
      .drop_untouched_after_ms = static_cast<int>(settings.drop_untouched_after_ms),
      .arrival_attack_hold_ms = static_cast<int>(settings.arrival_attack_hold_ms),
      .offensive_skills = static_cast<int>(settings.skills.offensive),
      .permissions = {},
      .skill_sp_reserve = static_cast<int>(settings.skills.sp_reserve_points),
      .skill_delay_ms = static_cast<int>(settings.skills.delay_ms),
      .standby_defends = settings.standby_defends,
      .standby_after_fight = settings.standby_after_fight,
      .standby_kept_across_maps = settings.standby_kept_across_maps,
      .move_order_holds_cell = settings.move_order_holds_cell,
      .move_order_hold_fights = settings.move_order_hold_fights,
      .owner_seated = static_cast<int>(settings.owner_seated),
      .return_below_hp_percent = settings.return_below_hp_percent,
      .flee_below_hp_percent = settings.flee_below_hp_percent};
  for (const ro::ui::OffensiveSkillRow& row : offensive) {
    ro::ui::SkillPermissionField permission{.skill_id = row.skill_id};
    for (const ro::sim::SkillPermission& set : settings.skills.permissions) {
      if (set.skill_id == row.skill_id) {
        permission.allowed = set.allowed;
        permission.level = set.level;
      }
    }
    fields.permissions.push_back(permission);
  }
  return fields;
}

CompanionAiSettings WithBaseFields(CompanionAiSettings settings,
                                   const ro::ui::CompanionBaseFields& fields) {
  const auto ms = [](int value) {
    return static_cast<std::uint32_t>(std::clamp(value, 0, ro::ui::kCompanionAiDelayMostMs));
  };
  settings.follow_distance_cells = fields.follow_cells;
  settings.leash_still_cells = fields.leash_still_cells;
  settings.leash_walking_cells = fields.leash_walking_cells;
  settings.engage_still_cells = fields.engage_still_cells;
  settings.engage_walking_cells = fields.engage_walking_cells;
  settings.attack_on_own = fields.attack_on_own;
  settings.own_attack_min_hp_percent = fields.own_attack_min_hp_percent;
  settings.own_attack_min_sp_percent = fields.own_attack_min_sp_percent;
  settings.defend_owner = fields.defend_owner;
  settings.assist_owner = fields.assist_owner;
  settings.defend_partner = fields.defend_partner;
  settings.defend_self = fields.defend_self;
  settings.spare_others_monsters = fields.spare_others_monsters;
  settings.chase = fields.chase;
  settings.retarget_while_chasing = fields.retarget_while_chasing;
  settings.drop_untouched_after_ms = ms(fields.drop_untouched_after_ms);
  settings.arrival_attack_hold_ms = ms(fields.arrival_attack_hold_ms);
  settings.skills.offensive = static_cast<ro::sim::OffensiveSkills>(
      std::clamp(fields.offensive_skills, 0, ro::ui::kOffensiveSkillChoices - 1));
  settings.skills.sp_reserve_points =
      std::clamp(fields.skill_sp_reserve, 0, ro::ui::kCompanionAiSpReserveMost);
  settings.skills.delay_ms = ms(fields.skill_delay_ms);
  settings.standby_defends = fields.standby_defends;
  settings.standby_after_fight = fields.standby_after_fight;
  settings.standby_kept_across_maps = fields.standby_kept_across_maps;
  settings.move_order_holds_cell = fields.move_order_holds_cell;
  settings.move_order_hold_fights = fields.move_order_hold_fights;
  settings.owner_seated = static_cast<ro::sim::OwnerSeated>(
      std::clamp(fields.owner_seated, 0, ro::ui::kOwnerSeatedChoices - 1));
  settings.return_below_hp_percent = fields.return_below_hp_percent;
  settings.flee_below_hp_percent = fields.flee_below_hp_percent;
  for (const ro::ui::SkillPermissionField& shown : fields.permissions) {
    const ro::sim::SkillPermission permission{
        .skill_id = shown.skill_id,
        .allowed = shown.allowed,
        .level = static_cast<std::uint16_t>(
            std::clamp(shown.level, 0, static_cast<int>(companion_yaml::kSettingSkillLevelMost)))};
    const auto found = std::ranges::find_if(settings.skills.permissions, [&shown](const auto& set) {
      return set.skill_id == shown.skill_id;
    });
    if (found != settings.skills.permissions.end()) {
      *found = permission;
    } else {
      settings.skills.permissions.push_back(permission);
    }
  }
  return settings;
}

ro::sim::TacticKey GenericKeyOf(int generic) noexcept {
  const int index = std::clamp(generic, 0, ro::ui::kTacticGenericChoices - 1);
  return ro::sim::TacticKey{.kind = kGenericKinds[static_cast<std::size_t>(index)]};
}

int GenericIndexOf(TacticKeyKind kind) noexcept {
  for (std::size_t index = 0; index < kGenericKinds.size(); ++index) {
    if (kGenericKinds[index] == kind) {
      return static_cast<int>(index);
    }
  }
  return -1;
}

ro::ui::RuleFields RuleFieldsOf(const CompanionRule& rule) {
  ro::ui::RuleFields fields{.action = static_cast<int>(rule.action),
                            .aim = static_cast<int>(rule.aim),
                            .skill_id = rule.skill_id,
                            .skill_level = rule.skill_level,
                            .step_cells = rule.step_cells,
                            .conditions = {},
                            .own_delay_ms = static_cast<int>(rule.own_delay_ms),
                            .per_target_most = rule.per_target_most,
                            .ignores_sp_reserve = rule.ignores_sp_reserve};
  for (const RuleCondition& condition : rule.conditions) {
    fields.conditions.push_back(ConditionFieldsOf(condition));
  }
  return fields;
}

CompanionRule RuleOf(const ro::ui::RuleFields& fields) {
  CompanionRule rule{.action = static_cast<ro::sim::RuleAction>(fields.action),
                     .aim = static_cast<ro::sim::RuleAim>(fields.aim),
                     .skill_id = Id16(fields.skill_id),
                     .skill_level = static_cast<std::uint16_t>(
                         std::clamp(fields.skill_level, 0, ro::sim::kRuleSkillLevelMost)),
                     .step_cells = fields.step_cells,
                     .conditions = {},
                     .own_delay_ms = static_cast<std::uint32_t>(
                         std::clamp(fields.own_delay_ms, 0, ro::ui::kCompanionAiRuleDelayMostMs)),
                     .per_target_most = fields.per_target_most,
                     .ignores_sp_reserve = fields.ignores_sp_reserve};
  for (const ro::ui::RuleConditionFields& condition : fields.conditions) {
    rule.conditions.push_back(ConditionOf(condition));
  }
  return rule;
}

CompanionRule DefaultCompanionRule() {
  return CompanionRule{.action = ro::sim::RuleAction::kReturnToOwner,
                       .conditions = {RuleCondition{.subject = ro::sim::RuleSubject::kCompanion,
                                                    .predicate = ro::sim::RulePredicate::kHpPercent,
                                                    .comparison = ro::sim::RuleComparison::kBelow,
                                                    .number = kDefaultRuleHpPercent}}};
}

std::string RejectedRuleLine(const RejectedRule& rejected) {
  std::string why;
  std::array<char, kLineBytes> named{};
  switch (rejected.why) {
    case RuleRejection::kNotARule:
      why = ro::core::Tr("illisible");
      break;
    case RuleRejection::kUnknownWord:
      std::snprintf(named.data(), named.size(), ro::core::Tr("mot inconnu pour « %s »"),
                    rejected.field.c_str());
      why = named.data();
      break;
    case RuleRejection::kBadNumber:
      std::snprintf(named.data(), named.size(), ro::core::Tr("nombre illisible pour « %s »"),
                    rejected.field.c_str());
      why = named.data();
      break;
    case RuleRejection::kNoCondition:
      why = ro::core::Tr("aucune condition");
      break;
    case RuleRejection::kBadCondition:
      why = ro::core::Tr("une condition illisible ou hors du cadre");
      break;
    case RuleRejection::kOutOfFrame:
      why = ro::core::Tr("action, skill ou cible d'action hors du cadre");
      break;
    case RuleRejection::kBeyondLimit:
      why = ro::core::Tr("au-delà de la dernière règle permise");
      break;
  }
  std::array<char, kLineBytes> line{};
  std::snprintf(line.data(), line.size(), ro::core::Tr("Règle %d du fichier : %s"),
                static_cast<int>(rejected.position + 1), why.c_str());
  return line.data();
}

}  // namespace moonclient
