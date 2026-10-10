// Voir l'en-tête.

#include "ro/sim/companion_rules.h"

#include "ro/sim/companion_skills.h"
#include "ro/sim/skill_book.h"

namespace ro::sim {
namespace {

[[nodiscard]] bool Between(int value, int lowest, int highest) noexcept {
  return value >= lowest && value <= highest;
}

[[nodiscard]] bool OwnerOrCompanion(RuleSubject subject) noexcept {
  return subject == RuleSubject::kOwner || subject == RuleSubject::kCompanion;
}

// Sous ou au-dessus : une mesure.
[[nodiscard]] bool Measure(RuleComparison comparison) noexcept {
  return comparison == RuleComparison::kBelow || comparison == RuleComparison::kAbove;
}

// Est ou n'est pas : un fait.
[[nodiscard]] bool Fact(RuleComparison comparison) noexcept {
  return comparison == RuleComparison::kIs || comparison == RuleComparison::kIsNot;
}

}  // namespace

std::uint16_t RuleAimTargeting(RuleAim aim) noexcept {
  if (AimsAtCell(aim)) {
    return kSkillOnGround;
  }
  switch (aim) {
    case RuleAim::kSelf:
      return kSkillOnSelf | kSkillOnFriend;
    case RuleAim::kOwner:
    case RuleAim::kFriend:
      return kSkillOnFriend;
    default:
      break;
  }
  return kSkillOnEnemy;
}

bool ValidCondition(const RuleCondition& condition) noexcept {
  const RuleSubject subject = condition.subject;
  const RuleComparison comparison = condition.comparison;
  if (static_cast<int>(subject) > static_cast<int>(RuleSubject::kSurroundings) ||
      static_cast<int>(comparison) > static_cast<int>(RuleComparison::kIsNot)) {
    return false;
  }
  switch (condition.predicate) {
    case RulePredicate::kHpPercent:
      return Between(condition.number, 0, kRulePercentMost) &&
             ((OwnerOrCompanion(subject) && Measure(comparison)) ||
              (subject == RuleSubject::kTarget && comparison == RuleComparison::kBelow));
    case RulePredicate::kSpPercent:
      return subject == RuleSubject::kCompanion && Measure(comparison) &&
             Between(condition.number, 0, kRulePercentMost);
    case RulePredicate::kSpPoints:
      return subject == RuleSubject::kCompanion && Measure(comparison) &&
             Between(condition.number, 0, kRulePointsMost);
    case RulePredicate::kAttackedFor:
      return OwnerOrCompanion(subject) && comparison == RuleComparison::kIs &&
             Between(condition.seconds, 0, kRuleSecondsMost);
    case RulePredicate::kAttackersWeight:
      return OwnerOrCompanion(subject) && comparison == RuleComparison::kIs &&
             Between(condition.number, 1, kRuleCountMost);
    case RulePredicate::kHasStatus:
      return OwnerOrCompanion(subject) && Fact(comparison) && condition.id != 0;
    case RulePredicate::kSitting:
      return subject == RuleSubject::kOwner && Fact(comparison);
    case RulePredicate::kActivity:
      return subject == RuleSubject::kCompanion && Fact(comparison) &&
             Between(condition.choice, 0, static_cast<int>(CompanionActivity::kFighting));
    case RulePredicate::kCastWithin:
      return subject == RuleSubject::kCompanion && Fact(comparison) && condition.id != 0 &&
             Between(condition.seconds, 1, kRuleSecondsMost);
    case RulePredicate::kAppearedWithin:
      return subject == RuleSubject::kCompanion && comparison == RuleComparison::kIs &&
             Between(condition.seconds, 1, kRuleSecondsMost);
    case RulePredicate::kFartherThan:
      return subject == RuleSubject::kCompanion && comparison == RuleComparison::kIs &&
             Between(condition.cells, 0, kRuleCellsMost);
    case RulePredicate::kRace:
      return subject == RuleSubject::kTarget && Fact(comparison) &&
             Between(condition.choice, 0, kMonsterRaceCount - 1);
    case RulePredicate::kElement:
      return subject == RuleSubject::kTarget && Fact(comparison) &&
             Between(condition.choice, 0, kMonsterElementCount - 1);
    case RulePredicate::kSize:
      return subject == RuleSubject::kTarget && Fact(comparison) &&
             Between(condition.choice, 0, kMonsterSizeCount - 1);
    case RulePredicate::kMonsterRank:
      return subject == RuleSubject::kTarget && Fact(comparison) &&
             Between(condition.choice, 0, kMonsterRankCount - 1);
    case RulePredicate::kMonsterJob:
      return subject == RuleSubject::kTarget && Fact(comparison) && condition.id != 0;
    case RulePredicate::kCasting:
      return subject == RuleSubject::kTarget && Fact(comparison);
    case RulePredicate::kAttacks:
      return subject == RuleSubject::kTarget && Fact(comparison) &&
             Between(condition.choice, 0, static_cast<int>(RuleWhom::kFriend));
    case RulePredicate::kEnemiesNear:
      return subject == RuleSubject::kSurroundings && comparison == RuleComparison::kIs &&
             Between(condition.number, 1, kRuleCountMost) &&
             Between(condition.cells, 1, kRuleCellsMost) &&
             Between(condition.choice, 0, static_cast<int>(RuleAround::kTarget));
  }
  return false;
}

bool ValidRule(const CompanionRule& rule) noexcept {
  if (rule.conditions.empty() || rule.conditions.size() > kRuleConditionMost ||
      static_cast<int>(rule.aim) > static_cast<int>(kLastRuleAim) ||
      rule.own_delay_ms > kRuleDelayMostMs ||
      !Between(rule.per_target_most, 0, kRulePerTargetMost)) {
    return false;
  }
  for (const RuleCondition& condition : rule.conditions) {
    if (!ValidCondition(condition)) {
      return false;
    }
  }
  switch (rule.action) {
    case RuleAction::kAttack:
      return AimsAtEnemy(rule.aim);
    case RuleAction::kCastSkill:
      // ⚠ LES SKILLS « JAMAIS SEULS » LE RESTENT DANS UNE RÈGLE : ce qu'ils
      // coûtent ne se rattrape pas, et une règle se rejoue sans qu'on la voie.
      return rule.skill_id != 0 && !NeverCastAutomatically(rule.skill_id) &&
             Between(rule.skill_level, 0, kRuleSkillLevelMost);
    case RuleAction::kStepAway:
      return AimsAtEnemy(rule.aim) && Between(rule.step_cells, 1, kRuleCellsMost);
    case RuleAction::kReturnToOwner:
    case RuleAction::kDoNothing:
      return true;
  }
  return false;
}

bool ReadsCompanionStatus(std::span<const CompanionRule> rules) noexcept {
  for (const CompanionRule& rule : rules) {
    for (const RuleCondition& condition : rule.conditions) {
      if (condition.subject == RuleSubject::kCompanion &&
          condition.predicate == RulePredicate::kHasStatus) {
        return true;
      }
    }
  }
  return false;
}

bool ValidFriendName(const std::string& name) noexcept {
  return !name.empty() && name.find_first_of("\r\n") == std::string::npos;
}

}  // namespace ro::sim
