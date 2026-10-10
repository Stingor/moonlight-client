#include "ro/ui/staff_skill_id.h"

#include <array>
#include <cstdio>

#include "ro/core/i18n.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skinned_window.h"

#include "imgui.h"

namespace ro::ui {
namespace {

// Un numéro sur cinq chiffres et un nom interne d'une quarantaine de lettres
// au plus, avec leur habillage.
constexpr std::size_t kStaffSkillIdBytes = 96;

}  // namespace

std::string StaffSkillIdText(StaffSkillId skill) {
  if (skill.skill_id == 0) {
    return {};
  }
  std::array<char, kStaffSkillIdBytes> line = {};
  if (skill.idname.empty()) {
    std::snprintf(line.data(), line.size(), core::Tr("ID : %u"),
                  static_cast<unsigned>(skill.skill_id));
  } else {
    std::snprintf(line.data(), line.size(), core::Tr("ID : %u (%.*s)"),
                  static_cast<unsigned>(skill.skill_id), static_cast<int>(skill.idname.size()),
                  skill.idname.data());
  }
  return line.data();
}

void DrawStaffSkillId(bool staff, StaffSkillId skill) {
  if (!staff || skill.skill_id == 0) {
    return;
  }
  ImGui::TextDisabled("%s", StaffSkillIdText(skill).c_str());
}

void SkinTooltipWithStaffSkillId(const char* text, bool staff, StaffSkillId skill) {
  if (!staff || skill.skill_id == 0) {
    SkinTooltipFormat("%s", text);
    return;
  }
  if (text == nullptr || !BeginSkinTooltip()) {
    return;
  }
  // ⚠ REPLIÉE COMME `SkinTooltipFormat` : la même bulle, une ligne de plus.
  const float wrap = kSkinnedTooltipWidest - 2.0F * ImGui::GetStyle().WindowPadding.x;
  ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + wrap);
  ImGui::TextUnformatted(text);
  ImGui::PopTextWrapPos();
  DrawStaffSkillId(staff, skill);
  EndSkinTooltip();
}

}  // namespace ro::ui
