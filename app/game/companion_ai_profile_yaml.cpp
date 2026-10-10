// LES RÈGLES SI/ALORS ET LES AMIS D'UN PROFIL DE `companion_ai.yaml`. Voir
// `game/companion_ai_yaml.h`.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <yaml-cpp/yaml.h>

#include "ro/sim/companion_rules.h"

#include "game/companion_ai_yaml.h"

namespace moonclient::companion_yaml {
namespace {

using ro::sim::CompanionRule;
using ro::sim::RuleCondition;
using ro::sim::RulePredicate;

// ⚠ GRAVÉS, comme tous les champs du fichier.
constexpr const char* kActionField = "action";
constexpr const char* kAimField = "aim";
constexpr const char* kRuleSkillField = "skill";
constexpr const char* kRuleLevelField = "level";
constexpr const char* kStepField = "step_cells";
constexpr const char* kDelayField = "delay_ms";
constexpr const char* kPerTargetField = "per_target";
constexpr const char* kIgnoresReserveField = "ignores_sp_reserve";
constexpr const char* kConditionsField = "when";
constexpr const char* kSubjectField = "subject";
constexpr const char* kPredicateField = "predicate";
constexpr const char* kComparisonField = "comparison";
constexpr const char* kNumberField = "number";
constexpr const char* kSecondsField = "seconds";
constexpr const char* kCellsField = "cells";
constexpr const char* kIdField = "id";
constexpr const char* kChoiceField = "choice";
// Les champs d'une règle que ce client connaît : les autres restent.
constexpr std::array<std::string_view, 9> kRuleFields = {
    kActionField, kAimField,       kRuleSkillField,      kRuleLevelField, kStepField,
    kDelayField,  kPerTargetField, kIgnoresReserveField, kConditionsField};

constexpr std::array<std::string_view, 5> kActionWords = {"attack", "cast_skill", "return_to_owner",
                                                          "step_away", "do_nothing"};
constexpr std::array<std::string_view, 9> kAimWords = {"target",
                                                       "self",
                                                       "owner",
                                                       "owner_target",
                                                       "owner_attacker",
                                                       "friend",
                                                       "cell_under_target",
                                                       "cell_under_owner",
                                                       "best_cell"};
constexpr std::array<std::string_view, 4> kSubjectWords = {"owner", "companion", "target",
                                                           "surroundings"};
constexpr std::array<std::string_view, 19> kPredicateWords = {
    "hp_percent",   "sp_percent", "sp_points", "attacked_for", "attackers_weight",
    "has_status",   "sitting",    "activity",  "cast_within",  "appeared_within",
    "farther_than", "race",       "element",   "size",         "monster_rank",
    "monster_job",  "casting",    "attacks",   "enemies_near"};
constexpr std::array<std::string_view, 4> kComparisonWords = {"below", "above", "is", "is_not"};
// LES CHOIX QUI SONT DES MOTS : ceux qu'on numérote, pas le serveur. La race,
// l'élément, la taille et le rang sont des constantes du serveur, et restent
// des nombres.
constexpr std::array<std::string_view, 3> kActivityWords = {"resting", "chasing", "fighting"};
constexpr std::array<std::string_view, 3> kWhomWords = {"owner", "companion", "friend"};
constexpr std::array<std::string_view, 3> kAroundWords = {"companion", "owner", "target"};

static_assert(kActionWords.size() == static_cast<std::size_t>(ro::sim::kLastRuleAction) + 1);
static_assert(kAimWords.size() == static_cast<std::size_t>(ro::sim::kLastRuleAim) + 1);
static_assert(kPredicateWords.size() == static_cast<std::size_t>(ro::sim::kLastRulePredicate) + 1);
static_assert(kActivityWords.size() ==
              static_cast<std::size_t>(ro::sim::CompanionActivity::kFighting) + 1);
static_assert(kWhomWords.size() == static_cast<std::size_t>(ro::sim::RuleWhom::kFriend) + 1);
static_assert(kAroundWords.size() == static_cast<std::size_t>(ro::sim::RuleAround::kTarget) + 1);

// Les mots du choix de ce prédicat ; vide quand le choix est un nombre.
[[nodiscard]] std::span<const std::string_view> ChoiceWordsOf(RulePredicate predicate) {
  switch (predicate) {
    case RulePredicate::kActivity:
      return kActivityWords;
    case RulePredicate::kAttacks:
      return kWhomWords;
    case RulePredicate::kEnemiesNear:
      return kAroundWords;
    default:
      break;
  }
  return {};
}

// LE CHOIX : un mot de son vocabulaire, ou son rang — un fichier écrit avant
// les mots les a.
[[nodiscard]] bool ReadChoice(const YAML::Node& entry, RuleCondition& condition,
                              FieldFault& fault) {
  const YAML::Node node = entry[kChoiceField];
  if (!node) {
    return true;
  }
  const std::span<const std::string_view> words = ChoiceWordsOf(condition.predicate);
  if (node.IsScalar()) {
    const auto found = std::ranges::find(words, node.Scalar());
    if (found != words.end()) {
      condition.choice = static_cast<int>(found - words.begin());
      return true;
    }
  }
  return ReadNumber(entry, kChoiceField, condition.choice, fault);
}

[[nodiscard]] std::optional<RuleCondition> ReadCondition(const YAML::Node& entry) {
  if (!entry.IsMap()) {
    return std::nullopt;
  }
  RuleCondition condition;
  FieldFault fault;
  const bool read =
      ReadWord(entry, kSubjectField, kSubjectWords, condition.subject, fault) &&
      ReadWord(entry, kPredicateField, kPredicateWords, condition.predicate, fault) &&
      ReadWord(entry, kComparisonField, kComparisonWords, condition.comparison, fault) &&
      ReadNumber(entry, kNumberField, condition.number, fault) &&
      ReadNumber(entry, kSecondsField, condition.seconds, fault) &&
      ReadNumber(entry, kCellsField, condition.cells, fault) &&
      ReadNumber(entry, kIdField, condition.id, fault) && ReadChoice(entry, condition, fault);
  CapReadCells(condition.cells, ro::sim::kRuleCellsMost);
  if (!read || !ro::sim::ValidCondition(condition)) {
    return std::nullopt;
  }
  return condition;
}

void EmitCondition(YAML::Emitter& out, const RuleCondition& condition) {
  out << YAML::BeginMap;
  out << YAML::Key << kSubjectField << YAML::Value << WordOf(kSubjectWords, condition.subject);
  out << YAML::Key << kPredicateField << YAML::Value
      << WordOf(kPredicateWords, condition.predicate);
  out << YAML::Key << kComparisonField << YAML::Value
      << WordOf(kComparisonWords, condition.comparison);
  out << YAML::Key << kNumberField << YAML::Value << condition.number;
  out << YAML::Key << kSecondsField << YAML::Value << condition.seconds;
  out << YAML::Key << kCellsField << YAML::Value << condition.cells;
  out << YAML::Key << kIdField << YAML::Value << condition.id;
  out << YAML::Key << kChoiceField << YAML::Value;
  const std::span<const std::string_view> words = ChoiceWordsOf(condition.predicate);
  if (!words.empty() && condition.choice >= 0 &&
      static_cast<std::size_t>(condition.choice) < words.size()) {
    out << std::string(words[static_cast<std::size_t>(condition.choice)]);
  } else {
    out << condition.choice;
  }
  out << YAML::EndMap;
}

}  // namespace

// UNE RÈGLE SEULE, voir l'en-tête.
void EmitRule(YAML::Emitter& out, const CompanionRule& rule, const YAML::Node& replaced) {
  out << YAML::BeginMap;
  out << YAML::Key << kActionField << YAML::Value << WordOf(kActionWords, rule.action);
  out << YAML::Key << kAimField << YAML::Value << WordOf(kAimWords, rule.aim);
  out << YAML::Key << kRuleSkillField << YAML::Value << rule.skill_id;
  out << YAML::Key << kRuleLevelField << YAML::Value << rule.skill_level;
  out << YAML::Key << kStepField << YAML::Value << rule.step_cells;
  out << YAML::Key << kDelayField << YAML::Value << rule.own_delay_ms;
  out << YAML::Key << kPerTargetField << YAML::Value << rule.per_target_most;
  out << YAML::Key << kIgnoresReserveField << YAML::Value << rule.ignores_sp_reserve;
  out << YAML::Key << kConditionsField << YAML::Value << YAML::BeginSeq;
  for (const RuleCondition& condition : rule.conditions) {
    EmitCondition(out, condition);
  }
  out << YAML::EndSeq;
  if (replaced && replaced.IsMap()) {
    for (const auto& field : replaced) {
      if (!field.first.IsScalar() ||
          std::ranges::find(kRuleFields, field.first.Scalar()) == kRuleFields.end()) {
        out << YAML::Key << field.first << YAML::Value << field.second;
      }
    }
  }
  out << YAML::EndMap;
}

std::string Describe(const FieldFault& fault) {
  return std::string(fault.unknown_word ? "valeur inconnue pour " : "nombre illisible pour ") +
         fault.field;
}

std::optional<CompanionRule> ReadRule(const YAML::Node& entry, RuleReadFailure& failure) {
  if (!entry.IsMap()) {
    failure = RuleReadFailure{.why = RuleRejection::kNotARule};
    return std::nullopt;
  }
  CompanionRule rule;
  FieldFault fault;
  const bool read = ReadWord(entry, kActionField, kActionWords, rule.action, fault) &&
                    ReadWord(entry, kAimField, kAimWords, rule.aim, fault) &&
                    ReadNumber(entry, kRuleSkillField, rule.skill_id, fault) &&
                    ReadNumber(entry, kRuleLevelField, rule.skill_level, fault) &&
                    ReadNumber(entry, kStepField, rule.step_cells, fault) &&
                    ReadNumber(entry, kDelayField, rule.own_delay_ms, fault) &&
                    ReadNumber(entry, kPerTargetField, rule.per_target_most, fault) &&
                    ReadNumber(entry, kIgnoresReserveField, rule.ignores_sp_reserve, fault);
  if (!read) {
    failure = RuleReadFailure{
        .why = fault.unknown_word ? RuleRejection::kUnknownWord : RuleRejection::kBadNumber,
        .field = fault.field};
    return std::nullopt;
  }
  CapReadCells(rule.step_cells, ro::sim::kRuleCellsMost);
  const YAML::Node conditions = entry[kConditionsField];
  if (!conditions || !conditions.IsSequence()) {
    failure = RuleReadFailure{.why = RuleRejection::kNoCondition};
    return std::nullopt;
  }
  for (const YAML::Node& item : conditions) {
    std::optional<RuleCondition> condition = ReadCondition(item);
    if (!condition.has_value()) {
      failure = RuleReadFailure{.why = RuleRejection::kBadCondition};
      return std::nullopt;
    }
    rule.conditions.push_back(*condition);
  }
  if (!ro::sim::ValidRule(rule)) {
    failure = RuleReadFailure{.why = RuleRejection::kOutOfFrame};
    return std::nullopt;
  }
  return rule;
}

std::string UnknownRuleFields(const YAML::Node& entry) {
  if (!entry.IsMap()) {
    return std::string();
  }
  YAML::Emitter out;
  out << YAML::BeginMap;
  bool any = false;
  for (const auto& field : entry) {
    if (!field.first.IsScalar() ||
        std::ranges::find(kRuleFields, field.first.Scalar()) == kRuleFields.end()) {
      out << YAML::Key << field.first << YAML::Value << field.second;
      any = true;
    }
  }
  out << YAML::EndMap;
  return any && out.good() ? std::string(out.c_str()) : std::string();
}

void EmitRules(YAML::Emitter& out, const YAML::Node& existing,
               const std::vector<CompanionRule>& rules, const std::vector<std::string>& extras) {
  const bool carried = extras.size() == rules.size();
  EmitKeepingRejected(
      out, existing, rules, ro::sim::kCompanionRuleMost,
      [](const YAML::Node& entry) {
        RuleReadFailure failure;
        return ReadRule(entry, failure).has_value();
      },
      [&out, &rules, &extras, carried](const CompanionRule& rule, const YAML::Node& replaced) {
        if (!carried) {
          EmitRule(out, rule, replaced);
          return;
        }
        const auto index = static_cast<std::size_t>(&rule - rules.data());
        YAML::Node unknown;
        if (!extras[index].empty()) {
          try {
            unknown = YAML::Load(extras[index]);
          } catch (const YAML::Exception&) {
            unknown = YAML::Node();
          }
        }
        EmitRule(out, rule, unknown);
      });
}

bool ReadableFriend(const YAML::Node& entry) {
  return entry.IsScalar() && ro::sim::ValidFriendName(entry.Scalar());
}

void EmitFriends(YAML::Emitter& out, const YAML::Node& existing,
                 const std::vector<std::string>& friends) {
  EmitKeepingRejected(out, existing, friends, ro::sim::kCompanionFriendMost, &ReadableFriend,
                      [&out](const std::string& name, const YAML::Node&) { out << name; });
}

}  // namespace moonclient::companion_yaml
