#include <cstdint>
#include <optional>

#include "ro/render/actor_effect_table.h"
#include "ro/render/skill_held_pose.h"

namespace ro::render {
namespace {

// LE CHAMP QUI PORTE L'ÉTAT DE L'ACTEUR, et non son rang d'animation.
//
// ⚠ LA DIFFÉRENCE COÛTE UNE LECTURE. `kActorStateField` est un ÉTAT, que le
// client traduit en rang : l'état
// vingt-sept donne le rang douze — l'incantation —, l'état deux le rang dix —
// l'attaque. Prendre l'état pour un rang ferait jouer l'image deux à un effet
// qui demande la douzième.
inline constexpr std::uint16_t kActorStateField = 0x70;

// LA PÉRIODE D'UNE POSE QU'UN EFFET MÈNE SANS EN POSER : l'acteur garde celle
// de sa dernière action, et la table des poses de compétence donne la plus
// ordinaire, dix avances par image. Les deux poses du lot n'ont qu'une image :
// elle ne compte que pour la forme.
inline constexpr std::uint8_t kInheritedPeriodTicks = 10;

}  // namespace

std::optional<EffectMotionWrite> LastMotionWrite(const ActorEffectPlay& played) {
  const ActorEffectChange* change = ActorEffectChangeFor(played.effect_id);
  if (change == nullptr || played.age_ticks < change->first_tick) {
    return std::nullopt;
  }
  // ⚠ CELLE QUI A DÉJÀ EU LIEU, ET LA PLUS TARDIVE. Un effet en écrit deux —
  // l'état vingt-sept au cinquième tic, l'état deux au trente-cinquième — et
  // prendre la première laisserait le coup de pied sur son élan.
  std::optional<EffectMotionWrite> last;
  for (const ActorFieldWrite& write : change->writes) {
    if (write.field != kActorStateField || write.at_tick == kWriteEveryTick) {
      continue;
    }
    const auto tick = static_cast<std::uint32_t>(write.at_tick);
    if (played.age_ticks >= tick && (!last.has_value() || tick > last->tick)) {
      last = EffectMotionWrite{tick, static_cast<std::int8_t>(write.value)};
    }
  }
  return last;
}

HeldPoseFrame EffectLedPoseFrameAt(const EffectLedPose& pose, std::uint32_t elapsed_ms,
                                   std::uint32_t tick_ms) {
  SkillHeldPose led;
  led.play_rank = pose.play_rank;
  led.first_frame = pose.first_frame;
  led.last_frame = pose.last_frame;
  led.hold_rank = pose.hold_rank;
  led.hold_frame = pose.hold_frame;
  led.timed = true;
  led.hold_ms = pose.hold_ms;
  led.period_ticks = kInheritedPeriodTicks;
  led.period_inherited = true;
  if (const std::optional<HeldPoseFrame> frame = HeldPoseFrameAt(led, elapsed_ms, tick_ms)) {
    return *frame;
  }
  // Passé l'échéance, l'image tenue reste : le client cesse de la faire avancer
  // et ne la remplace qu'à la motion suivante.
  return HeldPoseFrame{pose.hold_rank, pose.hold_frame};
}

}  // namespace ro::render
