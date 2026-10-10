// Voir l'en-tête.

#include "ro/sim/homunculus.h"

#include <algorithm>
#include <array>

#include "ro/sim/skill_book.h"

namespace ro::sim {
namespace {

// Les bornes HAUTES de chaque palier, incluses.
constexpr int kHateWithPassionMost = 3;
constexpr int kHateMost = 10;
constexpr int kAwkwardMost = 100;
constexpr int kShyMost = 250;
constexpr int kNeutralMost = 750;
constexpr int kCordialMost = 910;

// Le délai d'attaque de référence, et le pas qui en fait une vitesse.
constexpr int kAspdAmotionBaseMs = 2000;
constexpr int kAspdAmotionStepMs = 10;

// Les satiétés qui font écrire l'alerte de faim.
constexpr std::array<int, 3> kHungerWarnings = {1, 5, 10};

}  // namespace

HomunculusIntimacyGrade GradeOfIntimacy(int intimacy) noexcept {
  if (intimacy <= kHateWithPassionMost) {
    return HomunculusIntimacyGrade::kHateWithPassion;
  }
  if (intimacy <= kHateMost) {
    return HomunculusIntimacyGrade::kHate;
  }
  if (intimacy <= kAwkwardMost) {
    return HomunculusIntimacyGrade::kAwkward;
  }
  if (intimacy <= kShyMost) {
    return HomunculusIntimacyGrade::kShy;
  }
  if (intimacy <= kNeutralMost) {
    return HomunculusIntimacyGrade::kNeutral;
  }
  if (intimacy <= kCordialMost) {
    return HomunculusIntimacyGrade::kCordial;
  }
  if (intimacy <= kHomunculusIntimacyMost) {
    return HomunculusIntimacyGrade::kLoyal;
  }
  return HomunculusIntimacyGrade::kUnknown;
}

CompanionCast CompanionCastOf(std::uint16_t targeting, int level) {
  const KnownSkill skill{.targeting = targeting};
  if (SkillIsPassive(skill)) {
    return CompanionCast::kPassive;
  }
  if (level <= 0) {
    return CompanionCast::kNotLearned;
  }
  return SkillCastsOnSelf(skill) ? CompanionCast::kNow : CompanionCast::kNeedsAim;
}

int AspdFromAmotion(int amotion_ms) noexcept {
  return (kAspdAmotionBaseMs - amotion_ms) / kAspdAmotionStepMs;
}

void Homunculus::Acknowledge(std::uint32_t actor_id) noexcept {
  acknowledged_ = true;
  actor_id_ = actor_id;
}

bool Homunculus::TakeSheet(const HomunculusSheet& sheet) {
  sheet_ = sheet;
  sheet_known_ = true;
  if (std::ranges::find(kHungerWarnings, sheet.hunger) == kHungerWarnings.end() ||
      last_hunger_warned_ == sheet.hunger) {
    return false;
  }
  last_hunger_warned_ = sheet.hunger;
  return true;
}

void Homunculus::UpdateSkill(const HomunculusSkill& skill) {
  const auto found = std::ranges::find(skills_, skill.skill_id, &HomunculusSkill::skill_id);
  if (found == skills_.end()) {
    skills_.push_back(skill);
    return;
  }
  // ⚠ LA MISE À JOUR NE PORTE NI LA VISÉE NI LE NOM : ceux de la liste restent.
  found->level = skill.level;
  found->sp_cost = skill.sp_cost;
  found->range_cells = skill.range_cells;
  found->upgradable = skill.upgradable;
}

void Homunculus::Forget() noexcept {
  acknowledged_ = false;
  actor_id_ = 0;
}

bool Homunculus::NoticeOwnSkill(std::uint16_t skill_id, std::uint32_t announced_ms) noexcept {
  constexpr std::uint16_t kRestSkill = 244;
  if (announced_ms == last_noticed_ms_) {
    return false;
  }
  last_noticed_ms_ = announced_ms;
  if (skill_id != kRestSkill || !acknowledged_) {
    return false;
  }
  Forget();
  return true;
}

const HomunculusSkill* Homunculus::FindSkill(std::uint16_t skill_id) const noexcept {
  const auto found = std::ranges::find(skills_, skill_id, &HomunculusSkill::skill_id);
  return found == skills_.end() ? nullptr : &*found;
}

}  // namespace ro::sim
