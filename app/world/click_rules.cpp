#include "world/click_rules.h"

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
