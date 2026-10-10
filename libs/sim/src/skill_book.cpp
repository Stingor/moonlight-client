#include "ro/sim/skill_book.h"

#include <algorithm>

namespace ro::sim {

bool SkillNeedsGround(const KnownSkill& skill) { return (skill.targeting & kSkillOnGround) != 0; }

bool SkillTargetsTrap(const KnownSkill& skill) {
  return !SkillNeedsGround(skill) && (skill.targeting & kSkillTrap) != 0;
}

bool SkillCastsOnSelf(const KnownSkill& skill) {
  return !SkillNeedsGround(skill) && !SkillTargetsTrap(skill) &&
         (skill.targeting & kSkillOnSelf) != 0;
}

bool SkillNeedsTarget(const KnownSkill& skill) {
  return !SkillNeedsGround(skill) && !SkillCastsOnSelf(skill) &&
         (skill.targeting & (kSkillOnEnemy | kSkillOnFriend)) != 0;
}

bool SkillIsPassive(const KnownSkill& skill) { return skill.targeting == kSkillPassive; }

void SkillBook::Learn(KnownSkill skill) {
  const auto place = std::ranges::lower_bound(skills_, skill.id, {}, &KnownSkill::id);
  if (place != skills_.end() && place->id == skill.id) {
    if (skill.name.empty()) {
      skill.name = place->name;
    }
    *place = std::move(skill);
    return;
  }
  skills_.insert(place, std::move(skill));
}

bool SkillBook::Forget(std::uint16_t skill_id) {
  const auto place = std::ranges::lower_bound(skills_, skill_id, {}, &KnownSkill::id);
  if (place == skills_.end() || place->id != skill_id) {
    return false;
  }
  skills_.erase(place);
  return true;
}

void SkillBook::Clear() { skills_.clear(); }

void SkillBook::RecordRefusal(SkillRefusal refusal) {
  if (refusals_.size() >= kSkillRefusalsKept) {
    refusals_.erase(refusals_.begin());
  }
  refusal.seq = ++refusal_seq_;
  refusals_.push_back(refusal);
}

void SkillBook::StartCooldown(std::uint16_t skill_id, std::uint32_t duration_ms,
                              std::uint32_t now_ms) {
  const Cooldown fresh{
      .skill_id = skill_id, .started_ms = now_ms, .until_ms = now_ms + duration_ms};
  const auto place = std::ranges::lower_bound(cooldowns_, skill_id, {}, &Cooldown::skill_id);
  if (place != cooldowns_.end() && place->skill_id == skill_id) {
    *place = fresh;
    return;
  }
  cooldowns_.insert(place, fresh);
}

std::uint32_t SkillBook::CooldownRemainingMs(std::uint16_t skill_id, std::uint32_t now_ms) const {
  const auto place = std::ranges::lower_bound(cooldowns_, skill_id, {}, &Cooldown::skill_id);
  if (place == cooldowns_.end() || place->skill_id != skill_id) {
    return 0;
  }
  // L'échéance PASSÉE est le cas ordinaire, et la soustraction non signée en
  // ferait quarante-neuf jours au lieu de zéro.
  return now_ms < place->until_ms ? place->until_ms - now_ms : 0;
}

float SkillBook::CooldownFraction(std::uint16_t skill_id, std::uint32_t now_ms) const {
  const auto place = std::ranges::lower_bound(cooldowns_, skill_id, {}, &Cooldown::skill_id);
  if (place == cooldowns_.end() || place->skill_id != skill_id) {
    return 0.0F;
  }
  // ⚠ UNE DURÉE NULLE N'EST PAS UNE DIVISION PAR ZÉRO À ÉCRIRE : le serveur
  // annonce parfois une attente déjà écoulée, et la part qui reste est alors
  // zéro, pas l'infini.
  if (place->until_ms <= place->started_ms || now_ms >= place->until_ms) {
    return 0.0F;
  }
  const auto whole = static_cast<float>(place->until_ms - place->started_ms);
  const auto left = static_cast<float>(place->until_ms - now_ms);
  return left / whole;
}

const KnownSkill* SkillBook::Find(std::uint16_t skill_id) const {
  const auto place = std::ranges::lower_bound(skills_, skill_id, {}, &KnownSkill::id);
  if (place == skills_.end() || place->id != skill_id) {
    return nullptr;
  }
  return &*place;
}

}  // namespace ro::sim
