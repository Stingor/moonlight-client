// LES MOTS DES RÈGLES SI/ALORS : ce que la fenêtre « IA du compagnon »
// propose dans ses listes, et la phrase qui dit une règle. Voir
// `game/companion_ai_fields.h`.

#include <algorithm>
#include <array>
#include <cstdio>
#include <functional>
#include <initializer_list>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "ro/core/i18n.h"
#include "ro/sim/skill_book.h"

#include "game/companion_ai_fields.h"

namespace moonclient {
namespace {

using ro::sim::CompanionRule;
using ro::sim::RuleAction;
using ro::sim::RuleAim;
using ro::sim::RuleComparison;
using ro::sim::RuleCondition;
using ro::sim::RulePredicate;
using ro::sim::RuleSubject;
using ro::ui::RuleChoice;
using ro::ui::RuleConditionKind;
using ro::ui::RuleListField;
using ro::ui::RuleNumberField;
using ro::ui::RuleNumberSlot;

constexpr std::size_t kPhraseBytes = 256;
constexpr int kAimCount = static_cast<int>(ro::sim::kLastRuleAim) + 1;
constexpr std::uint32_t kMsPerSecond = 1000;
// Les rangs d'un monstre que la fiche du serveur donne, avant le MVP.
constexpr int kOrdinaryRank = 0;
constexpr int kBossRank = 1;

[[nodiscard]] int Rank(auto value) noexcept { return static_cast<int>(value); }

// ── LES LIBELLÉS ────────────────────────────────────────────────────────────

[[nodiscard]] const char* AimLabel(RuleAim aim) {
  switch (aim) {
    case RuleAim::kTarget:
      return ro::core::Tr("la cible");
    case RuleAim::kSelf:
      return ro::core::Tr("lui-même");
    case RuleAim::kOwner:
      return ro::core::Tr("le maître");
    case RuleAim::kOwnerTarget:
      return ro::core::Tr("ce que le maître attaque");
    case RuleAim::kOwnerAttacker:
      return ro::core::Tr("ce qui attaque le maître");
    case RuleAim::kFriend:
      return ro::core::Tr("un ami");
    case RuleAim::kCellUnderTarget:
      return ro::core::Tr("la case sous la cible");
    case RuleAim::kCellUnderOwner:
      return ro::core::Tr("la case sous le maître");
    case RuleAim::kBestCell:
      return ro::core::Tr("la meilleure case");
  }
  return "?";
}

[[nodiscard]] const char* ActivityLabel(int choice) {
  switch (static_cast<ro::sim::CompanionActivity>(choice)) {
    case ro::sim::CompanionActivity::kResting:
      return ro::core::Tr("au repos");
    case ro::sim::CompanionActivity::kChasing:
      return ro::core::Tr("en poursuite");
    case ro::sim::CompanionActivity::kFighting:
      return ro::core::Tr("en combat");
  }
  return "?";
}

// LE RANG D'UN MONSTRE selon sa fiche : ordinaire, mini-boss, puis MVP.
[[nodiscard]] const char* RankLabel(int choice) {
  switch (choice) {
    case kOrdinaryRank:
      return ro::core::Tr("un monstre ordinaire");
    case kBossRank:
      return ro::core::Tr("un mini-boss");
    default:
      break;
  }
  return ro::core::Tr("un MVP");
}

[[nodiscard]] const char* WhomLabel(int choice) {
  switch (static_cast<ro::sim::RuleWhom>(choice)) {
    case ro::sim::RuleWhom::kOwner:
      return ro::core::Tr("le maître");
    case ro::sim::RuleWhom::kCompanion:
      return ro::core::Tr("le compagnon");
    case ro::sim::RuleWhom::kFriend:
      return ro::core::Tr("un ami");
  }
  return "?";
}

[[nodiscard]] const char* AroundLabel(int choice) {
  switch (static_cast<ro::sim::RuleAround>(choice)) {
    case ro::sim::RuleAround::kCompanion:
      return ro::core::Tr("du compagnon");
    case ro::sim::RuleAround::kOwner:
      return ro::core::Tr("du maître");
    case ro::sim::RuleAround::kTarget:
      return ro::core::Tr("de la cible");
  }
  return "?";
}

[[nodiscard]] std::vector<RuleChoice> Comparisons(
    std::initializer_list<std::pair<RuleComparison, const char*>> each) {
  std::vector<RuleChoice> choices;
  for (const auto& [comparison, label] : each) {
    choices.push_back(RuleChoice{.value = Rank(comparison), .label = label});
  }
  return choices;
}

[[nodiscard]] std::vector<RuleChoice> BelowAbove() {
  return Comparisons({{RuleComparison::kBelow, ro::core::Tr("sous")},
                      {RuleComparison::kAbove, ro::core::Tr("au-dessus de")}});
}
[[nodiscard]] std::vector<RuleChoice> IsIsNot(const char* is, const char* is_not) {
  return Comparisons({{RuleComparison::kIs, is}, {RuleComparison::kIsNot, is_not}});
}
[[nodiscard]] std::vector<RuleChoice> OnlyIs(const char* is) {
  return Comparisons({{RuleComparison::kIs, is}});
}

[[nodiscard]] RuleNumberSlot Slot(RuleNumberField field, int least, int most, const char* label) {
  return RuleNumberSlot{.field = field, .least = least, .most = most, .label = label};
}

// ── LES PHRASES ─────────────────────────────────────────────────────────────

[[nodiscard]] const char* Sign(RuleComparison comparison) {
  return comparison == RuleComparison::kAbove ? ">" : "<";
}

[[nodiscard]] std::string ChoiceWord(const RuleCondition& condition, const RuleNames& names) {
  switch (condition.predicate) {
    case RulePredicate::kRace:
      return names.race ? names.race(condition.choice) : std::to_string(condition.choice);
    case RulePredicate::kElement:
      return names.element ? names.element(condition.choice) : std::to_string(condition.choice);
    case RulePredicate::kSize:
      return names.size ? names.size(condition.choice) : std::to_string(condition.choice);
    case RulePredicate::kMonsterRank:
      return RankLabel(condition.choice);
    case RulePredicate::kMonsterJob:
      return names.monster ? names.monster(condition.id) : std::to_string(condition.id);
    default:
      break;
  }
  return std::string();
}

[[nodiscard]] std::string ConditionPhrase(const RuleCondition& condition, const RuleNames& names) {
  std::array<char, kPhraseBytes> line{};
  const bool is = condition.comparison != RuleComparison::kIsNot;
  const bool owner = condition.subject == RuleSubject::kOwner;
  switch (condition.predicate) {
    case RulePredicate::kHpPercent:
      if (owner) {
        std::snprintf(line.data(), line.size(), ro::core::Tr("PV du maître %s %d %%"),
                      Sign(condition.comparison), condition.number);
      } else if (condition.subject == RuleSubject::kCompanion) {
        std::snprintf(line.data(), line.size(), ro::core::Tr("PV du compagnon %s %d %%"),
                      Sign(condition.comparison), condition.number);
      } else {
        std::snprintf(line.data(), line.size(), ro::core::Tr("PV de la cible %s %d %%"),
                      Sign(condition.comparison), condition.number);
      }
      break;
    case RulePredicate::kSpPercent:
      std::snprintf(line.data(), line.size(), ro::core::Tr("SP du compagnon %s %d %%"),
                    Sign(condition.comparison), condition.number);
      break;
    case RulePredicate::kSpPoints:
      std::snprintf(line.data(), line.size(), ro::core::Tr("SP du compagnon %s %d"),
                    Sign(condition.comparison), condition.number);
      break;
    case RulePredicate::kAttackedFor:
      std::snprintf(line.data(), line.size(),
                    owner ? ro::core::Tr("le maître est attaqué depuis %d s")
                          : ro::core::Tr("le compagnon est attaqué depuis %d s"),
                    condition.seconds);
      break;
    case RulePredicate::kAttackersWeight:
      std::snprintf(line.data(), line.size(),
                    owner ? ro::core::Tr("au moins %d monstres attaquent le maître")
                          : ro::core::Tr("au moins %d monstres attaquent le compagnon"),
                    condition.number);
      break;
    case RulePredicate::kHasStatus: {
      const std::string status =
          names.status ? names.status(condition.id) : std::to_string(condition.id);
      if (owner) {
        std::snprintf(
            line.data(), line.size(),
            is ? ro::core::Tr("le maître porte %s") : ro::core::Tr("le maître ne porte pas %s"),
            status.c_str());
      } else {
        std::snprintf(line.data(), line.size(),
                      is ? ro::core::Tr("le compagnon porte %s")
                         : ro::core::Tr("le compagnon ne porte pas %s"),
                      status.c_str());
      }
      break;
    }
    case RulePredicate::kSitting:
      return is ? ro::core::Tr("le maître est assis") : ro::core::Tr("le maître n'est pas assis");
    case RulePredicate::kActivity:
      std::snprintf(
          line.data(), line.size(),
          is ? ro::core::Tr("le compagnon est %s") : ro::core::Tr("le compagnon n'est pas %s"),
          ActivityLabel(condition.choice));
      break;
    case RulePredicate::kCastWithin: {
      const std::string skill =
          names.skill ? names.skill(condition.id) : std::to_string(condition.id);
      std::snprintf(line.data(), line.size(),
                    is ? ro::core::Tr("%s lancé il y a moins de %d s")
                       : ro::core::Tr("%s non lancé depuis %d s"),
                    skill.c_str(), condition.seconds);
      break;
    }
    case RulePredicate::kAppearedWithin:
      std::snprintf(line.data(), line.size(),
                    ro::core::Tr("le compagnon est arrivé il y a moins de %d s"),
                    condition.seconds);
      break;
    case RulePredicate::kFartherThan:
      std::snprintf(line.data(), line.size(),
                    ro::core::Tr("le compagnon est à plus de %d cases du maître"), condition.cells);
      break;
    case RulePredicate::kRace:
    case RulePredicate::kElement:
    case RulePredicate::kSize:
    case RulePredicate::kMonsterRank:
    case RulePredicate::kMonsterJob:
      std::snprintf(line.data(), line.size(),
                    is ? ro::core::Tr("la cible est %s") : ro::core::Tr("la cible n'est pas %s"),
                    ChoiceWord(condition, names).c_str());
      break;
    case RulePredicate::kCasting:
      return is ? ro::core::Tr("la cible incante") : ro::core::Tr("la cible n'incante pas");
    case RulePredicate::kAttacks:
      std::snprintf(
          line.data(), line.size(),
          is ? ro::core::Tr("la cible attaque %s") : ro::core::Tr("la cible n'attaque pas %s"),
          WhomLabel(condition.choice));
      break;
    case RulePredicate::kEnemiesNear:
      std::snprintf(line.data(), line.size(),
                    ro::core::Tr("au moins %d ennemis à moins de %d cases %s"), condition.number,
                    condition.cells, AroundLabel(condition.choice));
      break;
  }
  return line.data();
}

[[nodiscard]] std::string ActionPhrase(const CompanionRule& rule, const RuleNames& names) {
  std::array<char, kPhraseBytes> line{};
  switch (rule.action) {
    case RuleAction::kAttack:
      std::snprintf(line.data(), line.size(), ro::core::Tr("attaquer %s"), AimLabel(rule.aim));
      break;
    case RuleAction::kCastSkill: {
      const std::string skill =
          names.skill ? names.skill(rule.skill_id) : std::to_string(rule.skill_id);
      if (rule.skill_level == 0) {
        std::snprintf(line.data(), line.size(), ro::core::Tr("lancer %s sur %s"), skill.c_str(),
                      AimLabel(rule.aim));
      } else {
        std::snprintf(line.data(), line.size(), ro::core::Tr("lancer %s niv. %d sur %s"),
                      skill.c_str(), static_cast<int>(rule.skill_level), AimLabel(rule.aim));
      }
      break;
    }
    case RuleAction::kReturnToOwner:
      return ro::core::Tr("revenir au maître");
    case RuleAction::kStepAway:
      std::snprintf(line.data(), line.size(), ro::core::Tr("s'écarter de %s de %d cases"),
                    AimLabel(rule.aim), rule.step_cells);
      break;
    case RuleAction::kDoNothing:
      return ro::core::Tr("rester passif");
  }
  return line.data();
}

[[nodiscard]] std::string AttributesPhrase(const CompanionRule& rule) {
  std::vector<std::string> parts;
  std::array<char, kPhraseBytes> line{};
  if (rule.own_delay_ms > 0) {
    std::snprintf(line.data(), line.size(), ro::core::Tr("toutes les %u s au plus"),
                  static_cast<unsigned>(rule.own_delay_ms / kMsPerSecond));
    parts.emplace_back(line.data());
  }
  if (rule.per_target_most > 0) {
    std::snprintf(line.data(), line.size(), ro::core::Tr("%d fois au plus par cible"),
                  rule.per_target_most);
    parts.emplace_back(line.data());
  }
  if (rule.ignores_sp_reserve) {
    parts.emplace_back(ro::core::Tr("ignore la SP à garder"));
  }
  std::string joined;
  for (const std::string& part : parts) {
    joined += joined.empty() ? " (" : "; ";
    joined += part;
  }
  return joined.empty() ? joined : joined + ")";
}

// LES CIBLES D'ACTION QU'UN SKILL DE CE GENRE PERMET.
[[nodiscard]] std::vector<RuleChoice> AimsFor(std::uint16_t targeting) {
  std::vector<RuleChoice> aims;
  for (int rank = 0; rank < kAimCount; ++rank) {
    const auto aim = static_cast<RuleAim>(rank);
    if ((targeting & ro::sim::RuleAimTargeting(aim)) != 0) {
      aims.push_back(RuleChoice{.value = rank, .label = AimLabel(aim)});
    }
  }
  return aims;
}

[[nodiscard]] std::vector<RuleChoice> EnemyAims() {
  std::vector<RuleChoice> aims;
  for (const RuleAim aim : {RuleAim::kTarget, RuleAim::kOwnerTarget, RuleAim::kOwnerAttacker}) {
    aims.push_back(RuleChoice{.value = Rank(aim), .label = AimLabel(aim)});
  }
  return aims;
}

// UNE LISTE D'IDENTIFIANTS SANS DOUBLON : `seen` retient ceux qu'elle porte.
void AddNamed(std::vector<RuleChoice>& into, std::unordered_set<int>& seen, std::uint16_t id,
              const std::function<std::string(std::uint16_t)>& name) {
  if (id == 0 || !seen.insert(id).second) {
    return;
  }
  into.push_back(RuleChoice{.value = id, .label = name ? name(id) : std::to_string(id)});
}

// LES IDENTIFIANTS QUE LES RÈGLES NOMMENT : statuts, monstres, skills.
[[nodiscard]] std::vector<std::uint16_t> NamedByRules(std::span<const CompanionRule> rules) {
  std::vector<std::uint16_t> named;
  for (const CompanionRule& rule : rules) {
    for (const RuleCondition& condition : rule.conditions) {
      if (condition.predicate == RulePredicate::kHasStatus ||
          condition.predicate == RulePredicate::kMonsterJob ||
          condition.predicate == RulePredicate::kCastWithin) {
        named.push_back(condition.id);
      }
    }
  }
  return named;
}

[[nodiscard]] std::vector<RuleChoice> Ranked(int count, const std::function<std::string(int)>& name,
                                             const char* (*fallback)(int)) {
  std::vector<RuleChoice> choices;
  for (int rank = 0; rank < count; ++rank) {
    choices.push_back(
        RuleChoice{.value = rank, .label = name ? name(rank) : std::string(fallback(rank))});
  }
  return choices;
}

[[nodiscard]] const char* NoWord(int) { return "?"; }

// LA SORTE DÉROULE UNE LISTE : la liste qu'elle désigne se pose ensuite.
RuleConditionKind& Listed(RuleConditionKind& kind, RuleListField list) {
  kind.list = list;
  return kind;
}

}  // namespace

std::string RuleSentence(const CompanionRule& rule, const RuleNames& names) {
  std::string conditions;
  for (const RuleCondition& condition : rule.conditions) {
    if (!conditions.empty()) {
      conditions += ' ';
      conditions += ro::core::Tr("et");
      conditions += ' ';
    }
    conditions += ConditionPhrase(condition, names);
  }
  const std::string action = ActionPhrase(rule, names);
  std::array<char, kPhraseBytes * 2> line{};
  std::snprintf(line.data(), line.size(), ro::core::Tr("Si %s, alors %s"), conditions.c_str(),
                action.c_str());
  return line.data() + AttributesPhrase(rule);
}

bool RuleVocabularyStore::Build(std::span<const RuleSkillFact> skills,
                                std::span<const std::uint16_t> statuses,
                                std::span<const std::uint16_t> monsters,
                                std::span<const CompanionRule> rules, const RuleNames& names) {
  // 🔴 RIEN NE SE REFAIT TANT QUE LES ENTRÉES NE CHANGENT PAS : le livre de
  // skills, les statuts et les monstres connus, ce que les règles nomment, la
  // langue.
  std::vector<std::uint16_t> named = NamedByRules(rules);
  const std::size_t language = ro::core::LanguageSerial();
  if (built_ && std::ranges::equal(skills, seen_skills_) &&
      std::ranges::equal(statuses, seen_statuses_) &&
      std::ranges::equal(monsters, seen_monsters_) && named == seen_named_ &&
      language == seen_language_) {
    return false;
  }
  built_ = true;
  seen_skills_.assign(skills.begin(), skills.end());
  seen_statuses_.assign(statuses.begin(), statuses.end());
  seen_monsters_.assign(monsters.begin(), monsters.end());
  seen_named_ = std::move(named);
  seen_language_ = language;

  // ── LES LISTES ──
  std::unordered_set<int> seen_statuses;
  std::unordered_set<int> seen_monsters;
  std::unordered_set<int> seen_skills;
  statuses_.clear();
  for (const std::uint16_t status : statuses) {
    AddNamed(statuses_, seen_statuses, status, names.status);
  }
  monsters_.clear();
  const auto add_monster = [this, &seen_monsters, &names](std::uint16_t monster) {
    const std::size_t before = monsters_.size();
    AddNamed(monsters_, seen_monsters, monster, names.monster);
    if (monsters_.size() > before && names.monster_detail) {
      monsters_.back().detail = names.monster_detail(monster);
    }
  };
  for (const std::uint16_t monster : monsters) {
    add_monster(monster);
  }
  cast_skills_.clear();
  skills_.clear();
  for (const RuleSkillFact& skill : skills) {
    if (skill.level <= 0) {
      continue;
    }
    AddNamed(cast_skills_, seen_skills, skill.skill_id, names.skill);
    std::vector<RuleChoice> aims = AimsFor(skill.targeting);
    if (aims.empty() || ro::sim::NeverCastAutomatically(skill.skill_id)) {
      continue;
    }
    skills_.push_back(ro::ui::RuleSkillChoice{
        .skill_id = skill.skill_id,
        .label = names.skill ? names.skill(skill.skill_id) : std::to_string(skill.skill_id),
        .learned_level = std::min(skill.level, ro::sim::kRuleSkillLevelMost),
        .aims = std::move(aims)});
  }
  for (const CompanionRule& rule : rules) {
    for (const RuleCondition& condition : rule.conditions) {
      if (condition.predicate == RulePredicate::kHasStatus) {
        AddNamed(statuses_, seen_statuses, condition.id, names.status);
      } else if (condition.predicate == RulePredicate::kMonsterJob) {
        add_monster(condition.id);
      } else if (condition.predicate == RulePredicate::kCastWithin) {
        AddNamed(cast_skills_, seen_skills, condition.id, names.skill);
      }
    }
  }
  activities_ = Ranked(Rank(ro::sim::CompanionActivity::kFighting) + 1, nullptr, &ActivityLabel);
  races_ = Ranked(ro::sim::kMonsterRaceCount, names.race, &NoWord);
  elements_ = Ranked(ro::sim::kMonsterElementCount, names.element, &NoWord);
  sizes_ = Ranked(ro::sim::kMonsterSizeCount, names.size, &NoWord);
  ranks_ = Ranked(ro::sim::kMonsterRankCount, nullptr, &RankLabel);
  whom_ = Ranked(Rank(ro::sim::RuleWhom::kFriend) + 1, nullptr, &WhomLabel);
  around_ = Ranked(Rank(ro::sim::RuleAround::kTarget) + 1, nullptr, &AroundLabel);

  // ── LES SORTES DE CONDITION, celles que `ValidCondition` permet ──
  using ro::core::Tr;
  conditions_.clear();
  const auto add =
      [this](
          RuleSubject subject, RulePredicate predicate, const char* label,
          std::vector<RuleChoice> comparisons,
          std::array<RuleNumberSlot, ro::ui::kRuleNumberSlots> numbers = {}) -> RuleConditionKind& {
    return conditions_.emplace_back(RuleConditionKind{.subject = Rank(subject),
                                                      .predicate = Rank(predicate),
                                                      .label = label,
                                                      .comparisons = std::move(comparisons),
                                                      .numbers = std::move(numbers)});
  };
  const RuleNumberSlot percent =
      Slot(RuleNumberField::kNumber, 0, ro::sim::kRulePercentMost, Tr("pour cent"));
  const RuleNumberSlot seconds =
      Slot(RuleNumberField::kSeconds, 0, ro::sim::kRuleSecondsMost, Tr("secondes"));
  const RuleNumberSlot recent =
      Slot(RuleNumberField::kSeconds, 1, ro::sim::kRuleSecondsMost, Tr("secondes"));
  const RuleNumberSlot weight =
      Slot(RuleNumberField::kNumber, 1, ro::sim::kRuleCountMost, Tr("(poids)"));
  add(RuleSubject::kOwner, RulePredicate::kHpPercent, Tr("PV du maître"), BelowAbove(), {percent});
  add(RuleSubject::kCompanion, RulePredicate::kHpPercent, Tr("PV du compagnon"), BelowAbove(),
      {percent});
  add(RuleSubject::kTarget, RulePredicate::kHpPercent, Tr("PV de la cible"),
      Comparisons({{RuleComparison::kBelow, Tr("sous")}}), {percent});
  add(RuleSubject::kCompanion, RulePredicate::kSpPercent, Tr("SP du compagnon (%)"), BelowAbove(),
      {percent});
  add(RuleSubject::kCompanion, RulePredicate::kSpPoints, Tr("SP du compagnon (points)"),
      BelowAbove(), {Slot(RuleNumberField::kNumber, 0, ro::sim::kRulePointsMost, Tr("SP"))});
  add(RuleSubject::kOwner, RulePredicate::kAttackedFor, Tr("Le maître est attaqué"),
      OnlyIs(Tr("depuis au moins")), {seconds});
  add(RuleSubject::kCompanion, RulePredicate::kAttackedFor, Tr("Le compagnon est attaqué"),
      OnlyIs(Tr("depuis au moins")), {seconds});
  add(RuleSubject::kOwner, RulePredicate::kAttackersWeight, Tr("Monstres qui attaquent le maître"),
      OnlyIs(Tr("au moins")), {weight});
  add(RuleSubject::kCompanion, RulePredicate::kAttackersWeight,
      Tr("Monstres qui attaquent le compagnon"), OnlyIs(Tr("au moins")), {weight});
  // Une liste vide ne propose rien qu'une condition puisse retenir.
  if (!statuses_.empty()) {
    Listed(add(RuleSubject::kOwner, RulePredicate::kHasStatus, Tr("Statut du maître"),
               IsIsNot(Tr("porte"), Tr("ne porte pas")), {}),
           RuleListField::kId)
        .choices = statuses_;
    RuleConditionKind& companion_status =
        Listed(add(RuleSubject::kCompanion, RulePredicate::kHasStatus, Tr("Statut du compagnon"),
                   IsIsNot(Tr("porte"), Tr("ne porte pas")), {}),
               RuleListField::kId);
    companion_status.choices = statuses_;
    companion_status.tip =
        Tr("Le client ne voit pas tous les statuts du compagnon : sans la "
           "liste que le serveur rend, « porte » et « ne porte pas » sont "
           "tous deux faux.");
  }
  add(RuleSubject::kOwner, RulePredicate::kSitting, Tr("Le maître est assis"),
      IsIsNot(Tr("oui"), Tr("non")));
  Listed(add(RuleSubject::kCompanion, RulePredicate::kActivity, Tr("Activité du compagnon"),
             IsIsNot(Tr("est"), Tr("n'est pas")), {}),
         RuleListField::kChoice)
      .choices = activities_;
  if (!cast_skills_.empty()) {
    Listed(
        add(RuleSubject::kCompanion, RulePredicate::kCastWithin, Tr("Skill lancé par le compagnon"),
            IsIsNot(Tr("il y a moins de"), Tr("non lancé depuis")), {recent}),
        RuleListField::kId)
        .choices = cast_skills_;
  }
  add(RuleSubject::kCompanion, RulePredicate::kAppearedWithin, Tr("Le compagnon est arrivé"),
      OnlyIs(Tr("il y a moins de")), {recent});
  add(RuleSubject::kCompanion, RulePredicate::kFartherThan, Tr("Distance au maître"),
      OnlyIs(Tr("plus de")),
      {Slot(RuleNumberField::kCells, 0, ro::sim::kRuleCellsMost, Tr("cases"))});
  Listed(add(RuleSubject::kTarget, RulePredicate::kRace, Tr("Race de la cible"),
             IsIsNot(Tr("est"), Tr("n'est pas")), {}),
         RuleListField::kChoice)
      .choices = races_;
  Listed(add(RuleSubject::kTarget, RulePredicate::kElement, Tr("Élément de la cible"),
             IsIsNot(Tr("est"), Tr("n'est pas")), {}),
         RuleListField::kChoice)
      .choices = elements_;
  Listed(add(RuleSubject::kTarget, RulePredicate::kSize, Tr("Taille de la cible"),
             IsIsNot(Tr("est"), Tr("n'est pas")), {}),
         RuleListField::kChoice)
      .choices = sizes_;
  Listed(add(RuleSubject::kTarget, RulePredicate::kMonsterRank, Tr("Rang de la cible"),
             IsIsNot(Tr("est"), Tr("n'est pas")), {}),
         RuleListField::kChoice)
      .choices = ranks_;
  if (!monsters_.empty()) {
    Listed(add(RuleSubject::kTarget, RulePredicate::kMonsterJob, Tr("Monstre de la cible"),
               IsIsNot(Tr("est"), Tr("n'est pas")), {}),
           RuleListField::kId)
        .choices = monsters_;
  }
  add(RuleSubject::kTarget, RulePredicate::kCasting, Tr("La cible incante"),
      IsIsNot(Tr("oui"), Tr("non")));
  Listed(add(RuleSubject::kTarget, RulePredicate::kAttacks, Tr("Ce que la cible attaque"),
             IsIsNot(Tr("est"), Tr("n'est pas")), {}),
         RuleListField::kChoice)
      .choices = whom_;
  Listed(add(RuleSubject::kSurroundings, RulePredicate::kEnemiesNear, Tr("Ennemis autour"),
             OnlyIs(Tr("au moins")),
             {Slot(RuleNumberField::kNumber, 1, ro::sim::kRuleCountMost, Tr("ennemis (poids)")),
              Slot(RuleNumberField::kCells, 1, ro::sim::kRuleCellsMost, Tr("à moins de (cases)"))}),
         RuleListField::kChoice)
      .choices = around_;

  // ── LES ACTIONS ──
  actions_.clear();
  actions_.push_back(ro::ui::RuleActionKind{
      .action = Rank(RuleAction::kAttack),
      .label = Tr("Attaquer"),
      .aims = EnemyAims(),
      .tip = Tr("Il attaque l'ennemi visé, en continu, tant que la règle décide.")});
  if (!skills_.empty()) {
    actions_.push_back(ro::ui::RuleActionKind{
        .action = Rank(RuleAction::kCastSkill),
        .label = Tr("Lancer un skill"),
        .needs_skill = true,
        .tip = Tr("Il lance ce skill sur la cible d'action, dès qu'il est à portée et que sa SP "
                  "le permet.")});
  }
  actions_.push_back(ro::ui::RuleActionKind{.action = Rank(RuleAction::kReturnToOwner),
                                            .label = Tr("Revenir au maître"),
                                            .tip = Tr("Il revient à côté du maître.")});
  actions_.push_back(ro::ui::RuleActionKind{
      .action = Rank(RuleAction::kStepAway),
      .label = Tr("S'écarter"),
      .needs_step = true,
      .aims = EnemyAims(),
      .tip = Tr("Il s'éloigne de l'ennemi visé du nombre de cases choisi.")});
  actions_.push_back(ro::ui::RuleActionKind{
      .action = Rank(RuleAction::kDoNothing),
      .label = Tr("Rester passif"),
      .tip = Tr("Ni attaque ni skill tant que la règle est vraie, et les règles suivantes se "
                "taisent. Le suivi du maître, la laisse, le repli, la fuite et l'évitement "
                "continuent, et un ordre direct passe toujours.")});
  return true;
}

ro::ui::RuleVocabulary RuleVocabularyStore::view() const {
  return ro::ui::RuleVocabulary{.conditions = conditions_,
                                .actions = actions_,
                                .skills = skills_,
                                .step_most = ro::sim::kRuleCellsMost};
}

}  // namespace moonclient
