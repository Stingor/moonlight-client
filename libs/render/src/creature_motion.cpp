#include "ro/render/creature_motion.h"

#include "ro/render/sprite_frame.h"

namespace ro::render {
namespace {

// Les états auxquels le client pose un rang.
constexpr std::int8_t kIdleState = 0;
constexpr std::int8_t kWalkState = 1;
constexpr std::int8_t kAttackState = 2;
constexpr std::int8_t kDieState = 3;
constexpr std::int8_t kHurtState = 4;
constexpr std::int8_t kUseState = 7;
constexpr std::int8_t kSleepState = 10;

// Les états que le pas du monde n'avance pas ; au-delà du dernier qu'il
// connaisse, aucun ne s'avance non plus.
constexpr std::int8_t kSitState = 6;
constexpr std::int8_t kFrozenState = 11;
constexpr std::int8_t kFirstUnsteppedState = 14;
constexpr std::int8_t kSecondUnsteppedState = 15;
constexpr std::int8_t kThirdUnsteppedState = 29;
constexpr std::int8_t kFallenAngelState = 46;
constexpr std::int8_t kLastSteppedState = 59;

// LES MERCENAIRES HUMAINS, dont le .act range ses gestes comme celui d'un
// joueur.
constexpr std::uint32_t kFirstHumanMercenaryJob = 6017;
constexpr std::uint32_t kLastHumanMercenaryJob = 6046;
constexpr std::size_t kMercenaryAnimationAttack = 10;
constexpr std::size_t kMercenaryAnimationDie = 8;
constexpr std::size_t kMercenaryAnimationHurt = 6;
constexpr std::size_t kMercenaryAnimationUse = 12;

std::optional<std::size_t> HumanMercenaryAnimation(std::int8_t action_state) noexcept {
  switch (action_state) {
    case kIdleState:
    case kSleepState:
      return kAnimationIdle;
    case kWalkState:
      return kAnimationWalk;
    case kAttackState:
      return kMercenaryAnimationAttack;
    case kDieState:
      return kMercenaryAnimationDie;
    case kHurtState:
      return kMercenaryAnimationHurt;
    case kUseState:
      return kMercenaryAnimationUse;
    default:
      return std::nullopt;
  }
}

std::optional<std::size_t> UseAnimation(CreatureMotionFamily family) noexcept {
  switch (family) {
    case CreatureMotionFamily::kSkillRank:
      return kHomunculusAnimationSkill;
    case CreatureMotionFamily::kMercenary:
      return kCreatureAnimationAttack;
    case CreatureMotionFamily::kNpc:
    case CreatureMotionFamily::kOrdinary:
      break;
  }
  return std::nullopt;
}

}  // namespace

std::optional<std::size_t> CreatureActionAnimationFor(std::int8_t action_state,
                                                      CreatureMotionFamily family,
                                                      std::uint32_t job_id) noexcept {
  if (action_state < kIdleState) {
    return std::nullopt;
  }
  if (family == CreatureMotionFamily::kNpc) {
    return kAnimationIdle;
  }
  if (job_id >= kFirstHumanMercenaryJob && job_id <= kLastHumanMercenaryJob) {
    return HumanMercenaryAnimation(action_state);
  }
  switch (action_state) {
    case kIdleState:
    case kSleepState:
      return kAnimationIdle;
    case kWalkState:
      return kAnimationWalk;
    case kAttackState:
      return kCreatureAnimationAttack;
    case kDieState:
      return kCreatureAnimationDie;
    case kHurtState:
      return kCreatureAnimationHurt;
    case kUseState:
      return UseAnimation(family);
    default:
      return std::nullopt;
  }
}

bool CreatureStateFreezes(std::int8_t action_state) noexcept {
  switch (action_state) {
    case kSitState:
    case kFrozenState:
    case kFirstUnsteppedState:
    case kSecondUnsteppedState:
    case kThirdUnsteppedState:
    case kFallenAngelState:
      return true;
    default:
      return action_state > kLastSteppedState;
  }
}

}  // namespace ro::render
