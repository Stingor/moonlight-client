#include "world/click_rules.h"

#include <algorithm>

#include "world/targeting.h"

namespace moonclient {

ro::render::CursorShape ShapeUnderShift(ro::render::CursorShape shape,
                                        const ro::sim::Actor* hovered, bool shift_held,
                                        bool no_shift) {
  if (shape != ro::render::CursorShape::kAttack || hovered == nullptr ||
      hovered->kind != ro::sim::ActorKind::kMonster ||
      !MonsterClickWithheld(shift_held, no_shift, /*alt_held=*/false)) {
    return shape;
  }
  return ro::render::CursorShape::kArrow;
}

bool TakesAttackOrder(const ro::sim::Actor& self) {
  return self.posture != ro::sim::Posture::kSitting && (self.option & ro::sim::kOptionHide) == 0;
}

bool RidingWithholdsAttack(const ro::sim::Actor& self) {
  return self.status_effects.contains(ro::sim::kStatusAllRiding);
}

bool TakesSkillOrder(const ro::sim::Actor& self, std::uint16_t skill_id) {
  if (self.posture == ro::sim::Posture::kSitting) {
    return false;
  }
  if ((self.option & ro::sim::kOptionHide) == 0) {
    return true;
  }
  return skill_id == kGrimtoothSkillId || skill_id == kBackStabSkillId ||
         skill_id == kShadowLeapSkillId || skill_id == kShadowSlashSkillId ||
         skill_id == kDarkSlashSkillId;
}

WeightVerdict WeightGuardOf(std::int64_t weight, std::int64_t max_weight, std::uint16_t skill_id) {
  if (skill_id == kWeightUnguardedSkillId) {
    return WeightVerdict::kPasses;
  }
  constexpr std::int64_t kPercent = 100;
  const std::int64_t carried_percent = kPercent * weight / std::max<std::int64_t>(max_weight, 1);
  if (carried_percent < kOverweightActionPercent) {
    return WeightVerdict::kPasses;
  }
  return skill_id == kWeightWarnedSkillId ? WeightVerdict::kWarnsAndPasses
                                          : WeightVerdict::kRefuses;
}

bool ClickContactsNpc(const ro::sim::Actor& npc) {
  return npc.kind == ro::sim::ActorKind::kNonPlayer && npc.familiar == ro::sim::Familiar::kNone &&
         npc.look.job != ro::sim::kPortalJob;
}

int BasicAttackAction(bool repeat, const PlayerAction& action, std::uint32_t target_id) {
  if (repeat) {
    return kAttackActionRepeat;
  }
  const bool held_on_it = action.deed == PlayerDeed::kAttack && action.target_id == target_id &&
                          action.stops_on_release && !action.released;
  return held_on_it ? kAttackActionRepeat : kAttackActionOnce;
}

bool DoubleClickClock::NotePress(std::uint32_t now_ms) noexcept {
  // ⚠ STRICTEMENT MOINS que le délai : à l'échéance pile, l'appui est neuf.
  const bool doubled = armed_ && now_ms - pressed_ms_ < kDoubleClickMs;
  // Le second appui désarme : le suivant repart à neuf.
  armed_ = !doubled;
  pressed_ms_ = now_ms;
  return doubled;
}

}  // namespace moonclient
