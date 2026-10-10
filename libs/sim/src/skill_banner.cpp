#include "ro/sim/skill_banner.h"

#include <string>

#include "ro/sim/skill_tree.h"

namespace ro::sim {

std::string SkillBannerText(const SkillTree* skills, std::uint16_t skill_id, bool staff) {
  const SkillNode* node = skills != nullptr ? skills->Find(skill_id) : nullptr;
  std::string text = node != nullptr && !node->display_name.empty()
                         ? node->display_name
                         : "#" + std::to_string(skill_id);
  if (staff) {
    text += " (" + std::to_string(skill_id) + ")";
  }
  text += kSkillNameBannerSuffix;
  return text;
}

}  // namespace ro::sim
