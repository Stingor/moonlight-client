#include "ro/render/strike_play.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "ro/render/monster_skill_motion.h"
#include "ro/render/skill_impl.h"
#include "ro/render/skill_use_motion.h"
#include "ro/render/sprite_frame.h"

namespace ro::render {
namespace {

// Le pas d'une durée d'image imposée, en millisecondes.
constexpr float kImpactStepMs = 24.0F;

// Une passe du rang, à la cadence du coup.
void PlayOnePass(StrikePlay& play, const ActorOutfit& outfit, std::size_t animation) {
  play.plays_motion = true;
  play.animation = animation;
  play.duration_ms = outfit.AnimationDurationMs(animation) * play.factor;
}

// Les états de dix et plus qui posent une action, et le dernier des états que
// le client pose lui-même par sa motion.
constexpr std::array<std::int8_t, 9> kActionPosingStatesFromTen = {11, 12, 13, 45, 52,
                                                                   53, 54, 58, 59};
constexpr std::int8_t kLastMotionPosedState = 9;

}  // namespace

bool StatePosesAction(std::int8_t action_state) noexcept {
  if (action_state >= 0 && action_state <= kLastMotionPosedState) {
    return true;
  }
  return std::ranges::find(kActionPosingStatesFromTen, action_state) !=
         kActionPosingStatesFromTen.end();
}

float StrikeFrameOf(const ActorLook& look, const ActorOutfit& outfit,
                    std::size_t attack_animation) {
  return look.player ? DamageFrame(look, attack_animation)
                     : static_cast<float>(outfit.AttackSoundFrame(attack_animation));
}

void FloorSkillGesture(StrikePlay& play, float floor_ms) noexcept {
  if (play.attack || play.duration_ms <= 0.0F || play.duration_ms >= floor_ms) {
    return;
  }
  play.factor *= floor_ms / play.duration_ms;
  play.duration_ms = floor_ms;
}

StrikePlay StrikePlayOf(const SpriteCatalog& catalog, const ActorOutfit& outfit,
                        const ActorLook& look, const StrikeFacts& facts) {
  StrikePlay play;
  if (SplashHitKeepsAction(facts.skill_id, facts.damage_type)) {
    // L'image de l'action en cours, à la durée d'image qu'elle a — sauf pour
    // deux compétences, dont l'impact réécrit cette durée AVANT qu'on la lise.
    play.hit_frame = facts.ongoing_hit_frame;
    float interval_ms = facts.ongoing_frame_interval_ms;
    if (const std::optional<float> steps =
            ImpactFramePeriodSteps(facts.skill_id, facts.damage_type, facts.attack_motion_ms)) {
      interval_ms = *steps * kImpactStepMs;
    }
    // Une image remise rend un instant négatif, que le client sature à zéro.
    play.lands_at_ms = std::max(0.0F, play.hit_frame * interval_ms);
    return play;
  }
  const bool ordinary = facts.skill_id == 0;
  play.state = ordinary ? std::optional<std::int8_t>{kStrikeAttackState}
                        : SkillHitMotionState(facts.skill_id, facts.damage_type);
  // LA CADENCE DU COUP : la durée que le serveur lui donne, sauf pour les
  // compétences qui ne la règlent pas et gardent celle du fichier.
  play.factor = ordinary || SkillHitSetsMotionFactor(facts.skill_id)
                    ? AttackMotionFactor(catalog, look, facts.attack_motion_ms)
                    : 1.0F;
  // LE RATTRAPAGE PAR ESPÈCE d'une créature, ajouté au coup et non au geste :
  // celui de l'attaque ordinaire, ou celui du coup d'une compétence. Le
  // projectile d'un joueur recule son image de frappe et passe ci-dessous.
  const float species_delay_ms = ordinary ? (look.player ? 0.0F : ProjectileDelayMs(catalog, look))
                                          : SkillHitProjectileDelayMs(look);
  // L'IMAGE DE FRAPPE DE L'ATTAQUE et l'instant du coup qu'elle donne : reculée
  // du projectile d'un joueur quand l'attaque le tire.
  const auto attack_timing = [&](std::size_t animation, bool with_projectile) {
    const float interval_ms = outfit.FrameIntervalMs(animation) * play.factor;
    play.hit_frame = StrikeFrameOf(look, outfit, animation);
    if (with_projectile && interval_ms > 0.0F) {
      play.hit_frame += ArrowDelayMs(catalog, look, facts.shoots_souls) / interval_ms;
    }
    play.lands_at_ms = play.hit_frame * interval_ms + species_delay_ms;
  };

  if (!play.state.has_value()) {
    // LE COUP LAISSE LA MOTION, et l'image de frappe d'avant avec elle : celle
    // de l'attaque qui déclenche un enchaînement.
    attack_timing(AttackAnimation(catalog, look, facts.chance_tenth), true);
  } else if (!look.player) {
    // UNE CRÉATURE N'A QUE L'ATTAQUE : tout autre état la laisse telle qu'elle
    // est, et le coup touche aussitôt — sauf l'homoncule et l'élémentaire, dont
    // l'emploi joue le rang cinq.
    if (*play.state == kStrikeAttackState) {
      const std::size_t animation = AttackAnimation(catalog, look, facts.chance_tenth);
      attack_timing(animation, false);
      play.attack = true;
      PlayOnePass(play, outfit, animation);
    } else {
      play.lands_at_ms = species_delay_ms;
      if (*play.state == kStrikeUseState && facts.skill_led_familiar) {
        PlayOnePass(play, outfit, kHomunculusAnimationSkill);
      }
    }
  } else if (*play.state == kStrikeAttackState || *play.state == kStrikeAttackVariantState) {
    // ⚠ LA VARIANTE NE TIRE PAS : le client y compare la valeur brute de l'arme
    // à deux classes, et ce serveur envoie des identifiants d'objet, qui ne les
    // égalent jamais.
    const bool plain_attack = *play.state == kStrikeAttackState;
    const std::size_t animation = plain_attack ? AttackAnimation(catalog, look, facts.chance_tenth)
                                               : AttackVariantAnimation(catalog, look);
    attack_timing(animation, plain_attack);
    play.attack = true;
    PlayOnePass(play, outfit, animation);
  } else if (*play.state == kStrikeUseState) {
    // L'EMPLOI ORDINAIRE : le rang que l'état donne au métier, et l'image six.
    // Assis ou terré, il ne touche à rien, et l'image de frappe reste remise.
    if (!facts.keeps_posture) {
      if (const std::optional<std::size_t> animation =
              ActionAnimationFor(kStrikeUseState, SpriteLayout::kPlayer, look.job)) {
        play.hit_frame = kUseStateHitFrame;
        play.lands_at_ms = kUseStateHitFrame * outfit.FrameIntervalMs(*animation) * play.factor;
        PlayOnePass(play, outfit, *animation);
      }
    }
  } else if (*play.state != kStrikeRestState) {
    // LES AUTRES ÉTATS jouent une passe du rang que l'état donne — ceux qui
    // mènent une pose la montrent par-dessus. Ceux qui posent une action
    // remettent l'image de frappe ; les autres laissent l'image d'avant, celle
    // de l'attaque. Voir `StatePosesAction`.
    if (!StatePosesAction(*play.state)) {
      attack_timing(AttackAnimation(catalog, look, facts.chance_tenth), true);
    }
    if (const std::optional<std::size_t> animation =
            ActionAnimationFor(*play.state, SpriteLayout::kPlayer, look.job)) {
      PlayOnePass(play, outfit, *animation);
    }
  }

  // L'IMPACT RÉÉCRIT LA CADENCE de certaines compétences, après coup : l'instant
  // où le coup porte reste celui de la cadence ordinaire.
  if (const std::optional<float> steps =
          ImpactFramePeriodSteps(facts.skill_id, facts.damage_type, facts.attack_motion_ms);
      steps.has_value() && play.plays_motion) {
    const float native_interval_ms = outfit.FrameIntervalMs(play.animation);
    if (native_interval_ms > 0.0F) {
      play.factor = *steps * kImpactStepMs / native_interval_ms;
      play.duration_ms = outfit.AnimationDurationMs(play.animation) * play.factor;
    }
  }
  // UNE CRÉATURE QUI FRAPPE PAR UNE COMPÉTENCE prend le geste de sa table, s'il
  // y en a un : il remplace le rang à l'écran, sans rien changer à l'instant où
  // le coup porte, déjà réglé.
  if (!ordinary && IsMonsterOrNpcClass(look.job)) {
    if (const std::optional<MonsterSkillMotion> gesture =
            MonsterSkillMotionFor(catalog, look.job, facts.skill_id)) {
      PlayOnePass(play, outfit, gesture->animation);
    }
  }
  return play;
}

StrikePlay UsePlayOf(const SpriteCatalog& catalog, const ActorOutfit& outfit, const ActorLook& look,
                     std::int8_t action_state, std::uint8_t chance_tenth, bool shoots_souls) {
  StrikePlay play;
  play.state = action_state;
  if (!look.player) {
    if (action_state == kStrikeAttackState) {
      const std::size_t animation = AttackAnimation(catalog, look, chance_tenth);
      play.hit_frame = StrikeFrameOf(look, outfit, animation);
      play.attack = true;
      PlayOnePass(play, outfit, animation);
    }
    return play;
  }
  if (action_state == kStrikeAttackState || action_state == kStrikeAttackVariantState) {
    const bool plain_attack = action_state == kStrikeAttackState;
    const std::size_t animation = plain_attack ? AttackAnimation(catalog, look, chance_tenth)
                                               : AttackVariantAnimation(catalog, look);
    play.hit_frame = StrikeFrameOf(look, outfit, animation);
    const float interval_ms = outfit.FrameIntervalMs(animation);
    if (plain_attack && interval_ms > 0.0F) {
      play.hit_frame += ArrowDelayMs(catalog, look, shoots_souls) / interval_ms;
    }
    play.attack = true;
    PlayOnePass(play, outfit, animation);
    return play;
  }
  if (const std::optional<std::size_t> animation =
          ActionAnimationFor(action_state, SpriteLayout::kPlayer, look.job)) {
    if (action_state == kStrikeUseState) {
      play.hit_frame = kUseStateHitFrame;
    }
    PlayOnePass(play, outfit, *animation);
  }
  return play;
}

std::optional<std::size_t> PosedActionAnimation(const SpriteCatalog& catalog, const ActorLook& look,
                                                std::int8_t action_state,
                                                std::uint8_t chance_tenth) {
  if (!look.player) {
    return std::nullopt;
  }
  if (action_state == kStrikeAttackState) {
    return AttackAnimation(catalog, look, chance_tenth);
  }
  if (action_state == kStrikeAttackVariantState) {
    return AttackVariantAnimation(catalog, look);
  }
  return ActionAnimationFor(action_state, SpriteLayout::kPlayer, look.job);
}

}  // namespace ro::render
