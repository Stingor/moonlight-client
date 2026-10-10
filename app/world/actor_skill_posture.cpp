// LES POSTURES DE COMPÉTENCE d'un acteur : celle de l'incantation, celle de
// l'emploi, le geste qu'une créature prend dans sa table — et le repos qui les
// suit, garde ou attente.
//
// Séparées de `actor_motion.cpp`, où `MotionAt` les consulte
// dans son ordre de priorité et ne garde que cet ordre.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>

#include "ro/render/actor_effect_table.h"
#include "ro/render/effect_frame.h"
#include "ro/render/led_pose.h"
#include "ro/render/monster_skill_motion.h"
#include "ro/render/skill_impl.h"
#include "ro/render/skill_use_motion.h"
#include "ro/render/strike_play.h"
#include "ro/sim/combat_guard.h"
#include "ro/sim/status_effects.h"

#include "world/actor_view.h"

namespace moonclient {
namespace {

// LE PAS DE L'ANIMATION, vingt-quatre millisecondes : une cadence imposée s'y
// compte.
constexpr float kActionStepMs = 24.0F;
// L'état d'emploi ordinaire, le seul que deux gardes écartent.
constexpr std::int8_t kUseState = 7;
// L'ÉTAT ZÉRO N'EST PAS UNE ANIMATION : c'est le RETOUR AU REPOS. Voir
// `RestPostureAt`.
constexpr std::int8_t kRestState = 0;
// Les états que l'acteur quitte en marchant, en frappant, en bronchant et en
// ramassant : ils ne décident que de la garde, au retour au repos.
constexpr std::int8_t kWalkState = 1;
constexpr std::int8_t kAttackState = 2;
constexpr std::int8_t kHurtState = 4;
constexpr std::int8_t kPickupState = 5;
// L'INCANTATION ORDINAIRE : la seule pose qui TIENT sa première image au lieu
// de jouer son geste.
constexpr std::int8_t kCastState = 8;
// L'ÉTAT DU SOMMEIL, que le réveil quitte.
constexpr std::int8_t kSleepState = 10;
// 🔴 L'ÉTAT QUE LE PAS DU MONDE NE FAIT PLUS AVANCER : le client pose son rang,
// image zéro, et n'y touche plus jusqu'à la motion suivante — ni à la fin de la
// barre, ni au bout du geste. Spiral Pierce à l'incantation, Death Bound et
// Millennium Shield à l'emploi.
constexpr std::int8_t kFrozenState = 11;
// LA GARDE D'AUTO COUNTER — voir `ro::sim::CounterStanceMs`.
constexpr std::int8_t kCounterStanceState = 13;

// LE GESTE QU'UNE TABLE DONNE, joué depuis `since_ms` : une fois, ou sans fin
// quand il tourne — c'est à l'appelant de l'arrêter au changement de motion.
// `cadence_ms` impose la durée d'une image ; zéro garde celle du fichier.
std::optional<PlayedMotion> TableMotion(const ro::render::MonsterSkillMotion& gesture,
                                        ro::render::ActorOutfit& outfit, float since_ms,
                                        float cadence_ms) {
  const float native_ms = outfit.FrameIntervalMs(gesture.animation);
  const float one_pass_ms = outfit.AnimationDurationMs(gesture.animation);
  if (!(native_ms > 0.0F) || !(one_pass_ms > 0.0F)) {
    return std::nullopt;
  }
  const float scale = cadence_ms > 0.0F ? cadence_ms / native_ms : 1.0F;
  const float until_ms =
      gesture.loops ? std::numeric_limits<float>::infinity() : one_pass_ms * scale;
  if (since_ms < 0.0F || since_ms >= until_ms) {
    return std::nullopt;
  }
  // L'IMAGE SE TIENT ICI, et non par l'horloge commune des boucles : la
  // cadence imposée et le départ du geste ne se lisent qu'à partir de lui.
  const std::size_t images = std::max<std::size_t>(outfit.FrameCount(gesture.animation), 1);
  const auto rang = static_cast<std::size_t>(std::floor(since_ms / (native_ms * scale)));
  PlayedMotion played;
  played.motion = ro::render::Motion::kCast;
  played.animation = gesture.animation;
  played.elapsed_ms = since_ms / scale;
  played.held_frame = gesture.loops ? rang % images : std::min(rang, images - 1);
  return played;
}

}  // namespace

bool SkillUseKeepsPosture(const ro::sim::Actor& actor, ro::render::Motion current,
                          std::int8_t use_state) {
  // L'EMPLOI ORDINAIRE NE DÉRANGE NI QUI EST ASSIS, NI QUI S'EST TERRÉ : le
  // client rend la main avant de toucher l'action, et l'acteur garde tout — son
  // geste, sa cadence, son image du coup.
  return use_state == kUseState && (current == ro::render::Motion::kSit ||
                                    actor.status_effects.contains(ro::sim::kEfstSuHide));
}

namespace {

// Vrai quand cet état change ce que l'acteur montre : il lui pose un rang, lui
// mène une pose — même sur un rang que la table ne nomme pas —, ou fige une
// créature là où elle en est.
bool StateChangesMotion(const ro::sim::Actor& actor, ro::render::SpriteLayout layout,
                        std::int8_t action_state) {
  if (layout == ro::render::SpriteLayout::kPlayer) {
    return ActionAnimationOf(actor, action_state, layout).has_value() ||
           ro::render::StateLeadsPose(action_state);
  }
  return ActionAnimationOf(actor, action_state, layout).has_value() ||
         ro::render::CreatureStateFreezes(action_state);
}

// Vrai quand cet emploi change la motion : un geste de table, ou un état
// d'emploi qui pose un rang et que rien ne retient.
bool SkillUseChangesMotion(const ro::render::SpriteCatalog* catalog, const ro::sim::Actor& actor,
                           ro::render::SpriteLayout layout, ro::render::Motion current,
                           std::uint16_t skill_id) {
  if (ro::render::SkillUsePosedByTable(actor.look.job, skill_id)) {
    return catalog != nullptr &&
           ro::render::MonsterSkillMotionFor(*catalog, actor.look.job, skill_id).has_value();
  }
  const std::int8_t status = ro::render::SkillUseStateFor(skill_id);
  return !SkillUseKeepsPosture(actor, current, status) && StateChangesMotion(actor, layout, status);
}

// Vrai quand cette incantation change la motion : elle dure, et son état
// pose un rang ou fige une créature — ou la table d'une créature lui donne un
// geste.
bool CastChangesMotion(const ro::render::SpriteCatalog* catalog, const ro::sim::Actor& actor,
                       ro::render::SpriteLayout layout, const ro::sim::Cast& cast) {
  if (cast.duration_ms == 0) {
    return false;
  }
  if (StateChangesMotion(actor, layout, ro::render::SkillImplFor(cast.skill_id).CastState())) {
    return true;
  }
  return catalog != nullptr && ro::render::IsMonsterOrNpcClass(actor.look.job) &&
         ro::render::MonsterSkillMotionFor(*catalog, actor.look.job, cast.skill_id).has_value();
}

bool StrikeChangesMotion(const ro::render::SpriteCatalog* catalog, const ro::sim::Actor& actor,
                         ro::render::SpriteLayout layout, ro::render::Motion current,
                         const ro::sim::Strike& strike);

}  // namespace

std::uint32_t TickInstantMs(const ro::render::ActorEffectPlay& play, std::uint32_t tick,
                            const MotionEvents& events) {
  const auto tick_ms = static_cast<std::uint32_t>(ro::render::kEffectTickMs);
  return events.effects_at_ms - (play.age_ticks - tick) * tick_ms;
}

// 🔴 UN GESTE DE TABLE QUI TOURNE TIENT JUSQU'AU CHANGEMENT DE MOTION SUIVANT,
// quel qu'il soit : c'est la prochaine action posée qui l'arrête, et rien
// d'autre — ni une durée, ni la fin de l'incantation. Une pose menée de même :
// l'action suivante la remet à zéro.
bool MotionChangedSince(const ro::render::SpriteCatalog* catalog, const ro::sim::Actor& actor,
                        ro::render::SpriteLayout layout, ro::render::Motion current,
                        const MotionEvents& events, const ChangeWindow& window) {
  const auto after = [&window](std::uint32_t instant_ms) {
    return instant_ms > window.since_ms && instant_ms <= window.until_ms;
  };
  if (current == ro::render::Motion::kDie || after(actor.posture_since_ms) ||
      (actor.movement.has_value() && after(actor.movement_started_ms)) ||
      (actor.stopped_ms.has_value() && after(*actor.stopped_ms)) ||
      (actor.awoke_ms.has_value() && after(*actor.awoke_ms)) ||
      (SleepFreezesMotion(actor, layout) && after(*actor.asleep_since_ms)) ||
      (actor.attack.has_value() && after(actor.attack->started_ms) &&
       StrikeChangesMotion(catalog, actor, layout, current, *actor.attack)) ||
      (events.hurt_at_ms.has_value() && after(*events.hurt_at_ms)) ||
      (actor.pickup_since_ms.has_value() && after(*actor.pickup_since_ms))) {
    return true;
  }
  if (actor.cast.has_value() && after(actor.cast->started_ms) &&
      CastChangesMotion(catalog, actor, layout, *actor.cast)) {
    return true;
  }
  if (actor.skill_use.has_value() && after(actor.skill_use->started_ms) &&
      SkillUseChangesMotion(catalog, actor, layout, current, actor.skill_use->skill_id)) {
    return true;
  }
  if (const auto mort = actor.status_since_ms.find(ro::sim::kEfstTrickDead);
      mort != actor.status_since_ms.end() && after(mort->second)) {
    return true;
  }
  if (!window.counts_effects) {
    return false;
  }
  // UN EFFET QUI ÉCRIT L'ÉTAT le change à son tic, pas à sa naissance.
  return std::ranges::any_of(events.played_effects, [&](const ro::render::ActorEffectPlay& play) {
    const std::optional<ro::render::EffectMotionWrite> write = ro::render::LastMotionWrite(play);
    return write.has_value() && after(TickInstantMs(play, write->tick, events));
  });
}

std::optional<std::int8_t> StrikeState(const ro::sim::Strike& strike) {
  if (strike.skill_id == 0) {
    return kAttackState;
  }
  return ro::render::SkillHitMotionState(strike.skill_id, strike.type);
}

namespace {

// Vrai quand ce coup CHANGE la motion de son auteur : un coup ordinaire
// toujours ; un coup de compétence quand son état pose une action — chez un
// joueur, tout état sauf l'emploi de qui est assis ou terré ; chez une
// créature, l'attaque, l'emploi d'un homoncule ou d'un élémentaire, et le
// geste que sa table nomme. Un coup qui laisse la motion ne termine ni un
// geste de table qui tourne, ni une pose menée.
bool StrikeChangesMotion(const ro::render::SpriteCatalog* catalog, const ro::sim::Actor& actor,
                         ro::render::SpriteLayout layout, ro::render::Motion current,
                         const ro::sim::Strike& strike) {
  const std::optional<std::int8_t> state = StrikeState(strike);
  if (!state.has_value()) {
    return false;
  }
  if (layout == ro::render::SpriteLayout::kCreature) {
    if (*state == kAttackState) {
      return true;
    }
    if (*state == kUseState && (actor.familiar == ro::sim::Familiar::kHomunculus ||
                                actor.familiar == ro::sim::Familiar::kElemental)) {
      return true;
    }
    return catalog != nullptr && ro::render::IsMonsterOrNpcClass(actor.look.job) &&
           ro::render::MonsterSkillMotionFor(*catalog, actor.look.job, strike.skill_id).has_value();
  }
  return *state != kUseState || !SkillUseKeepsPosture(actor, current, kUseState);
}

// L'ÉTAT QU'UN EMPLOI fait prendre, ou rien quand il laisse la posture : un
// geste de table n'en pose aucun, l'emploi ordinaire de qui est assis ou terré
// non plus.
std::optional<std::int8_t> SkillUseState(const ro::sim::Actor& actor,
                                         const ro::sim::SkillUse& use) {
  if (ro::sim::CounterStanceMs(use).has_value()) {
    return kCounterStanceState;
  }
  if (ro::render::SkillUsePosedByTable(actor.look.job, use.skill_id)) {
    return std::nullopt;
  }
  const std::int8_t use_state = ro::render::SkillUseStateFor(use.skill_id);
  const ro::render::Motion resting = actor.posture == ro::sim::Posture::kSitting
                                         ? ro::render::Motion::kSit
                                         : ro::render::Motion::kIdle;
  if (SkillUseKeepsPosture(actor, resting, use_state)) {
    return std::nullopt;
  }
  return use_state;
}

// L'ÉTAT QUE L'ACTEUR QUITTE à `at_ms` : celui du dernier changement de motion
// connu STRICTEMENT avant cet instant, ou le repos quand aucun ne précède. À
// instant égal, le geste examiné en dernier l'emporte — l'ordre des paquets.
std::int8_t StateLeftAt(const ro::sim::Actor& actor, std::uint32_t at_ms,
                        const MotionEvents& events) {
  std::int8_t left_state = kRestState;
  std::uint32_t latest_ms = 0;
  const auto consider = [&](std::uint32_t since_ms, std::int8_t state) {
    if (since_ms < at_ms && since_ms >= latest_ms) {
      latest_ms = since_ms;
      left_state = state;
    }
  };
  consider(actor.posture_since_ms, kRestState);
  if (actor.movement.has_value()) {
    consider(actor.movement_started_ms, kWalkState);
  }
  // L'arrêt qui fait glisser quitte la marche ; le réveil quitte le sommeil.
  if (actor.stopped_ms.has_value() && actor.stop_slid) {
    consider(*actor.stopped_ms, kWalkState);
  }
  if (actor.awoke_ms.has_value()) {
    consider(*actor.awoke_ms, kSleepState);
  }
  if (actor.pickup_since_ms.has_value()) {
    consider(*actor.pickup_since_ms, kPickupState);
  }
  if (actor.cast.has_value() && actor.cast->duration_ms != 0) {
    consider(actor.cast->started_ms, ro::render::SkillImplFor(actor.cast->skill_id).CastState());
  }
  if (actor.hit.has_value() && actor.hit->flinches) {
    consider(actor.hit->started_ms, kHurtState);
  }
  // Un état que le client inscrit autrement, ou qui change en finissant, compte
  // pour l'état qu'il laisse : voir `ro::render::StateLeftByFinishedMotion`.
  if (actor.attack.has_value()) {
    if (const std::optional<std::int8_t> state = StrikeState(*actor.attack)) {
      consider(actor.attack->started_ms, ro::render::StateLeftByFinishedMotion(*state));
    }
  }
  if (actor.skill_use.has_value()) {
    if (const std::optional<std::int8_t> state = SkillUseState(actor, *actor.skill_use)) {
      consider(actor.skill_use->started_ms, ro::render::StateLeftByFinishedMotion(*state));
    }
  }
  // L'ÉTAT QU'UN EFFET ÉCRIT compte comme un geste : celui qu'il avait écrit en
  // dernier à cet instant-là.
  const auto tick_ms = static_cast<std::uint32_t>(ro::render::kEffectTickMs);
  const std::uint32_t ticks_since =
      events.effects_at_ms > at_ms ? (events.effects_at_ms - at_ms) / tick_ms : 0;
  for (const ro::render::ActorEffectPlay& play : events.played_effects) {
    if (play.age_ticks < ticks_since) {
      continue;
    }
    const ro::render::ActorEffectPlay then{play.effect_id, play.age_ticks - ticks_since};
    if (const std::optional<ro::render::EffectMotionWrite> write =
            ro::render::LastMotionWrite(then)) {
      consider(TickInstantMs(play, write->tick, events), write->state);
    }
  }
  return left_state;
}

// 🔴 LE RETOUR AU REPOS, que l'état zéro demande à l'incantation comme à
// l'emploi. Le client n'y joue AUCUNE animation : il pose l'attente, dont un
// joueur TIENT l'image du regard, et c'est tout. Jouer le rang zéro comme un
// geste fait défiler ses trois images — trois orientations de la tête — et le
// personnage regarde à gauche puis à droite pendant toute la durée du rang.
//
// ⚠ LA GARDE DÉPEND DE L'ÉTAT QUITTÉ, et du mode PK : voir
// `ro::sim::GuardsLeaving`. Sortant d'une attaque, d'une blessure ou d'une
// incantation ordinaire, l'acteur prend la garde ; sortant de n'importe quoi
// d'autre — l'incantation d'état sept d'Agi Up, le ramassage de Pick Stone, la
// garde elle-même —, il revient à l'attente.
//
// Le repos tient jusqu'au changement de motion suivant. Une marche que le
// chemin porte encore n'est pas interrompue : c'est le serveur qui l'arrête.
std::optional<PlayedMotion> RestPostureAt(const ro::sim::Actor& actor,
                                          ro::render::SpriteLayout layout,
                                          ro::render::Motion current, bool changed_since,
                                          std::uint32_t started_ms, const MotionEvents& events) {
  if (changed_since || current == ro::render::Motion::kWalk) {
    return std::nullopt;
  }
  const ro::render::Motion rest =
      ro::sim::GuardsLeaving(actor, StateLeftAt(actor, started_ms, events))
          ? ro::render::Motion::kReady
          : ro::render::Motion::kIdle;
  return PlayedMotion{rest, ro::render::AnimationFor(rest, layout), 0.0F, std::nullopt, true};
}

// 🔴 LA GARDE D'AUTO COUNTER : la première image de l'attaque que l'arme seule
// décide, tenue le temps de la barre, puis le repos — quitté depuis la garde,
// donc l'attente. La motion posée depuis la termine.
std::optional<PlayedMotion> CounterStanceAt(const ro::render::SpriteCatalog* catalog,
                                            const ro::sim::Actor& actor,
                                            ro::render::SpriteLayout layout, bool changed_since,
                                            std::uint32_t hold_ms, float since_ms) {
  if (changed_since || !(since_ms < static_cast<float>(hold_ms))) {
    return std::nullopt;
  }
  const std::size_t animation = catalog != nullptr
                                    ? ro::render::AttackVariantAnimation(*catalog, LookOf(actor))
                                    : ro::render::AnimationFor(ro::render::Motion::kAttack, layout);
  return PlayedMotion{ro::render::Motion::kAttack, animation, since_ms, std::size_t{0}, false};
}

// L'ÉTAT FIGÉ : le rang, image zéro, que plus rien n'avance — pas même sa
// direction, celle de l'instant `started_ms`.
PlayedMotion FrozenPosture(std::size_t animation, float since_ms, std::uint32_t started_ms) {
  PlayedMotion frozen{ro::render::Motion::kCast, animation, since_ms, std::size_t{0}, false};
  frozen.frozen = true;
  frozen.frozen_since_ms = started_ms;
  return frozen;
}

// 🔴 CE QU'UN ÉTAT FAIT D'UNE CRÉATURE — voir `ro::render::CreatureMotionFamily`.
// Le rang qu'il lui pose, joué une fois depuis sa première image ; figée là où
// elle en est, action, image et direction, par un état que le pas du monde
// n'avance pas ; et rien du tout pour les autres états, qui la laissent
// continuer ce qu'elle jouait. L'attente que l'état lui pose, son repos la
// joue déjà.
std::optional<PlayedMotion> CreatureStatePostureAt(const ro::sim::Actor& actor,
                                                   ro::render::ActorOutfit& outfit,
                                                   ro::render::SpriteLayout layout,
                                                   ro::render::Motion current,
                                                   std::int8_t action_state, bool changed,
                                                   std::uint32_t started_ms, float since_ms) {
  if (changed) {
    return std::nullopt;
  }
  if (ro::render::CreatureStateFreezes(action_state)) {
    PlayedMotion frozen{ro::render::Motion::kCast, ro::render::AnimationFor(current, layout),
                        since_ms, std::size_t{0}, false};
    frozen.frozen = true;
    frozen.frozen_since_ms = started_ms;
    frozen.keeps_shown_image = true;
    return frozen;
  }
  const std::optional<std::size_t> animation = ActionAnimationOf(actor, action_state, layout);
  if (!animation.has_value() || *animation == ro::render::kAnimationIdle ||
      !(since_ms < outfit.AnimationDurationMs(*animation))) {
    return std::nullopt;
  }
  return PlayedMotion{ro::render::Motion::kCast, *animation, since_ms, std::nullopt, false};
}

}  // namespace

ro::render::Motion RestMotionAt(const ro::sim::Actor& actor, const MotionEvents& events,
                                std::uint32_t now_ms) {
  return ro::sim::GuardsLeaving(actor, StateLeftAt(actor, now_ms, events))
             ? ro::render::Motion::kReady
             : ro::render::Motion::kIdle;
}

std::optional<PlayedMotion> CastPostureAt(const ro::render::SpriteCatalog* catalog,
                                          const ro::sim::Actor& actor,
                                          ro::render::ActorOutfit& outfit,
                                          ro::render::SpriteLayout layout,
                                          ro::render::Motion current, const MotionEvents& events,
                                          std::uint32_t now_ms) {
  // QUI INCANTE le montre. Le serveur n'annonce PAS la fin d'une incantation
  // qui aboutit — seulement celle qu'on interrompt. C'est donc la durée
  // annoncée qui la borne, sans quoi le personnage incanterait jusqu'à son
  // prochain geste.
  //
  // La POSE dépend de la compétence : le client la résout par une table de
  // huit cents cas, où deux cent quatre-vingt-dix-huit compétences prennent
  // autre chose que l'incantation et soixante-quatre ne changent RIEN.
  const ro::sim::Cast& cast = *actor.cast;
  const auto since_ms = static_cast<float>(now_ms - cast.started_ms);
  // 🔴 TOUTE MOTION POSÉE DEPUIS L'ANNONCE LA TERMINE : un coup encaissé, un
  // emploi arrivé avant la fin de la barre, une marche. Le client remplace la
  // pose et ne la reprend pas ; la barre, elle, continue.
  const bool changed = MotionChangedSince(catalog, actor, layout, current, events,
                                          ChangeWindow{cast.started_ms, now_ms, true});
  // 🔴 UNE CRÉATURE PREND ENSUITE LE GESTE DE SA TABLE, par-dessus la posture
  // d'incantation, quand la table en nomme un : une fois, ou jusqu'au
  // changement de motion suivant s'il tourne. La métamorphose y règle en plus
  // sa cadence.
  if (catalog != nullptr && ro::render::IsMonsterOrNpcClass(actor.look.job)) {
    if (const std::optional<ro::render::MonsterSkillMotion> gesture =
            ro::render::MonsterSkillMotionFor(*catalog, actor.look.job, cast.skill_id)) {
      if (changed) {
        return std::nullopt;
      }
      const std::optional<float> step =
          ro::render::MonsterCastCadenceSteps(cast.skill_id, cast.duration_ms);
      return TableMotion(*gesture, outfit, since_ms, step.value_or(0.0F) * kActionStepMs);
    }
  }
  const std::int8_t cast_state = ro::render::SkillImplFor(cast.skill_id).CastState();
  if (cast.duration_ms != 0 && cast_state == kRestState) {
    return RestPostureAt(actor, layout, current, changed, cast.started_ms, events);
  }
  if (layout != ro::render::SpriteLayout::kPlayer) {
    return cast.duration_ms == 0
               ? std::nullopt
               : CreatureStatePostureAt(actor, outfit, layout, current, cast_state, changed,
                                        cast.started_ms, since_ms);
  }
  const std::optional<std::size_t> animation =
      PosedAnimationOf(catalog, actor, cast_state, layout, cast.started_ms);
  if (changed || cast.duration_ms == 0 || !animation.has_value()) {
    return std::nullopt;
  }
  // L'ÉTAT FIGÉ survit à la barre : rien ne l'en sort que la motion suivante.
  if (cast_state == kFrozenState) {
    return FrozenPosture(*animation, since_ms, cast.started_ms);
  }
  // 🔴 L'ÉTAT QUI MÈNE SA POSE ne déroule pas son rang : il en montre les
  // images que le client choisit, et la dernière survit à la barre — voir
  // `ro::render::StateLeadsPose`. Le Sniper tient ainsi, immobile, l'image zéro
  // de son tir pendant tout Sharpshooting.
  if (const std::optional<ro::render::StateLedFrame> led =
          CastLedFrameAt(actor, layout, cast_state, now_ms)) {
    return PlayedMotion{ro::render::Motion::kCast, led->frame.rank, since_ms,
                        std::size_t{led->frame.frame}, false};
  }
  // 🔴 L'INCANTATION ORDINAIRE TIENT SA PREMIÈRE IMAGE, et ne la fait pas
  // défiler : le client pose le rang et fige l'image d'emblée. Elle tient
  // jusqu'à la fin de la barre — où le client repasse au repos, en garde — ou
  // jusqu'à la motion suivante.
  if (cast_state == kCastState) {
    if (since_ms < static_cast<float>(cast.duration_ms)) {
      return PlayedMotion{ro::render::Motion::kCast, *animation, since_ms, std::size_t{0}, false};
    }
    return std::nullopt;
  }
  // TOUT AUTRE ÉTAT JOUE SON GESTE UNE FOIS, depuis sa première image, puis
  // l'acteur revient au repos que cet état décide — sans attendre la fin de la
  // barre. Le ramassage de Pick Stone se baisse ainsi une fois, puis se
  // relève, pendant que sa barre court encore.
  if (since_ms < outfit.AnimationDurationMs(*animation)) {
    return PlayedMotion{ro::render::Motion::kCast, *animation, since_ms, std::nullopt, false};
  }
  return std::nullopt;
}

void SpreadCastOverBar(const ro::sim::Actor& actor, ro::render::ActorOutfit& outfit,
                       ro::render::SpriteLayout layout, std::uint32_t now_ms,
                       PlayedMotion& played) {
  if (layout != ro::render::SpriteLayout::kPlayer || !actor.cast.has_value() ||
      actor.cast->duration_ms == 0 || played.motion != ro::render::Motion::kCast ||
      played.held_frame != std::optional<std::size_t>{0} ||
      ro::render::SkillImplFor(actor.cast->skill_id).CastState() != kCastState) {
    return;
  }
  const auto since_ms = static_cast<float>(now_ms - actor.cast->started_ms);
  const auto bar_ms = static_cast<float>(actor.cast->duration_ms);
  if (since_ms >= bar_ms) {
    return;
  }
  played.held_frame.reset();
  played.elapsed_ms = since_ms * outfit.AnimationDurationMs(played.animation) / bar_ms;
  played.loops = false;
}

std::optional<PlayedMotion> SkillUsePostureAt(const ro::render::SpriteCatalog* catalog,
                                              const ro::sim::Actor& actor,
                                              ro::render::ActorOutfit& outfit,
                                              ro::render::SpriteLayout layout,
                                              ro::render::Motion current,
                                              const MotionEvents& events, std::uint32_t now_ms) {
  // 🔴 QUI EMPLOIE UNE COMPÉTENCE LA MONTRE, barre ou pas. C'est un SECOND
  // résolveur du client — celui de l'emploi —, et il est le seul à donner sa
  // posture à une compétence INSTANTANÉE.
  //
  // ⚠ LA DURÉE EST CELLE DE L'ANIMATION : le serveur n'en donne aucune.
  //
  // 🔴 LA POSTURE SE LIT SUR `skill_use`, QUE LE LANCEUR PORTE — et non sur
  // `support`, que porte la cible : voir `ro::sim::SkillUse`.
  const ro::sim::SkillUse& use = *actor.skill_use;
  const auto since_ms = static_cast<float>(now_ms - use.started_ms);
  const bool changed = MotionChangedSince(catalog, actor, layout, current, events,
                                          ChangeWindow{use.started_ms, now_ms, true});
  const std::optional<std::uint32_t> stance_ms = ro::sim::CounterStanceMs(use);
  // 🔴 UNE CRÉATURE N'A PAS D'ÉTAT D'EMPLOI : son geste vient de sa table, et
  // une compétence que la table ne nomme pas la laisse telle qu'elle est — sauf
  // la garde d'Auto Counter, que le client pose à tout lanceur.
  if (ro::render::SkillUsePosedByTable(actor.look.job, use.skill_id)) {
    const std::optional<ro::render::MonsterSkillMotion> gesture =
        catalog != nullptr
            ? ro::render::MonsterSkillMotionFor(*catalog, actor.look.job, use.skill_id)
            : std::nullopt;
    if (!gesture.has_value() && stance_ms.has_value()) {
      return CounterStanceAt(catalog, actor, layout, changed, *stance_ms, since_ms);
    }
    if (!gesture.has_value() || changed) {
      return std::nullopt;
    }
    return TableMotion(*gesture, outfit, since_ms, 0.0F);
  }
  if (stance_ms.has_value()) {
    return CounterStanceAt(catalog, actor, layout, changed, *stance_ms, since_ms);
  }
  const std::int8_t status = ro::render::SkillUseStateFor(use.skill_id);
  if (SkillUseKeepsPosture(actor, current, status)) {
    return std::nullopt;
  }
  // Agi Up et Pick Stone, entre autres, ne jouent rien : ils ramènent au repos.
  if (status == kRestState) {
    return RestPostureAt(actor, layout, current, changed, use.started_ms, events);
  }
  if (layout != ro::render::SpriteLayout::kPlayer) {
    return CreatureStatePostureAt(actor, outfit, layout, current, status, changed, use.started_ms,
                                  since_ms);
  }
  if (changed) {
    return std::nullopt;
  }
  // 🔴 LA POSE MENÉE MINUTÉE FINIT LE GESTE À SON ÉCHÉANCE, sous un état que la
  // fin ramène au repos : voir `SkillUseFinishedByLedPose`.
  if (SkillUseFinishedByLedPose(actor, layout, status, now_ms)) {
    return std::nullopt;
  }
  // 🔴 L'ÉTAT QUI MÈNE SA POSE ne déroule pas son rang : il en montre les
  // images que le client choisit, et la dernière reste — voir
  // `ro::render::StateLeadsPose`. Le taekwon qui prend sa garde tient l'image
  // 3 de son rang d'incantation ; le coup de pied descendant joue les images 3,
  // 5, 7, 8 et 2 de l'attaque.
  if (const std::optional<ro::render::StateLedFrame> led =
          SkillUseLedFrameAt(actor, layout, now_ms)) {
    return PlayedMotion{ro::render::Motion::kCast, led->frame.rank, since_ms,
                        std::size_t{led->frame.frame}, false};
  }
  const std::optional<std::size_t> animation =
      PosedAnimationOf(catalog, actor, status, layout, use.started_ms);
  if (!animation.has_value()) {
    return std::nullopt;
  }
  if (status == kFrozenState) {
    return FrozenPosture(*animation, since_ms, use.started_ms);
  }
  // Une passe, depuis la première image, puis le repos que l'état décide : voir
  // `CastPostureAt`. La motion posée depuis termine le geste.
  if (since_ms < outfit.AnimationDurationMs(*animation)) {
    return PlayedMotion{ro::render::Motion::kCast, *animation, since_ms, std::nullopt, false};
  }
  return std::nullopt;
}

}  // namespace moonclient
