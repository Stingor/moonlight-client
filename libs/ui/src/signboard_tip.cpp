#include "ro/ui/signboard_tip.h"

#include <imgui.h>
#include <imgui_internal.h>

#include "ro/ui/skin_tooltip.h"

namespace ro::ui {
void DrawSignboardTip(std::span<const SignboardTipView> boards) {
  const ImGuiIO& io = ImGui::GetIO();
  if (io.WantCaptureMouse) {
    return;
  }
  for (auto board = boards.rbegin(); board != boards.rend(); ++board) {
    if (!ImRect(board->board_min, board->board_max).Contains(io.MousePos)) {
      continue;
    }
    if (!board->cut_title.empty() &&
        ImRect(board->title_min, board->title_max).Contains(io.MousePos)) {
      ImGui::SetNextWindowPos(ImVec2(board->title_min.x + kSignboardTipOffset.x,
                                     board->title_min.y + kSignboardTipOffset.y));
      SkinTooltipFormat("%s", board->cut_title.c_str());
    }
    return;
  }
}

}  // namespace ro::ui
