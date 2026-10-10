// LES POSES MENÉES d'un acteur : celles que le client conduit image par image
// par-dessus l'animation du fichier — à l'emploi, au coup, sur une case, à
// l'incantation d'un état qui mène sa pose, sous un effet —, la blessure
// qu'elles écartent, et l'échéance qui finit le geste qu'elles couvrent.
//
// Séparées de `actor_skill_posture.cpp`, qui joue les postures elles-mêmes et
// n'y consulte que l'image d'une incantation qui mène et la fin d'une pose.

#include <cstddef>
#include <cstdint>
#include <optional>

#include "ro/render/actor_effect_table.h"
#include "ro/render/ailment_sprites.h"
#include "ro/render/effect_frame.h"
#include "ro/render/ground_skill_visual.h"
#include "ro/render/led_pose.h"
#include "ro/render/monster_skill_motion.h"
#include "ro/render/skill_held_pose.h"
#include "ro/render/skill_impl.h"
#include "ro/render/skill_use_motion.h"
#include "ro/sim/status_effects.h"

#include "world/actor_view.h"

namespace moonclient {
namespace {

// L'état d'emploi ordinaire : celui dont seize métiers mènent une pose.
constexpr std::int8_t kUseState = 7;

// 🔴 L'INSTANT OÙ UNE POSE MENÉE DÉCLARE FINI LE GESTE qu'elle couvre : son
// échéance, quand elle est minutée et que l'état du geste ramène alors au
// repos — voir `ro::render::StateRestsWhenFinished`. L'acteur y quitte la
// passe en cours : le reste de l'animation ne se joue pas. Une pose sans
// échéance rend la main au geste, qui continue.
std::optional<std::uint32_t> LedPoseFinishesAt(const ro::render::SkillHeldPose& pose,
                                               std::uint32_t posed_ms, std::int8_t action_state) {
  if (!pose.timed || !ro::render::StateRestsWhenFinished(action_state)) {
    return std::nullopt;
  }
  return posed_ms + pose.hold_ms;
}

// Le tirage au sort que le client fait en posant l'état : zéro ou un. Nous le
// lisons sur l'instant de la pose, qui ne change pas d'une image à l'autre.
std::uint8_t DrawAt(std::uint32_t posed_ms) { return static_cast<std::uint8_t>(posed_ms & 1U); }

// L'IMAGE QUE MÈNE L'ÉTAT `action_state` posé à `posed_ms`, ou rien. Une
// créature n'y passe pas : son client ne pose pas d'état qui mène. Un état que
// la table des rangs ne nomme pas pose le rang zéro, comme chez le client.
std::optional<ro::render::StateLedFrame> MotionLedFrameAt(const ro::sim::Actor& actor,
                                                          ro::render::SpriteLayout layout,
                                                          std::int8_t action_state,
                                                          std::uint32_t posed_ms,
                                                          std::uint32_t at_ms) {
  if (layout != ro::render::SpriteLayout::kPlayer || at_ms < posed_ms ||
      !ro::render::StateLeadsPose(action_state)) {
    return std::nullopt;
  }
  const std::size_t rank = ActionAnimationOf(actor, action_state, layout).value_or(0);
  return ro::render::StateLedFrameAt(
      action_state, static_cast<std::uint8_t>(rank), at_ms - posed_ms,
      static_cast<std::uint32_t>(ro::render::kEffectTickMs), DrawAt(posed_ms), ShownJobOf(actor));
}

// Vrai quand la pose que mène l'état est échue sous un état que la fin ramène
// au repos : l'acteur n'en montre plus rien.
bool StateLedPoseFinished(const ro::render::StateLedFrame& led, std::int8_t action_state) {
  return !led.running && ro::render::StateRestsWhenFinished(action_state);
}

// L'état que pose l'emploi de l'acteur, quand il en pose un qui mène.
std::optional<std::int8_t> UseLedState(const ro::sim::Actor& actor) {
  if (!actor.skill_use.has_value() ||
      ro::render::SkillUsePosedByTable(actor.look.job, actor.skill_use->skill_id)) {
    return std::nullopt;
  }
  return ro::render::SkillUseStateFor(actor.skill_use->skill_id);
}

// LA POSE QUE L'EMPLOI MÈNE : celle de la compétence, que l'aiguillage du
// client pose après l'état d'emploi et qui remplace donc celle du métier ;
// sinon celle que l'emploi ordinaire de seize métiers mène — ni pour qui est
// assis ou terré, ni quand la table d'une créature pose le geste.
std::optional<ro::render::SkillHeldPose> PoseLedByUse(const ro::sim::Actor& actor,
                                                      const ro::sim::SkillUse& use) {
  if (const ro::render::SkillHeldPose* pose = ro::render::SkillHeldPoseOf(use.skill_id)) {
    return *pose;
  }
  if (ro::render::SkillUsePosedByTable(actor.look.job, use.skill_id) ||
      ro::render::SkillUseStateFor(use.skill_id) != kUseState ||
      actor.posture == ro::sim::Posture::kSitting ||
      actor.status_effects.contains(ro::sim::kEfstSuHide)) {
    return std::nullopt;
  }
  return ro::render::UseLedPoseOf(actor.look.job);
}

// LA POSE QUE LE COUP MÈNE : celle de l'impact de la compétence, qui remplace
// toute pose du même instant et peut durer le délai de combo du lanceur ;
// sinon celle de l'emploi ordinaire que le coup pose, pour les mêmes seize
// métiers — pas pour qui est assis ou terré.
struct StrikeLedPose {
  ro::render::SkillHeldPose pose;
  bool from_impact = false;
};

std::optional<StrikeLedPose> PoseLedByStrike(const ro::sim::Actor& actor,
                                             ro::render::Motion current,
                                             const ro::sim::Strike& hit) {
  if (const ro::render::SkillHeldPose* pose =
          ro::render::SkillImpactPoseOf(hit.skill_id, hit.type)) {
    ro::render::SkillHeldPose impact = *pose;
    impact.hold_ms = ro::render::ImpactPoseHoldMs(*pose, hit.started_ms, actor.combo_delay_ms,
                                                  actor.combo_delay_since_ms);
    return StrikeLedPose{impact, true};
  }
  if (StrikeState(hit) != kUseState || SkillUseKeepsPosture(actor, current, kUseState)) {
    return std::nullopt;
  }
  if (const std::optional<ro::render::SkillHeldPose> pose =
          ro::render::UseLedPoseOf(actor.look.job)) {
    return StrikeLedPose{*pose, false};
  }
  return std::nullopt;
}

// `running_only` écarte la pose d'incantation échue : son image reste, mais
// elle ne garde plus de la blessure.
std::optional<ro::render::HeldPoseFrame> LedPoseOf(const ro::render::SpriteCatalog* catalog,
                                                   const ro::sim::Actor& actor,
                                                   ro::render::SpriteLayout layout,
                                                   ro::render::Motion current,
                                                   const MotionEvents& events, std::uint32_t at_ms,
                                                   bool running_only) {
  const auto tick_ms = static_cast<std::uint32_t>(ro::render::kEffectTickMs);
  // LA DERNIÈRE POSE POSÉE GAGNE : chacune remplace la précédente, et la
  // motion suivante les termine toutes. Au même instant, c'est l'ORDRE DES
  // APPELS qui tranche : on les examine ici dans cet ordre, et la suivante
  // remplace la précédente.
  std::optional<ro::render::HeldPoseFrame> view;
  std::uint32_t held_start_ms = 0;
  const auto keep = [&](std::uint32_t start_ms,
                        const std::optional<ro::render::HeldPoseFrame>& image,
                        bool counts_effects) {
    if (!image.has_value() || at_ms < start_ms || (view.has_value() && start_ms < held_start_ms) ||
        MotionChangedSince(catalog, actor, layout, current, events,
                           ChangeWindow{start_ms, at_ms, counts_effects})) {
      return;
    }
    view = image;
    held_start_ms = start_ms;
  };
  const auto held = [&](const ro::render::SkillHeldPose& pose, std::uint32_t start_ms) {
    return at_ms >= start_ms ? ro::render::HeldPoseFrameAt(pose, at_ms - start_ms, tick_ms)
                             : std::nullopt;
  };
  // L'IMAGE QUE MÈNE UN ÉTAT posé à `posed_ms` : échue sous un état que la fin
  // repose, elle ne se montre plus ; échue sous un autre, elle reste, mais ne
  // garde plus de la blessure.
  const auto keep_state_led = [&](std::int8_t action_state, std::uint32_t posed_ms) {
    const std::optional<ro::render::StateLedFrame> led =
        MotionLedFrameAt(actor, layout, action_state, posed_ms, at_ms);
    if (led.has_value() && !StateLedPoseFinished(*led, action_state) &&
        (led->running || !running_only)) {
      keep(posed_ms, led->frame, false);
    }
  };
  // 🔴 LA POSE DE L'EMPLOI : celle de la compétence écrase celle du métier, qui
  // ne revient donc pas quand la sienne finit avant — voir `PoseLedByUse`. Un
  // état d'emploi qui mène sa pose la pose en tête du répartiteur, et la pose
  // de la compétence, si elle en a une, la remplace de même.
  if (actor.skill_use.has_value()) {
    const ro::sim::SkillUse& use = *actor.skill_use;
    if (const std::optional<ro::render::SkillHeldPose> pose = PoseLedByUse(actor, use)) {
      keep(use.started_ms, held(*pose, use.started_ms), false);
    } else if (const std::optional<std::int8_t> use_state = UseLedState(actor)) {
      keep_state_led(*use_state, use.started_ms);
    }
  }
  // LA POSE QUE MÈNE LE RÉPARTITEUR D'UNE COMPÉTENCE POSÉE SUR UNE CASE, après
  // la motion d'emploi qu'il a posée en tête : elle la remplace, et son image
  // tenue reste après l'échéance, comme celle d'un effet.
  if (actor.ground_skill.has_value() && at_ms >= actor.ground_skill->started_ms) {
    const ro::sim::GroundSkillUse& use = *actor.ground_skill;
    if (const ro::render::GroundSkillPose* pose =
            ro::render::GroundSkillPoseOf(use.skill_id, use.level)) {
      if (view.has_value() && held_start_ms == use.started_ms) {
        view.reset();
      }
      const ro::render::EffectLedPose led_by_case{.tick = 0,
                                                  .play_rank = pose->play_rank,
                                                  .first_frame = pose->first_frame,
                                                  .last_frame = pose->last_frame,
                                                  .hold_rank = pose->hold_rank,
                                                  .hold_frame = pose->hold_frame,
                                                  .hold_ms = pose->hold_ms};
      const std::uint32_t age_ms = at_ms - use.started_ms;
      keep(use.started_ms, ro::render::EffectLedPoseFrameAt(led_by_case, age_ms, tick_ms),
           age_ms >= led_by_case.hold_ms);
    }
  }
  // LA POSE DU COUP : l'état qu'il pose mène la sienne, et l'emploi ordinaire
  // celle du métier, le client passant par la même motion qu'à l'emploi
  // annoncé ; la pose d'impact de la compétence la remplace, et ne laisse pas
  // revenir celle d'un autre geste du même instant — voir `PoseLedByStrike`.
  if (actor.attack.has_value()) {
    const ro::sim::Strike& hit = *actor.attack;
    if (const std::optional<std::int8_t> strike_state = StrikeState(hit)) {
      keep_state_led(*strike_state, hit.started_ms);
    }
    if (const std::optional<StrikeLedPose> led = PoseLedByStrike(actor, current, hit)) {
      if (led->from_impact && view.has_value() && held_start_ms == hit.started_ms) {
        view.reset();
      }
      keep(hit.started_ms, held(led->pose, hit.started_ms), false);
    }
  }
  // LA POSE D'ARRIVÉE qu'un impact d'Asura a armée, levée par l'arrêt qui suit.
  if (actor.arrival_pose_since_ms.has_value()) {
    keep(*actor.arrival_pose_since_ms,
         held(ro::render::kArrivalHeldPose, *actor.arrival_pose_since_ms), false);
  }
  // 🔴 L'INCANTATION D'UN ÉTAT QUI MÈNE SA POSE : voir `ro::render::StateLeadsPose`.
  // Son image tient au-delà de la barre, jusqu'à la motion suivante.
  if (actor.cast.has_value() && actor.cast->duration_ms != 0) {
    keep_state_led(ro::render::SkillImplFor(actor.cast->skill_id).CastState(),
                   actor.cast->started_ms);
  }
  // 🔴 LA POSE QU'UN EFFET MÈNE, à son tic. Son image tenue reste après son
  // échéance, jusqu'à la motion suivante — et l'état que le même effet écrit
  // plus tard en est une, dès que la pose est échue : le coup de pied sauté
  // tient l'image de son élan du cinquième au trente-cinquième tic, puis se
  // repose en garde.
  for (const ro::render::ActorEffectPlay& play : events.played_effects) {
    const ro::render::ActorEffectChange* change = ro::render::ActorEffectChangeFor(play.effect_id);
    if (change == nullptr || change->led_pose.tick == ro::render::kNoLedPoseTick ||
        play.age_ticks < static_cast<std::uint32_t>(change->led_pose.tick)) {
      continue;
    }
    const std::uint32_t start_ms =
        TickInstantMs(play, static_cast<std::uint32_t>(change->led_pose.tick), events);
    if (at_ms < start_ms) {
      continue;
    }
    const bool expired = at_ms - start_ms >= change->led_pose.hold_ms;
    keep(start_ms, ro::render::EffectLedPoseFrameAt(change->led_pose, at_ms - start_ms, tick_ms),
         expired);
  }
  return view;
}

}  // namespace

std::optional<ro::render::StateLedFrame> CastLedFrameAt(const ro::sim::Actor& actor,
                                                        ro::render::SpriteLayout layout,
                                                        std::int8_t cast_state,
                                                        std::uint32_t at_ms) {
  // La barre sans durée sort avant la pose, chez le client.
  if (actor.cast->duration_ms == 0) {
    return std::nullopt;
  }
  return MotionLedFrameAt(actor, layout, cast_state, actor.cast->started_ms, at_ms);
}

std::optional<ro::render::StateLedFrame> SkillUseLedFrameAt(const ro::sim::Actor& actor,
                                                            ro::render::SpriteLayout layout,
                                                            std::uint32_t at_ms) {
  const std::optional<std::int8_t> use_state = UseLedState(actor);
  if (!use_state.has_value()) {
    return std::nullopt;
  }
  return MotionLedFrameAt(actor, layout, *use_state, actor.skill_use->started_ms, at_ms);
}

bool SkillUseFinishedByLedPose(const ro::sim::Actor& actor, ro::render::SpriteLayout layout,
                               std::int8_t use_state, std::uint32_t now_ms) {
  if (!actor.skill_use.has_value()) {
    return false;
  }
  const ro::sim::SkillUse& use = *actor.skill_use;
  if (const std::optional<ro::render::SkillHeldPose> led = PoseLedByUse(actor, use)) {
    const std::optional<std::uint32_t> finished_ms =
        LedPoseFinishesAt(*led, use.started_ms, use_state);
    return finished_ms.has_value() && now_ms >= *finished_ms;
  }
  const std::optional<ro::render::StateLedFrame> led = SkillUseLedFrameAt(actor, layout, now_ms);
  return led.has_value() && StateLedPoseFinished(*led, use_state);
}

bool StrikeFinishedByLedPose(const ro::sim::Actor& actor, ro::render::SpriteLayout layout,
                             ro::render::Motion current, std::uint32_t now_ms) {
  if (!actor.attack.has_value()) {
    return false;
  }
  const ro::sim::Strike& hit = *actor.attack;
  const std::optional<std::int8_t> state = StrikeState(hit);
  if (!state.has_value()) {
    return false;
  }
  // La pose d'impact remplace celle de l'état : c'est la sienne qui finit.
  if (const std::optional<StrikeLedPose> led = PoseLedByStrike(actor, current, hit)) {
    const std::optional<std::uint32_t> finished_ms =
        LedPoseFinishesAt(led->pose, hit.started_ms, *state);
    return finished_ms.has_value() && now_ms >= *finished_ms;
  }
  const std::optional<ro::render::StateLedFrame> led =
      MotionLedFrameAt(actor, layout, *state, hit.started_ms, now_ms);
  return led.has_value() && StateLedPoseFinished(*led, *state);
}

std::optional<ro::render::HeldPoseFrame> LedPoseAt(const ro::render::SpriteCatalog* catalog,
                                                   const ro::sim::Actor& actor,
                                                   ro::render::SpriteLayout layout,
                                                   ro::render::Motion current,
                                                   const MotionEvents& events,
                                                   std::uint32_t at_ms) {
  return LedPoseOf(catalog, actor, layout, current, events, at_ms, false);
}

std::optional<ro::render::HeldPoseFrame> BladeStopPoseAt(const ro::render::SpriteCatalog* catalog,
                                                         const ro::sim::Actor& actor,
                                                         ro::render::SpriteLayout layout,
                                                         ro::render::Motion current,
                                                         const MotionEvents& events,
                                                         std::uint32_t at_ms) {
  if (!actor.blade_stop_caster) {
    return std::nullopt;
  }
  const bool holding = actor.blade_stop_since_ms.has_value();
  const bool kept = !holding && actor.blade_stop_released_ms.has_value() &&
                    !MotionChangedSince(catalog, actor, layout, current, events,
                                        ChangeWindow{*actor.blade_stop_released_ms, at_ms, true});
  if (!holding && !kept) {
    return std::nullopt;
  }
  return ro::render::HeldPoseFrame{static_cast<std::uint8_t>(ro::render::kBladeStopHeldAnimation),
                                   static_cast<std::uint8_t>(ro::render::kBladeStopHeldFrame)};
}

bool LedPoseHoldsAt(const ro::render::SpriteCatalog* catalog, const ro::sim::Actor& actor,
                    ro::render::SpriteLayout layout, ro::render::Motion current,
                    const MotionEvents& events, std::uint32_t at_ms) {
  // LA PRISE D'UNE LAME est une pose menée sans échéance : la blessure n'a pas
  // lieu tant qu'elle dure.
  if (actor.blade_stop_caster && actor.blade_stop_since_ms.has_value()) {
    return true;
  }
  // La blessure qu'on juge n'est pas encore un changement de motion.
  const MotionEvents without_hurt{std::nullopt, events.played_effects, events.effects_at_ms};
  return LedPoseOf(catalog, actor, layout, current, without_hurt, at_ms, true).has_value();
}

}  // namespace moonclient
