#include "ro/ui/window_focus.h"

#include <imgui.h>
#include <imgui_internal.h>

namespace ro::ui {

bool WindowHasFocus(const char* window_id) {
  if (window_id == nullptr || ImGui::GetCurrentContext() == nullptr) return false;
  ImGuiWindow* window = ImGui::FindWindowByName(window_id);
  ImGuiWindow* focused = ImGui::GetCurrentContext()->NavWindow;
  if (window == nullptr || focused == nullptr || !window->WasActive) return false;
  return ImGui::IsWindowChildOf(focused, window, /*popup_hierarchy=*/true);
}

}  // namespace ro::ui
