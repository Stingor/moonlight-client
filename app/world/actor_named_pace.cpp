// L'ALLURE D'UN ACTEUR, telle que les semis nommés la lisent : ce que dure une
// image de l'action qu'il vient de prendre, et l'image où son coup porte.
//
// Séparée de `actor_view.cpp`, qui compose la scène : ceci ne sert qu'aux
// délais que des méthodes nommées tirent de leur source.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "ro/render/monster_skill_motion.h"
#include "ro/render/skill_impl.h"
#include "ro/render/skill_use_motion.h"
#include "ro/render/sprite_catalog.h"
#include "ro/render/strike_play.h"
#include "ro/sim/status_effects.h"

#include "world/actor_view.h"

namespace moonclient {
namespace {

// Le client tire un dixième au sort à chaque coup de taekwon ; l'instant en
// tient lieu, comme pour le geste lui-même.
constexpr std::uint32_t kChanceTenthMs = 100;
constexpr std::uint32_t kTenths = 10;
// LE PAS DE L'ANIMATION, vingt-quatre millisecondes : la cadence d'une image
// s'y compte.
constexpr float kActionStepMs = 24.0F;
// La cadence ne descend pas sous un pas par image.
constexpr float kShortestCadenceSteps = 1.0F;
// LES ÉTATS D'EMPLOI, tels que le résolveur les rend : l'attaque et sa
// variante, et l'emploi ordinaire.
constexpr std::int8_t kAttackState = 2;
constexpr std::int8_t kAttackVariantState = 9;
constexpr std::int8_t kUseState = 7;
// L'état qui laisse la posture telle quelle.
constexpr std::int8_t kSkillUseStateNone = -1;
// L'IMAGE DU COUP QUE CHAQUE ÉTAT POSE : l'emploi ordinaire la fixe à six ;
// toute autre action la remet à moins un, ce qui fait partir aussitôt un délai
// qui en dépend.
constexpr float kUseStateHitFrame = ro::render::kUseStateHitFrame;
constexpr float kUnsetHitFrame = ro::render::kResetHitFrame;

// La cadence d'une action en pas, grandie d'un facteur.
float CadenceSteps(ro::render::ActorOutfit& outfit, std::size_t animation, float factor) {
  return std::max(outfit.FrameIntervalMs(animation) / kActionStepMs, kShortestCadenceSteps) *
         factor;
}

// L'image du coup d'une attaque : celle de la tenue — la table d'un joueur, le
// marqueur du coup d'une créature —, reculée du projectile d'un joueur quand
// l'état est l'attaque ordinaire : sa flèche, et son âme.
float AttackHitFrame(ro::render::ActorWardrobe& wardrobe, const ro::sim::Actor& actor,
                     const ro::render::ActorLook& look, ro::render::ActorOutfit& outfit,
                     std::size_t animation, bool with_arrow, float cadence_steps) {
  float frame = ro::render::StrikeFrameOf(look, outfit, animation);
  if (with_arrow && cadence_steps > 0.0F) {
    const bool shoots_souls = actor.status_effects.contains(ro::sim::kEfstSoulAttack);
    frame += ro::render::ArrowDelayMs(wardrobe.catalog(), look, shoots_souls) / kActionStepMs /
             cadence_steps;
  }
  return frame;
}

// 🔴 L'ALLURE DE L'ACTION QUE L'ACTEUR TIENT DÉJÀ : quand l'emploi ne change
// pas sa posture, le client ne touche ni à la cadence ni à l'image du coup, et
// ce sont celles de l'action d'avant que la méthode nommée relit. Elle se
// retrouve ici en demandant ce qu'il joue — l'emploi présent n'y change rien,
// puisqu'il ne pose rien.
ro::render::NamedEffectPaceOf CurrentPace(ro::render::ActorWardrobe& wardrobe,
                                          const ro::sim::World& world, const ro::sim::Actor& actor,
                                          ro::render::ActorOutfit& outfit,
                                          const ro::sim::WalkPath& path, std::uint32_t at_ms) {
  std::vector<ro::render::ActorEffectPlay> effects;
  PlayedEffectsOf(actor, at_ms, effects);
  const PlayedMotion played = MotionAt(&wardrobe, world, actor, outfit, path, at_ms, 0.0F, effects);
  const ro::render::ActorLook look = LookOf(actor);
  ro::render::NamedEffectPaceOf allure;
  allure.hit_frame = kUnsetHitFrame;
  // LE COUP QUE L'ACTEUR PORTE, s'il en joue encore le rang : sa cadence et
  // son image de frappe sont celles que le coup a laissées — l'attaque, ou le
  // rang de l'état d'emploi de la compétence et son image six.
  const ro::sim::Strike* posing = PosingStrikeOf(world, actor);
  if (posing != nullptr && (played.motion == ro::render::Motion::kAttack ||
                            played.motion == ro::render::Motion::kCast)) {
    if (const std::optional<ro::render::StrikePlay> strike =
            StrikePlayFor(wardrobe, world, actor, *posing);
        strike.has_value() && strike->plays_motion && strike->animation == played.animation) {
      allure.cadence_steps = CadenceSteps(outfit, strike->animation, strike->factor);
      allure.hit_frame = strike->hit_frame;
      return allure;
    }
  }
  float facteur = 1.0F;
  if (played.motion == ro::render::Motion::kAttack && actor.attack.has_value()) {
    facteur =
        ro::render::AttackMotionFactor(wardrobe.catalog(), look, actor.attack->attack_motion_ms);
  }
  allure.cadence_steps = CadenceSteps(outfit, played.animation, facteur);
  // LA BLESSURE NE PASSE PAS PAR LE FACTEUR : elle retouche la cadence après
  // coup, voir `HurtCadenceSteps`.
  if (played.motion == ro::render::Motion::kHurt && actor.hit.has_value()) {
    allure.cadence_steps =
        ro::render::HurtCadenceSteps(allure.cadence_steps, actor.hit->hurt_motion_ms,
                                     actor.hit->type, ro::render::IsPlayerClass(actor.look.job));
  }
  if (played.motion == ro::render::Motion::kAttack) {
    allure.hit_frame =
        AttackHitFrame(wardrobe, actor, look, outfit, played.animation, true, allure.cadence_steps);
  } else if (played.motion == ro::render::Motion::kCast && !actor.cast.has_value() &&
             actor.skill_use.has_value() &&
             !ro::render::SkillUsePosedByTable(actor.look.job, actor.skill_use->skill_id) &&
             ro::render::SkillUseStateFor(actor.skill_use->skill_id) == kUseState) {
    allure.hit_frame = kUseStateHitFrame;
  }
  return allure;
}

}  // namespace

std::optional<ro::render::NamedEffectPaceOf> NamedPaceOf(
    ro::render::ActorWardrobe& wardrobe, const ro::sim::World& world, const ro::sim::Actor& actor,
    const ro::sim::WalkPath& path, std::uint16_t skill_id,
    std::optional<std::uint32_t> attack_motion_ms, std::uint32_t at_ms) {
  const ro::render::ActorLook look = LookOf(actor);
  ro::render::ActorOutfit* outfit = wardrobe.OutfitFor(look);
  if (outfit == nullptr) {
    return std::nullopt;
  }
  // LE FACTEUR DE CADENCE : celui du coup que le paquet annonce, et un sans
  // coup — personne ne le pose avant l'emploi d'une compétence.
  const float facteur =
      attack_motion_ms.has_value()
          ? ro::render::AttackMotionFactor(wardrobe.catalog(), look, *attack_motion_ms)
          : 1.0F;
  // 🔴 UNE CRÉATURE PREND SON GESTE DANS SA TABLE : l'action qu'elle nomme, à
  // la cadence de son fichier, et l'image du coup remise à moins un. Sans
  // entrée, rien ne change — ni geste ni allure.
  if (ro::render::SkillUsePosedByTable(actor.look.job, skill_id)) {
    const std::optional<ro::render::MonsterSkillMotion> gesture =
        ro::render::MonsterSkillMotionFor(wardrobe.catalog(), actor.look.job, skill_id);
    if (!gesture.has_value()) {
      return CurrentPace(wardrobe, world, actor, *outfit, path, at_ms);
    }
    ro::render::NamedEffectPaceOf allure;
    allure.cadence_steps = CadenceSteps(*outfit, gesture->animation, facteur);
    allure.hit_frame = kUnsetHitFrame;
    return allure;
  }
  const std::int8_t state = ro::render::SkillUseStateFor(skill_id);
  // UN ÉTAT QUI NE CHANGE PAS LA POSTURE laisse l'allure de l'action d'avant —
  // l'état moins un, un mort, qui fait le mort, et l'emploi ordinaire de qui
  // est assis ou terré.
  const PlayedMotion before = [&] {
    std::vector<ro::render::ActorEffectPlay> effects;
    PlayedEffectsOf(actor, at_ms, effects);
    return MotionAt(&wardrobe, world, actor, *outfit, path, at_ms, 0.0F, effects);
  }();
  if (state == kSkillUseStateNone || before.motion == ro::render::Motion::kDie ||
      actor.status_effects.contains(ro::sim::kEfstTrickDead) ||
      SkillUseKeepsPosture(actor, before.motion, state)) {
    return CurrentPace(wardrobe, world, actor, *outfit, path, at_ms);
  }
  const bool attaque = state == kAttackState || state == kAttackVariantState;
  std::size_t animation = 0;
  if (attaque) {
    // L'attaque tire au sort le rang d'un taekwon ; sa variante ne tire pas.
    const auto dixieme = static_cast<std::uint8_t>((at_ms / kChanceTenthMs) % kTenths);
    animation = state == kAttackState
                    ? ro::render::AttackAnimation(wardrobe.catalog(), look, dixieme)
                    : ro::render::AttackVariantAnimation(wardrobe.catalog(), look);
  } else if (const std::optional<std::size_t> rang =
                 ro::render::ActionAnimationFor(state, LayoutOf(actor), ShownJobOf(actor))) {
    animation = *rang;
  } else {
    // Un état que la table ne nomme pas ne pose rien : l'allure reste celle
    // de l'action d'avant.
    return CurrentPace(wardrobe, world, actor, *outfit, path, at_ms);
  }
  ro::render::NamedEffectPaceOf allure;
  allure.cadence_steps = CadenceSteps(*outfit, animation, facteur);
  if (attaque) {
    allure.hit_frame = AttackHitFrame(wardrobe, actor, look, *outfit, animation,
                                      state == kAttackState, allure.cadence_steps);
  } else {
    allure.hit_frame = state == kUseState ? kUseStateHitFrame : kUnsetHitFrame;
  }
  return allure;
}

}  // namespace moonclient
