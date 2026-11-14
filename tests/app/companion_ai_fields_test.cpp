// LES CHAMPS DE LA FENÊTRE « IA DU COMPAGNON » : la phrase d'une règle, les
// allers-retours entre le profil et la fenêtre, et un vocabulaire qui ne
// propose que ce que l'IA accepte.

#include "game/companion_ai_fields.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/companion_ai.h"
#include "ro/sim/companion_rules.h"
#include "ro/sim/skill_book.h"

using moonclient::RuleNames;
using moonclient::RuleSkillFact;
using ro::sim::CompanionRule;
using ro::sim::RuleCondition;

namespace {

// Des skills de compagnon : un offensif, un de soutien, un « jamais seul », un
// debuff, et un au sol.
constexpr std::uint16_t kCaprice = 8013;
constexpr std::uint16_t kHealingHands = 8002;
constexpr std::uint16_t kBioExplosion = 8016;
constexpr std::uint16_t kProvoke = 8232;
constexpr std::uint16_t kGroundSkill = 8020;
constexpr std::uint16_t kStatus = 12;
constexpr std::uint16_t kPoring = 1002;
constexpr int kHalf = 50;
constexpr int kLevel = 5;

[[nodiscard]] RuleNames Names() {
  RuleNames names;
  names.skill = [](std::uint16_t skill_id) {
    return skill_id == kHealingHands ? std::string("Healing Hands") : std::to_string(skill_id);
  };
  names.monster = [](std::uint16_t) { return std::string("Poring"); };
  return names;
}

[[nodiscard]] std::vector<RuleSkillFact> Skills() {
  return {{.skill_id = kCaprice, .level = kLevel, .targeting = ro::sim::kSkillOnEnemy},
          {.skill_id = kHealingHands, .level = kLevel, .targeting = ro::sim::kSkillOnFriend},
          {.skill_id = kBioExplosion, .level = 1, .targeting = ro::sim::kSkillOnSelf},
          {.skill_id = kProvoke, .level = 1, .targeting = ro::sim::kSkillOnEnemy},
          {.skill_id = kGroundSkill, .level = 1, .targeting = ro::sim::kSkillOnGround}};
}

}  // namespace

TEST_CASE("Champs de l'IA : une règle se lit comme une phrase", "[app][companion_ai_fields]") {
  const CompanionRule rule{
      .action = ro::sim::RuleAction::kCastSkill,
      .aim = ro::sim::RuleAim::kOwner,
      .skill_id = kHealingHands,
      .skill_level = kLevel,
      .conditions = {RuleCondition{.subject = ro::sim::RuleSubject::kOwner,
                                   .predicate = ro::sim::RulePredicate::kHpPercent,
                                   .comparison = ro::sim::RuleComparison::kBelow,
                                   .number = kHalf}}};
  CHECK(moonclient::RuleSentence(rule, Names()) ==
        "Si PV du maître < 50 %, alors lancer Healing Hands niv. 5 sur le maître");

  CompanionRule two = moonclient::DefaultCompanionRule();
  two.conditions.push_back(RuleCondition{.subject = ro::sim::RuleSubject::kTarget,
                                         .predicate = ro::sim::RulePredicate::kMonsterJob,
                                         .comparison = ro::sim::RuleComparison::kIs,
                                         .id = kPoring});
  two.per_target_most = 2;
  CHECK(moonclient::RuleSentence(two, Names()) ==
        "Si PV du compagnon < 30 % et la cible est Poring, alors revenir au maître "
        "(2 fois au plus par cible)");
}

TEST_CASE("Champs de l'IA : une règle fait l'aller-retour par la fenêtre",
          "[app][companion_ai_fields]") {
  CompanionRule rule = moonclient::DefaultCompanionRule();
  rule.conditions.push_back(RuleCondition{.subject = ro::sim::RuleSubject::kSurroundings,
                                          .predicate = ro::sim::RulePredicate::kEnemiesNear,
                                          .comparison = ro::sim::RuleComparison::kIs,
                                          .number = 3,
                                          .cells = 4,
                                          .choice = 1});
  rule.own_delay_ms = 3000;
  rule.ignores_sp_reserve = true;
  CHECK(moonclient::RuleOf(moonclient::RuleFieldsOf(rule)) == rule);
  CHECK(ro::sim::ValidRule(moonclient::DefaultCompanionRule()));
}

TEST_CASE("Champs de l'IA : les réglages font l'aller-retour par la fenêtre",
          "[app][companion_ai_fields]") {
  ro::sim::CompanionAiSettings settings =
      ro::sim::BasicCompanionAiSettings(ro::sim::AiCompanion::kHomunculus, 6001);
  settings.skills.permissions.push_back(
      ro::sim::SkillPermission{.skill_id = kCaprice, .allowed = false, .level = 2});
  settings.skills.permissions.push_back(
      ro::sim::SkillPermission{.skill_id = kProvoke, .allowed = true, .level = 1});
  const std::vector<ro::ui::OffensiveSkillRow> shown{
      {.skill_id = kCaprice, .label = "Caprice", .learned_level = kLevel}};
  ro::ui::CompanionBaseFields fields = moonclient::BaseFieldsOf(settings, shown);
  REQUIRE(fields.permissions.size() == 1);
  CHECK_FALSE(fields.permissions.front().allowed);
  CHECK(moonclient::WithBaseFields(settings, fields) == settings);

  fields.follow_cells = 7;
  fields.permissions.front().allowed = true;
  const ro::sim::CompanionAiSettings changed = moonclient::WithBaseFields(settings, fields);
  CHECK(changed.follow_distance_cells == 7);
  CHECK(changed.skills.permissions.front().allowed);
  // Le skill que la fenêtre ne montre pas garde sa permission.
  CHECK(changed.skills.permissions.back() == settings.skills.permissions.back());
}

