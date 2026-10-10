#include "ro/ui/bitmap_button.h"

#include "ro/ui/skin_widgets.h"

namespace ro::ui {

bool BitmapButton(const char* id, const SkinArt& art, const ButtonFaces& faces,
                  const char* fallback_label, ImVec2 size) {
  const SkinImage& out = art.Get(faces.out);
  if (!out.usable()) {
    return SkinButton(fallback_label, art, size);
  }
  const ImVec2 at = ImGui::GetCursorScreenPos();
  const bool clicked = ImGui::InvisibleButton(id, size);
  const bool hovered = ImGui::IsItemHovered();
  if (hovered) {
    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
  }
  const SkinImage& over = art.Get(faces.over);
  const SkinImage& down = art.Get(faces.down);
  const SkinImage& shown = ImGui::IsItemActive() && hovered && down.usable()    ? down
                           : !ImGui::IsItemActive() && hovered && over.usable() ? over
                                                                                : out;
  ImGui::GetWindowDrawList()->AddImage(shown.id, at, ImVec2(at.x + size.x, at.y + size.y));
  return clicked;
}

}  // namespace ro::ui
