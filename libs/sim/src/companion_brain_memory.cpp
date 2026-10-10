// LA MÉMOIRE DU CERVEAU D'UN COMPAGNON : ce qui survit à une carte, à un
// changement de profil et à une reconnexion. Voir `ro/sim/companion_memory.h`.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "ro/sim/companion_ai.h"
#include "ro/sim/companion_memory.h"

#include "companion_brain_detail.h"

namespace ro::sim {
namespace {

using companion_detail::SignedElapsedMs;

// UN ÂGE, `elapsed_ms` plus tard ; vide au-delà de l'horizon.
[[nodiscard]] std::optional<std::uint32_t> Older(std::optional<std::uint32_t> ago_ms,
                                                 std::uint64_t elapsed_ms) noexcept {
  if (!ago_ms.has_value() || *ago_ms + elapsed_ms >= kCompanionMemoryHorizonMs) {
    return std::nullopt;
  }
  return static_cast<std::uint32_t>(*ago_ms + elapsed_ms);
}

// UN RESTE, `elapsed_ms` plus tard ; vide une fois échu.
[[nodiscard]] std::optional<std::uint32_t> Shorter(std::optional<std::uint32_t> left_ms,
                                                   std::uint64_t elapsed_ms) noexcept {
  if (!left_ms.has_value() || *left_ms <= elapsed_ms) {
    return std::nullopt;
  }
  return static_cast<std::uint32_t>(*left_ms - elapsed_ms);
}

// L'ÂGE D'UN INSTANT PASSÉ, vu de `now_ms` ; un instant daté après lui a l'âge
// zéro, un instant au-delà de l'horizon n'en a plus.
[[nodiscard]] std::optional<std::uint32_t> AgeOf(std::uint32_t at_ms,
                                                 std::uint32_t now_ms) noexcept {
  const std::int64_t ago_ms = std::max<std::int64_t>(SignedElapsedMs(now_ms, at_ms), 0);
  if (ago_ms >= static_cast<std::int64_t>(kCompanionMemoryHorizonMs)) {
    return std::nullopt;
  }
  return static_cast<std::uint32_t>(ago_ms);
}

// CE QUI RESTE JUSQU'À `until_ms`, vide une fois échu.
[[nodiscard]] std::optional<std::uint32_t> LeftUntil(std::uint32_t until_ms,
                                                     std::uint32_t now_ms) noexcept {
  const std::int64_t left_ms = SignedElapsedMs(until_ms, now_ms);
  if (left_ms <= 0) {
    return std::nullopt;
  }
  return static_cast<std::uint32_t>(left_ms);
}

}  // namespace

CompanionMemory AgedMemory(CompanionMemory memory, std::uint64_t elapsed_ms) {
  for (CompanionRuleMemory& rule : memory.rules) {
    rule.acted_ago_ms = Older(rule.acted_ago_ms, elapsed_ms);
    rule.suspended_for_ms = Shorter(rule.suspended_for_ms, elapsed_ms);
  }
  std::erase_if(memory.rules, [](const CompanionRuleMemory& rule) {
    return !rule.acted_ago_ms.has_value() && !rule.suspended_for_ms.has_value();
  });
  for (CompanionSkillMemory& skill : memory.skills) {
    skill.cast_ago_ms = Older(skill.cast_ago_ms, elapsed_ms);
    skill.suspended_for_ms = Shorter(skill.suspended_for_ms, elapsed_ms);
  }
  std::erase_if(memory.skills, [](const CompanionSkillMemory& skill) {
    return !skill.cast_ago_ms.has_value() && !skill.suspended_for_ms.has_value();
  });
  return memory;
}

CompanionMemory CompanionBrain::Remember(std::uint32_t now_ms) const {
  CompanionMemory memory;
  for (std::size_t index = 0; index < rule_states_.size() && index < rules_seen_.size(); ++index) {
    const RuleState& state = rule_states_[index];
    CompanionRuleMemory rule{.index = index, .rule = rules_seen_[index]};
    if (state.last_acted_ms.has_value()) {
      rule.acted_ago_ms = AgeOf(*state.last_acted_ms, now_ms);
    }
    if (state.suspended_until_ms.has_value()) {
      rule.suspended_for_ms = LeftUntil(*state.suspended_until_ms, now_ms);
    }
    if (rule.acted_ago_ms.has_value() || rule.suspended_for_ms.has_value()) {
      memory.rules.push_back(std::move(rule));
    }
  }
  for (const auto& [skill_id, seen_ms] : skill_seen_ms_) {
    if (const std::optional<std::uint32_t> ago_ms = AgeOf(seen_ms, now_ms); ago_ms.has_value()) {
      memory.skills.push_back(CompanionSkillMemory{.skill_id = skill_id, .cast_ago_ms = ago_ms});
    }
  }
  for (const auto& [skill_id, left_ms] : skills_.SuspensionsLeft(now_ms)) {
    const auto found = std::ranges::find_if(
        memory.skills, [skill_id](const auto& skill) { return skill.skill_id == skill_id; });
    if (found != memory.skills.end()) {
      found->suspended_for_ms = left_ms;
    } else {
      memory.skills.push_back(
          CompanionSkillMemory{.skill_id = skill_id, .suspended_for_ms = left_ms});
    }
  }
  memory.standby = standby_;
  return memory;
}

void CompanionBrain::Recall(const CompanionMemory& memory, std::uint32_t now_ms) {
  // 🔴 LA LISTE VUE SE REFAIT DEPUIS LA MÉMOIRE : chaque règle à son rang, des
  // trous sans condition — qu'aucune règle valable n'égale — entre elles. Le
  // tick suivant la compare à la vraie liste, et chaque état suit sa règle.
  std::size_t size = 0;
  for (const CompanionRuleMemory& rule : memory.rules) {
    size = std::max(size, rule.index + 1);
  }
  rules_seen_.assign(size, CompanionRule{});
  rule_states_.assign(size, RuleState{});
  for (const CompanionRuleMemory& rule : memory.rules) {
    rules_seen_[rule.index] = rule.rule;
    RuleState& state = rule_states_[rule.index];
    if (rule.acted_ago_ms.has_value()) {
      state.last_acted_ms = now_ms - *rule.acted_ago_ms;
    }
    if (rule.suspended_for_ms.has_value()) {
      state.suspended_until_ms = now_ms + *rule.suspended_for_ms;
    }
  }
  deciding_rule_.reset();
  for (const CompanionSkillMemory& skill : memory.skills) {
    if (skill.cast_ago_ms.has_value()) {
      skill_seen_ms_[skill.skill_id] = now_ms - *skill.cast_ago_ms;
    }
    if (skill.suspended_for_ms.has_value()) {
      skills_.Suspend(skill.skill_id, now_ms + *skill.suspended_for_ms);
    }
  }
  standby_ = memory.standby;
}

void CompanionBrain::Restart(const CompanionMemory& memory, std::uint32_t now_ms) {
  const std::optional<std::uint32_t> arrived_ms = arrived_ms_;
  *this = CompanionBrain();
  Recall(memory, now_ms);
  arrived_ms_ = arrived_ms;
}

}  // namespace ro::sim