TEST_CASE("Champs de l'IA : chaque condition proposée est dans le cadre",
          "[app][companion_ai_fields]") {
  moonclient::RuleVocabularyStore store;
  const std::vector<std::uint16_t> statuses{kStatus};
  const std::vector<std::uint16_t> monsters{kPoring};
  store.Build(Skills(), statuses, monsters, {}, Names());
  const ro::ui::RuleVocabulary vocabulary = store.view();
  REQUIRE_FALSE(vocabulary.conditions.empty());
  for (const ro::ui::RuleConditionKind& kind : vocabulary.conditions) {
    INFO(kind.label);
    REQUIRE_FALSE(kind.comparisons.empty());
    for (const ro::ui::RuleChoice& comparison : kind.comparisons) {
      for (const bool at_most : {false, true}) {
        ro::ui::RuleConditionFields fields{
            .subject = kind.subject, .predicate = kind.predicate, .comparison = comparison.value};
        for (const ro::ui::RuleNumberSlot& slot : kind.numbers) {
          const int value = at_most ? slot.most : slot.least;
          if (slot.field == ro::ui::RuleNumberField::kNumber) {
            fields.number = value;
          } else if (slot.field == ro::ui::RuleNumberField::kSeconds) {
            fields.seconds = value;
          } else if (slot.field == ro::ui::RuleNumberField::kCells) {
            fields.cells = value;
          }
        }
        if (kind.list != ro::ui::RuleListField::kNone) {
          REQUIRE_FALSE(kind.choices.empty());
          const int listed = at_most ? kind.choices.back().value : kind.choices.front().value;
          (kind.list == ro::ui::RuleListField::kId ? fields.id : fields.choice) = listed;
        }
        ro::ui::RuleFields rule{.action = static_cast<int>(ro::sim::RuleAction::kDoNothing)};
        rule.conditions.push_back(fields);
        CHECK(ro::sim::ValidRule(moonclient::RuleOf(rule)));
      }
    }
  }
}

TEST_CASE("Champs de l'IA : les skills d'une règle et leurs cibles d'action",
          "[app][companion_ai_fields]") {
  moonclient::RuleVocabularyStore store;
  store.Build(Skills(), {}, {}, {}, Names());
  const ro::ui::RuleVocabulary vocabulary = store.view();
  std::vector<int> offered;
  for (const ro::ui::RuleSkillChoice& skill : vocabulary.skills) {
    offered.push_back(skill.skill_id);
    REQUIRE_FALSE(skill.aims.empty());
    std::uint16_t targeting = 0;
    for (const RuleSkillFact& fact : Skills()) {
      if (fact.skill_id == skill.skill_id) {
        targeting = fact.targeting;
      }
    }
    for (const ro::ui::RuleChoice& aim : skill.aims) {
      // Chaque cible proposée convient au genre du skill, et la règle tient.
      CHECK((targeting & ro::sim::RuleAimTargeting(static_cast<ro::sim::RuleAim>(aim.value))) != 0);
      const ro::ui::RuleFields fields{
          .action = static_cast<int>(ro::sim::RuleAction::kCastSkill),
          .aim = aim.value,
          .skill_id = skill.skill_id,
          .skill_level = skill.learned_level,
          .conditions = {{.subject = static_cast<int>(ro::sim::RuleSubject::kOwner),
                          .predicate = static_cast<int>(ro::sim::RulePredicate::kSitting),
                          .comparison = static_cast<int>(ro::sim::RuleComparison::kIs)}}};
      CHECK(ro::sim::ValidRule(moonclient::RuleOf(fields)));
    }
  }
  // « Jamais seul » refusé, debuff permis, sol permis.
  CHECK(std::ranges::find(offered, kBioExplosion) == offered.end());
  CHECK(std::ranges::find(offered, kProvoke) != offered.end());
  CHECK(std::ranges::find(offered, kGroundSkill) != offered.end());
  CHECK(std::ranges::find(offered, kHealingHands) != offered.end());
  // Chaque action proposée, à sa première cible, donne une règle qui tient.
  for (const ro::ui::RuleActionKind& action : vocabulary.actions) {
    if (action.needs_skill) {
      continue;
    }
    ro::ui::RuleFields fields{.action = action.action,
                              .aim = action.aims.empty() ? 0 : action.aims.front().value,
                              .step_cells = action.needs_step ? 1 : 0};
    fields.conditions.push_back({.subject = static_cast<int>(ro::sim::RuleSubject::kTarget),
                                 .predicate = static_cast<int>(ro::sim::RulePredicate::kCasting),
                                 .comparison = static_cast<int>(ro::sim::RuleComparison::kIs)});
    CHECK(ro::sim::ValidRule(moonclient::RuleOf(fields)));
  }
}

TEST_CASE("Champs de l'IA : une règle écartée dit sa place et sa raison",
          "[app][companion_ai_fields]") {
  const moonclient::RejectedRule rejected{
      .position = 1, .why = moonclient::RuleRejection::kUnknownWord, .field = "action"};
  CHECK(moonclient::RejectedRuleLine(rejected) ==
        "Règle 2 du fichier : mot inconnu pour « action »");
}

TEST_CASE("Champs de l'IA : les lignes génériques gardent leur rang",
          "[app][companion_ai_fields]") {
  for (int generic = 0; generic < ro::ui::kTacticGenericChoices; ++generic) {
    CHECK(moonclient::GenericIndexOf(moonclient::GenericKeyOf(generic).kind) == generic);
  }
  CHECK(moonclient::GenericIndexOf(ro::sim::TacticKeyKind::kMonster) < 0);
}
