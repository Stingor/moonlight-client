#include "ro/render/skill_unit_pose.h"

#include "ro/core/sorted_table.h"

namespace ro::render {

std::span<const SkillUnitPoseStep> SkillUnitPoseOf(std::uint32_t unit_id) {
  return core::RowsWithKey(SkillUnitPoseTable(), unit_id, &SkillUnitPoseStep::unit_id);
}

}  // namespace ro::render
