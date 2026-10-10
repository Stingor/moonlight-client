#include "ro/render/monster_skill_motion.h"

#include <string>

namespace ro::render {
namespace {

constexpr std::uint32_t kFirstMonsterClass = 1001;
constexpr std::uint32_t kLastMonsterClass = 3998;
constexpr std::uint32_t kFirstExtendedClass = 20021;
constexpr std::uint32_t kLastExtendedClass = 32042;
// L'action de la table compte à partir de un.
constexpr std::int32_t kFirstTableAction = 1;
constexpr std::int32_t kOneShotLoopType = 0;

}  // namespace

bool IsMonsterOrNpcClass(std::uint32_t job) noexcept {
  return (job >= kFirstMonsterClass && job <= kLastMonsterClass) ||
         (job >= kFirstExtendedClass && job <= kLastExtendedClass);
}

bool SkillUsePosedByTable(std::uint32_t job, std::uint16_t skill_id) noexcept {
  return IsMonsterOrNpcClass(job) || skill_id == kSkillPosedByTableForAll;
}

std::optional<MonsterSkillMotion> MonsterSkillMotionFor(const SpriteCatalog& catalog,
                                                        std::uint32_t job, std::uint16_t skill_id) {
  if (IsPlayerClass(job)) {
    return std::nullopt;
  }
  const std::optional<formats::MonsterSkillAction> gesture =
      catalog.monster_skill_actions.Find(NonPlayerResourceName(catalog, job), skill_id);
  if (!gesture.has_value() || gesture->action < kFirstTableAction) {
    return std::nullopt;
  }
  return MonsterSkillMotion{static_cast<std::size_t>(gesture->action - kFirstTableAction),
                            gesture->loop_type != kOneShotLoopType};
}

std::optional<float> MonsterCastCadenceSteps(std::uint16_t skill_id,
                                             std::uint32_t cast_ms) noexcept {
  if (skill_id != kMetamorphosisSkill) {
    return std::nullopt;
  }
  const std::uint32_t step = cast_ms / kMetamorphosisCastDivisorMs;
  if (step == 0) {
    return std::nullopt;
  }
  return static_cast<float>(step);
}

}  // namespace ro::render
