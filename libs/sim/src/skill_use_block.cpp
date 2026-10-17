#include "ro/sim/skill_use_block.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <span>

#include "ro/sim/actor.h"
#include "ro/sim/world.h"

namespace ro::sim {
namespace {

// Le corps pris, et l'état qui le nomme. Stone Curse en cours de prise et la
// brûlure ne retiennent pas le lancer : le serveur les excepte.
struct HeldBody {
  std::uint16_t body_state = 0;
  StatusEffectId status = 0;
};
constexpr std::array kHeldBodies = {
    HeldBody{kBodyStone, kEfstStoneCurse},    HeldBody{kBodyFrozen, kEfstFreezing},
    HeldBody{kBodyStunned, kEfstStun},        HeldBody{kBodyAsleep, kEfstSleep},
    HeldBody{kBodyImprisoned, kEfstImprison},
};

// Les états à icône qui interdisent de lancer.
constexpr std::array kNoCastStatuses = {kEfstBerserk, kEfstSteelBody, kEfstDeepSleep,
                                        kEfstSaturdayNightFever};

[[nodiscard]] bool Frees(std::uint16_t skill_id) {
  return skill_id == kRefreshSkill || skill_id == kGentleTouchCureSkill ||
         skill_id == kGroomingSkill;
}

[[nodiscard]] bool Holds(std::span<const ActiveStatus> statuses, StatusEffectId effect) {
  return std::ranges::any_of(statuses,
                             [effect](const ActiveStatus& held) { return held.effect == effect; });
}

}  // namespace

std::optional<SkillUseBlock> SkillUseBlockFor(const World& world, std::uint16_t skill_id,
                                              bool lent_by_item) {
  const Actor* self = world.Find(world.player_id());
  if (self == nullptr) {
    return std::nullopt;
  }
  if (self->posture == Posture::kSitting) {
    return SkillUseBlock{.reason = LocalRefusal::kSitting};
  }
  const std::span<const ActiveStatus> statuses = world.player_statuses();
  if (!Frees(skill_id)) {
    for (const HeldBody& held : kHeldBodies) {
      if (self->body_state == held.body_state) {
        return SkillUseBlock{.reason = LocalRefusal::kStatus, .status = held.status};
      }
    }
  }
  if (Holds(statuses, kStatusAllRiding)) {
    return SkillUseBlock{.reason = LocalRefusal::kStatus, .status = kStatusAllRiding};
  }
  if (Holds(statuses, kEfstTrickDead) && skill_id != kPlayDeadSkill) {
    return SkillUseBlock{.reason = LocalRefusal::kStatus, .status = kEfstTrickDead};
  }
  if (!Frees(skill_id) && !lent_by_item) {
    if ((self->health_state & kHealthSilenced) != 0) {
      return SkillUseBlock{.reason = LocalRefusal::kStatus, .status = kEfstSilence};
    }
    for (const StatusEffectId effect : kNoCastStatuses) {
      if (Holds(statuses, effect)) {
        return SkillUseBlock{.reason = LocalRefusal::kStatus, .status = effect};
      }
    }
  }
  if (!lent_by_item && world.player_status().manner < 0) {
    return SkillUseBlock{.reason = LocalRefusal::kMuted};
  }
  if ((self->option & kOptionChaseWalk) != 0 && skill_id != kChaseWalkSkill) {
    return SkillUseBlock{.reason = LocalRefusal::kChaseWalk};
  }
  return std::nullopt;
}

SkillRefusal BlockedRefusal(std::uint16_t skill_id, const SkillUseBlock& block,
                            std::uint32_t now_ms) {
  return SkillRefusal{.skill_id = skill_id,
                      .refused_ms = now_ms,
                      .local = block.reason,
                      .blocking_status = block.status};
}

bool SkillBlockGate::Refuse(World& world, std::uint16_t skill_id, bool lent_by_item,
                            std::uint32_t now_ms) {
  const std::optional<SkillUseBlock> block = SkillUseBlockFor(world, skill_id, lent_by_item);
  if (!block.has_value()) {
    said_.reset();
    return false;
  }
  const Said said{.skill_id = skill_id, .block = *block};
  if (said_ != said) {
    world.skill_book().RecordRefusal(BlockedRefusal(skill_id, *block, now_ms));
    said_ = said;
  }
  return true;
}

}  // namespace ro::sim
