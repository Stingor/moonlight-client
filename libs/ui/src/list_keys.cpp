// Le clavier d'une liste. Voir l'en-tête.

#include "ro/ui/list_keys.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

#include <imgui_internal.h>

namespace ro::ui {

int ListKeyTarget(int current, int count, int page_rows, ListKeyMove move) {
  if (count <= 0) {
    return kListNoEntry;
  }
  const int last = count - 1;
  const int page = std::max(1, page_rows);
  const bool none = current < 0 || current > last;
  switch (move) {
    case ListKeyMove::kNone:
      return none ? kListNoEntry : current;
    case ListKeyMove::kPrevious:
      return none ? last : std::max(0, current - 1);
    case ListKeyMove::kNext:
      return none ? 0 : std::min(last, current + 1);
    case ListKeyMove::kPageUp:
      return none ? last : std::max(0, current - page);
    case ListKeyMove::kPageDown:
      return none ? 0 : std::min(last, current + page);
    case ListKeyMove::kFirst:
      return 0;
    case ListKeyMove::kLast:
      return last;
  }
  return current;
}

ListKeyMove PressedListKey(bool arrows_only) {
  constexpr std::array<std::pair<ImGuiKey, ListKeyMove>, 2> kArrows = {{
      {ImGuiKey_UpArrow, ListKeyMove::kPrevious},
      {ImGuiKey_DownArrow, ListKeyMove::kNext},
  }};
  constexpr std::array<std::pair<ImGuiKey, ListKeyMove>, 4> kJumps = {{
      {ImGuiKey_PageUp, ListKeyMove::kPageUp},
      {ImGuiKey_PageDown, ListKeyMove::kPageDown},
      {ImGuiKey_Home, ListKeyMove::kFirst},
      {ImGuiKey_End, ListKeyMove::kLast},
  }};
  for (const auto& [key, move] : kArrows) {
    if (ImGui::IsKeyPressed(key, /*repeat=*/true)) {
      return move;
    }
  }
  if (arrows_only) {
    return ListKeyMove::kNone;
  }
  for (const auto& [key, move] : kJumps) {
    if (ImGui::IsKeyPressed(key, /*repeat=*/true)) {
      return move;
    }
  }
  return ListKeyMove::kNone;
}

int VisibleListRows(float row_pitch) {
  const ImGuiWindow* const window = ImGui::GetCurrentWindow();
  if (window == nullptr || row_pitch <= 0.0F) {
    return 1;
  }
  return std::max(1, static_cast<int>(std::floor(window->InnerRect.GetHeight() / row_pitch)));
}

void KeepLastItemInView() { ImGui::ScrollToItem(ImGuiScrollFlags_KeepVisibleEdgeY); }

}  // namespace ro::ui
