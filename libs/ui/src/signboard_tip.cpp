#include "ro/ui/signboard_tip.h"

#include "ro/ui/skin_tooltip.h"

namespace ro::ui {
namespace {

// Le bord droit et le bord bas sont exclus : une zone de 106 pixels n'en
// couvre pas 107.
[[nodiscard]] bool Inside(ImVec2 point, ImVec2 min, ImVec2 max) {
  return point.x >= min.x && point.x < max.x && point.y >= min.y && point.y < max.y;
}

}  // namespace

void DrawSignboardTip(std::span<const SignboardTipView> boards) {
  const ImGuiIO& io = ImGui::GetIO();
  if (io.WantCaptureMouse) {
    return;
  }
  for (auto board = boards.rbegin(); board != boards.rend(); ++board) {
    if (!Inside(io.MousePos, board->board_min, board->board_max)) {
      continue;
    }
    if (!board->cut_title.empty() && Inside(io.MousePos, board->title_min, board->title_max)) {
      ImGui::SetNextWindowPos(ImVec2(board->title_min.x + kSignboardTipOffset.x,
                                     board->title_min.y + kSignboardTipOffset.y));
      SkinTooltipFormat("%s", board->cut_title.c_str());
    }
    return;
  }
}

}  // namespace ro::ui
