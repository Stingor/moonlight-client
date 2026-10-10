#include "ro/render/skill_unit_pose.h"

#include <algorithm>
#include <cstddef>

namespace ro::render {

std::span<const SkillUnitPoseStep> SkillUnitPoseOf(std::uint32_t unit_id) {
  const std::span<const SkillUnitPoseStep> table = SkillUnitPoseTable();
  const auto trouvees = std::ranges::equal_range(table, unit_id, {}, &SkillUnitPoseStep::unit_id);
  return table.subspan(static_cast<std::size_t>(trouvees.begin() - table.begin()),
                       static_cast<std::size_t>(trouvees.end() - trouvees.begin()));
}

}  // namespace ro::render
