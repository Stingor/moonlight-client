// Les cellules de table. Voir l'en-tête.

#include "table_cells.h"

#include <algorithm>

#include "ro/core/printed.h"
#include "ro/ui/skin_tooltip.h"

#include "ui_text.h"

namespace ro::ui {

namespace {

constexpr double kThousandthsPerPercent = 1000.0;

}  // namespace

MapLinkGesture MapLinkCell(std::string_view label) {
  // Sans l'opacité de la fenêtre : `CellText` la pose en couleur de texte, et
  // ImGui l'applique en écrivant.
  CellText(label, ImGui::ColorConvertFloat4ToU32(ImGui::GetStyleColorVec4(ImGuiCol_TextLink)));
  const bool hovered = ImGui::IsItemHovered();
  DecorateLink(hovered);
  // Un lien se suit au relâchement, comme tout lien du client.
  return MapLinkGesture{.hovered = hovered,
                        .released = hovered && ImGui::IsMouseReleased(ImGuiMouseButton_Left)};
}

std::string MapDropRateText(std::uint32_t rate_thousandths) {
  return core::Printed("%.3f%%", static_cast<double>(rate_thousandths) / kThousandthsPerPercent);
}

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
  Unformatted(text);
  if (color != 0) {
    ImGui::PopStyleColor();
  }
  if (overflows && ImGui::IsItemHovered()) {
    TooltipWhole(text);
  }
}

}  // namespace ro::ui
