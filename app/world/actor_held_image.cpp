// L'IMAGE QU'UNE MOTION TIENT au lieu de la dérouler : l'attente d'un joueur,
// l'incantation ordinaire, la garde d'Auto Counter, le sommeil d'une créature —
// et ce qu'y devient l'image qu'une compétence de case impose, bornée à ce que
// la motion porte, ou dessinée telle quelle sur une motion figée.
//
// Séparée de `actor_skill_posture.cpp`, qui décide des postures : ici, on ne
// décide que de l'image qu'elles montrent.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "ro/render/actor_effect_table.h"
#include "ro/render/ground_skill_visual.h"
#include "ro/render/skill_impl.h"
#include "ro/sim/combat_notice.h"

#include "world/actor_view.h"

namespace moonclient {
namespace {

// L'incantation ordinaire, la seule qui fige sa première image.
constexpr std::int8_t kCastState = 8;

// 🔴 CINQ COMPTES DONT L'ATTENTE SE DÉROULE : le client, configuré pour le
// service coréen comme l'est l'installation, ne leur fait pas tenir l'image du
// regard. Leurs trois images défilent comme le geste d'une créature.
constexpr std::array<std::uint32_t, 5> kAccountsWhoseIdleUnrolls = {2919549, 2919550, 2919551,
                                                                    2919552, 2919554};

// Vrai tant que la garde d'Auto Counter tient `played` : sa première image,
// avant la fin de sa barre.
bool CounterStanceHolds(const ro::sim::Actor& actor, const PlayedMotion& played,
                        std::uint32_t at_ms) {
  if (!actor.skill_use.has_value() || played.motion != ro::render::Motion::kAttack ||
      played.held_frame != std::size_t{0} || played.loops) {
    return false;
  }
  const std::optional<std::uint32_t> stance_ms = ro::sim::CounterStanceMs(*actor.skill_use);
  return stance_ms.has_value() && at_ms >= actor.skill_use->started_ms &&
         at_ms - actor.skill_use->started_ms < *stance_ms;
}

// Vrai quand le client TIENT l'image de cette motion au lieu de la dérouler :
// l'attente et l'assise d'un joueur, qui montrent l'image du regard,
// l'incantation ordinaire, qui fige la sienne tant que dure la barre, la garde
// d'Auto Counter, et le sommeil d'une créature. La garde, un geste joué une
// fois, l'attente d'une créature éveillée déroulent la leur.
bool HoldsImage(const ro::sim::Actor& actor, ro::render::SpriteLayout layout,
                const PlayedMotion& played, std::uint32_t at_ms) {
  if (CounterStanceHolds(actor, played, at_ms)) {
    return true;
  }
  if (layout != ro::render::SpriteLayout::kPlayer) {
    // Le mercenaire humain tient son image zéro au repos : voir
    // `ro::render::IsHumanMercenaryClass`.
    if (played.motion == ro::render::Motion::kIdle && actor.kind != ro::sim::ActorKind::kItem &&
        ro::render::IsHumanMercenaryClass(actor.look.job)) {
      return true;
    }
    return played.motion == ro::render::Motion::kIdle && SleepFreezesMotion(actor, layout) &&
           at_ms >= *actor.asleep_since_ms;
  }
  if (played.motion == ro::render::Motion::kIdle) {
    return actor.kind == ro::sim::ActorKind::kPlayer && IdleHoldsLookImage(actor);
  }
  if (played.motion == ro::render::Motion::kSit) {
    return actor.kind == ro::sim::ActorKind::kPlayer;
  }
  if (played.motion != ro::render::Motion::kCast || played.held_frame != std::size_t{0} ||
      !actor.cast.has_value()) {
    return false;
  }
  const ro::sim::Cast& cast = *actor.cast;
  return cast.duration_ms != 0 && at_ms >= cast.started_ms &&
         at_ms - cast.started_ms < cast.duration_ms &&
         ro::render::SkillImplFor(cast.skill_id).CastState() == kCastState &&
         ActionAnimationOf(actor, kCastState, layout) == played.animation;
}

// LES EFFETS QU'IL PORTAIT à `since_ms`, et l'âge qu'ils y avaient.
std::vector<ro::render::ActorEffectPlay> EffectsThen(const MotionEvents& events,
                                                     std::uint32_t now_ms, std::uint32_t since_ms) {
  const auto tick_ms = static_cast<std::uint32_t>(ro::render::kEffectTickMs);
  const std::uint32_t ticks_since = (now_ms - since_ms) / tick_ms;
  std::vector<ro::render::ActorEffectPlay> then;
  for (const ro::render::ActorEffectPlay& play : events.played_effects) {
    if (play.age_ticks >= ticks_since) {
      then.push_back({play.effect_id, play.age_ticks - ticks_since});
    }
  }
  return then;
}

// L'INSTANT OÙ LA COMPÉTENCE DE CASE QUI LAISSE LA MOTION est arrivée, quand
// rien n'a changé la motion depuis ; rien sinon.
std::optional<std::uint32_t> KeptMotionGroundSkillSince(const ro::render::SpriteCatalog* catalog,
                                                        const ro::sim::Actor& actor,
                                                        ro::render::SpriteLayout layout,
                                                        const PlayedMotion& played,
                                                        const MotionEvents& events,
                                                        std::uint32_t now_ms) {
  if (!actor.ground_skill.has_value() ||
      !ro::sim::GroundSkillKeepsCasterMotion(actor.ground_skill->skill_id)) {
    return std::nullopt;
  }
  const std::uint32_t since_ms = actor.ground_skill->started_ms;
  if (now_ms < since_ms || MotionChangedSince(catalog, actor, layout, played.motion, events,
                                              ChangeWindow{since_ms, now_ms, true})) {
    return std::nullopt;
  }
  return since_ms;
}

}  // namespace

bool IdleHoldsLookImage(const ro::sim::Actor& actor) {
  return ComposedAsPlayer(actor) &&
         std::ranges::find(kAccountsWhoseIdleUnrolls, actor.id) == kAccountsWhoseIdleUnrolls.end();
}

std::optional<std::size_t> VignetteHeldFrame(const ro::sim::Actor& actor, std::size_t animation) {
  if ((animation == ro::render::kAnimationIdle && IdleHoldsLookImage(actor)) ||
      (animation == ro::render::kAnimationSit && ComposedAsPlayer(actor))) {
    return kLookAheadImage;
  }
  return std::nullopt;
}

bool SleepFreezesMotion(const ro::sim::Actor& actor, ro::render::SpriteLayout layout) {
  return layout == ro::render::SpriteLayout::kCreature && actor.asleep_since_ms.has_value() &&
         CreatureFamilyOf(actor) != ro::render::CreatureMotionFamily::kNpc;
}

std::optional<PlayedMotion> AsleepCreatureMotionAt(const ro::render::SpriteCatalog* catalog,
                                                   const ro::sim::Actor& actor,
                                                   ro::render::SpriteLayout layout,
                                                   const MotionEvents& events,
                                                   std::uint32_t now_ms) {
  if (!SleepFreezesMotion(actor, layout)) {
    return std::nullopt;
  }
  const std::uint32_t since_ms = *actor.asleep_since_ms;
  if (now_ms < since_ms || MotionChangedSince(catalog, actor, layout, ro::render::Motion::kIdle,
                                              events, ChangeWindow{since_ms, now_ms, true})) {
    return std::nullopt;
  }
  return PlayedMotion{ro::render::Motion::kIdle,
                      ro::render::AnimationFor(ro::render::Motion::kIdle, layout),
                      static_cast<float>(now_ms - since_ms), std::size_t{0}, false};
}

std::optional<std::size_t> GroundSkillHeldFrameAt(
    const ro::render::SpriteCatalog* catalog, const ro::sim::Actor& actor,
    ro::render::ActorOutfit& outfit, ro::render::SpriteLayout layout, const PlayedMotion& played,
    const MotionEvents& events, std::uint32_t now_ms, const MotionAtInstant& motion_at) {
  if (!HoldsImage(actor, layout, played, now_ms)) {
    return std::nullopt;
  }
  const std::optional<std::uint32_t> since_ms =
      KeptMotionGroundSkillSince(catalog, actor, layout, played, events, now_ms);
  if (!since_ms.has_value()) {
    return std::nullopt;
  }
  // LE REGARD REÇU DEPUIS réécrit l'image de l'attente et de l'assise — pas
  // celle d'une motion qui la fige, qu'il laisse.
  if ((played.motion == ro::render::Motion::kIdle || played.motion == ro::render::Motion::kSit) &&
      layout == ro::render::SpriteLayout::kPlayer &&
      actor.turns_received != actor.turns_received_at_ground_skill) {
    return std::nullopt;
  }
  // CE QU'IL JOUAIT QUAND ELLE EST ARRIVÉE : un geste qui s'est achevé depuis
  // l'a ramené au repos par une motion neuve, qui a effacé l'image.
  const PlayedMotion before = motion_at(*since_ms, EffectsThen(events, now_ms, *since_ms));
  const std::size_t own_frames = outfit.FrameCount(played.animation);
  if (before.motion != played.motion || before.animation != played.animation ||
      !HoldsImage(actor, layout, before, *since_ms) || own_frames == 0) {
    return std::nullopt;
  }
  const std::size_t imposed =
      outfit.BodyActionFrameCount(ro::render::kGroundSkillHeldImageAnimation,
                                  ro::render::kGroundSkillHeldImageDirection) -
      1;
  return std::min(imposed, own_frames - 1);
}

std::optional<PlayedMotion> GroundSkillFrozenImageAt(
    const ro::render::SpriteCatalog* catalog, const ro::sim::Actor& actor,
    ro::render::ActorOutfit& outfit, ro::render::SpriteLayout layout, const PlayedMotion& played,
    const MotionEvents& events, std::uint32_t now_ms, const MotionAtInstant& motion_at) {
  if (!played.frozen) {
    return std::nullopt;
  }
  const std::optional<std::uint32_t> since_ms =
      KeptMotionGroundSkillSince(catalog, actor, layout, played, events, now_ms);
  if (!since_ms.has_value()) {
    return std::nullopt;
  }
  const PlayedMotion before = motion_at(*since_ms, EffectsThen(events, now_ms, *since_ms));
  if (!before.frozen || before.motion != played.motion || before.animation != played.animation) {
    return std::nullopt;
  }
  // L'ACTION À PART SE DESSINE ELLE-MÊME, à sa dernière image : rien ne la borne.
  PlayedMotion drawn = played;
  drawn.animation = ro::render::kGroundSkillHeldImageAnimation;
  drawn.held_direction = ro::render::kGroundSkillHeldImageDirection;
  drawn.keeps_shown_image = false;
  drawn.held_frame = outfit.BodyActionFrameCount(ro::render::kGroundSkillHeldImageAnimation,
                                                 ro::render::kGroundSkillHeldImageDirection) -
                     1;
  return drawn;
}

}  // namespace moonclient
