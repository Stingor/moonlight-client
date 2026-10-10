#include "ro/sim/skill_side_effects.h"

namespace ro::sim {

void ApplyAnnouncedSkillToCaster(Actor& caster, const SupportHit& hit, bool caster_is_player,
                                 bool target_known) {
  const std::uint32_t gauge_ms = SkillGaugeDurationMs(hit.skill_id, hit.amount);
  if (gauge_ms != 0 && caster_is_player && hit.succeeded) {
    caster.gauge = SkillGauge{hit.started_ms, gauge_ms};
  }
  if (hit.skill_id == kWinkCharmSkill && target_known) {
    caster.emotion = Emotion{kBaseEmotionPack, kEmotionChup};
    caster.emotion_since_ms = hit.started_ms;
  }
}

void ArmArrivalPose(Actor& attacker, const Strike& strike) {
  if (ArmsArrivalPose(strike.skill_id, strike.type)) {
    attacker.arrival_pose_armed = true;
  }
}

void NoteArrival(Actor& actor, std::uint32_t arrived_ms) {
  if (actor.arrival_pose_armed) {
    actor.arrival_pose_armed = false;
    actor.arrival_pose_since_ms = arrived_ms;
  }
}

}  // namespace ro::sim
