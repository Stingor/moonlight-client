#include "craft_window_parts.h"

#include <algorithm>
#include <cctype>
#include <cfloat>

#include "ro/ui/skin_tooltip.h"

namespace ro::ui::craft_parts {

bool ContainsFolded(std::string_view haystack, std::string_view needle) {
  if (needle.empty()) {
    return true;
  }
  const auto folded_equal = [](char left, char right) {
    return std::tolower(static_cast<unsigned char>(left)) ==
           std::tolower(static_cast<unsigned char>(right));
  };
  return !std::ranges::search(haystack, needle, folded_equal).empty();
}

void ColoredWrapped(ImU32 color, const char* text) {
  ImGui::PushStyleColor(ImGuiCol_Text, color);
  ImGui::TextWrapped("%s", text);
  ImGui::PopStyleColor();
}

void ColoredText(ImU32 color, const char* text) {
  ImGui::PushStyleColor(ImGuiCol_Text, color);
  ImGui::TextUnformatted(text);
  ImGui::PopStyleColor();
}

void BulletWrapped(const char* text) {
  ImGui::Bullet();
  ImGui::SameLine();
  ImGui::TextWrapped("%s", text);
}

bool ItemLink(const char* id, const SkinImage& icon, float icon_pixels, const char* text,
              ImU32 color, bool* hovered) {
  ImGui::PushID(id);
  ImGui::BeginGroup();
  const float side = Px(icon_pixels);
  if (icon.usable()) {
    ImGui::Image(icon.id, ImVec2(side, side));
  } else {
    ImGui::Dummy(ImVec2(side, side));
  }
  ImGui::SameLine(0.0F, ImGui::GetStyle().ItemInnerSpacing.x);
  const ImVec2 text_at = ImGui::GetCursorScreenPos();
  ColoredText(color, text);
  ImGui::EndGroup();
  ImGui::PopID();

  const bool over = ImGui::IsItemHovered();
  if (hovered != nullptr) {
    *hovered = over;
  }
  if (!over) {
    return false;
  }
  // SOULIGNÉ SOUS LE TEXTE SEUL, dans sa couleur : c'est ce qui dit « lien ».
  const ImVec2 size = ImGui::CalcTextSize(text);
  const float base = text_at.y + ImGui::GetTextLineHeight();
  ImGui::GetWindowDrawList()->AddLine(ImVec2(text_at.x, base), ImVec2(text_at.x + size.x, base),
                                      color);
  ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
  return ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
         ImGui::IsMouseClicked(ImGuiMouseButton_Right);
}

void DrawHistory(const char* id, std::span<const CraftLogLineView> lines, float height,
                 bool show_time, bool wrap) {
  ImGui::Separator();
  if (ImGui::BeginChild(id, ImVec2(-FLT_MIN, height), ImGuiChildFlags_Borders)) {
    for (const CraftLogLineView& line : lines) {
      if (show_time && !line.time.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, PaintingLook().faint_text);
        ImGui::TextUnformatted(line.time.data(), line.time.data() + line.time.size());
        ImGui::PopStyleColor();
        ImGui::SameLine();
      }
      // L'ALPHA EST FORCÉ À UN : une ligne du journal ne s'estompe pas avec la
      // liste qu'elle raconte. La couleur est celle de son rôle, dans l'habillage
      // du moment.
      ImGui::PushStyleColor(ImGuiCol_Text, PaintingLook().Tone(line.tone) | IM_COL32_A_MASK);
      if (wrap) {
        ImGui::PushTextWrapPos(0.0F);
      }
      ImGui::TextUnformatted(line.text.data(), line.text.data() + line.text.size());
      if (wrap) {
        ImGui::PopTextWrapPos();
      }
      ImGui::PopStyleColor();
    }
    // COLLÉ EN BAS tant qu'on y était déjà : une ligne neuve se voit, et qui
    // remonte lire l'ancienne n'est pas ramené de force.
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0F) {
      ImGui::SetScrollHereY(1.0F);
    }
  }
  ImGui::EndChild();
}

}  // namespace ro::ui::craft_parts
