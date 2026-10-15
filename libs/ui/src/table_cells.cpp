// Les cellules de table. Voir l'en-tête.

#include "table_cells.h"

#include <algorithm>

#include "ro/ui/skin_tooltip.h"

namespace ro::ui {
namespace {

[[nodiscard]] float TextWidth(std::string_view text) {
  return ImGui::CalcTextSize(text.data(), text.data() + text.size()).x;
}

}  // namespace

float WidestText(std::initializer_list<std::string_view> texts) {
  float widest = 0.0F;
  for (const std::string_view text : texts) {
    widest = std::max(widest, TextWidth(text));
  }
  return widest;
}

float SortedHeaderWidth(std::string_view label) {
  // La flèche d'ImGui tient dans une hauteur de police, marge comprise.
  return TextWidth(label) + ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.x;
}

bool OverflowsCell(std::string_view text) {
  return TextWidth(text) > ImGui::GetContentRegionAvail().x;
}

void TooltipWhole(std::string_view text) {
  SkinTooltipFormat("%.*s", static_cast<int>(text.size()), text.data());
}

void CellText(std::string_view text, ImU32 color) {
  const bool overflows = OverflowsCell(text);
  if (color != 0) {
    ImGui::PushStyleColor(ImGuiCol_Text, color);
  }
  ImGui::TextUnformatted(text.data(), text.data() + text.size());
  if (color != 0) {
    ImGui::PopStyleColor();
  }
  if (overflows && ImGui::IsItemHovered()) {
    TooltipWhole(text);
  }
}

}  // namespace ro::ui
