#include "ro/sim/motion_events.h"

#include "ro/sim/movement.h"

namespace ro::sim {

void NoteStop(Actor& actor, CellPosition where, std::uint32_t stopped_ms) {
  const CellPosition standing = StandingCell(actor);
  actor.stop_slid = IsWalking(actor, stopped_ms) || standing.x != where.x || standing.y != where.y;
  actor.stopped_ms = stopped_ms;
}

void NoteSleep(Actor& actor, bool asleep, std::uint32_t now_ms) {
  if (asleep) {
    if (!actor.asleep_since_ms.has_value()) {
      actor.asleep_since_ms = now_ms;
    }
    return;
  }
  if (actor.asleep_since_ms.has_value()) {
    actor.asleep_since_ms.reset();
    actor.awoke_ms = now_ms;
  }
}

void NoteBodyState(Actor& actor, std::uint16_t body_state, std::uint32_t now_ms) {
  if (actor.body_state == body_state) {
    return;
  }
  if (actor.body_state == kBodyAsleep || body_state == kBodyAsleep) {
    NoteSleep(actor, body_state == kBodyAsleep, now_ms);
  }
  actor.body_state = body_state;
  actor.body_state_since_ms = now_ms;
}

void NoteStatusSleep(Actor& actor, StatusEffectId effect, bool active, std::uint32_t now_ms) {
  if (effect == kEfstDeepSleep) {
    NoteSleep(actor, active, now_ms);
  }
}

namespace {

// Note l'instant où le bit commence à tenir, et l'oublie quand il cesse.
void NoteHeldBit(std::uint16_t health_state, std::uint16_t bit, std::uint32_t now_ms,
                 std::optional<std::uint32_t>& since_ms) {
  if ((health_state & bit) == 0) {
    since_ms.reset();
    return;
  }
  if (!since_ms.has_value()) {
    since_ms = now_ms;
  }
}

}  // namespace

void NoteHealthMarks(Actor& actor, std::uint32_t now_ms) {
  NoteHeldBit(actor.health_state, kHealthSilenced, now_ms, actor.silenced_since_ms);
  NoteHeldBit(actor.health_state, kHealthCursed, now_ms, actor.cursed_since_ms);
}

void KeepStateEdges(Actor& announced, const Actor& known) {
  announced.option_risen = known.option_risen;
  announced.option_rise_count = known.option_rise_count;
  announced.option_fallen = known.option_fallen;
  announced.option_fall_count = known.option_fall_count;
  announced.state_change_count = known.state_change_count;
  if ((announced.health_state & kHealthSilenced) != 0) {
    announced.silenced_since_ms = known.silenced_since_ms;
  }
  if ((announced.health_state & kHealthCursed) != 0) {
    announced.cursed_since_ms = known.cursed_since_ms;
  }
  announced.awoke_ms = known.awoke_ms;
  announced.blade_stop_since_ms = known.blade_stop_since_ms;
  announced.blade_stop_caster = known.blade_stop_caster;
  announced.blade_stop_released_ms = known.blade_stop_released_ms;
  if (announced.body_state == known.body_state) {
    announced.body_state_since_ms = known.body_state_since_ms;
    announced.asleep_since_ms = known.asleep_since_ms;
  }
}

std::optional<std::uint32_t> BodyStateHeldSince(const Actor& actor, std::uint16_t body_state) {
  if (actor.body_state != body_state) {
    return std::nullopt;
  }
  return actor.body_state_since_ms;
}

}  // namespace ro::sim
