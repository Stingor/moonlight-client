#include "ro/sim/skill_facts.h"

#include <utility>

namespace ro::sim {

void SkillFacts::TellCast(std::uint32_t skill_id, std::vector<SkillCastFacts> levels) {
  cast_[skill_id] = std::move(levels);
}

std::span<const SkillCastFacts> SkillFacts::CastOf(std::uint32_t skill_id) const {
  const auto found = cast_.find(skill_id);
  return found == cast_.end() ? std::span<const SkillCastFacts>() : std::span(found->second);
}

void SkillFacts::TellDamage(SkillDamageFacts told) {
  const std::uint32_t skill_id = told.skill_id;
  damage_[skill_id] = std::move(told);
}

const SkillDamageFacts* SkillFacts::DamageOf(std::uint32_t skill_id) const {
  const auto found = damage_.find(skill_id);
  return found == damage_.end() ? nullptr : &found->second;
}

}  // namespace ro::sim
