// Les écritures que les fichiers de la fiche de monstre se partagent : mesures,
// libellés, couleurs, aide et liens. Voir `monster_sheet_parts.h`.

#include <cstdarg>
#include <cstdio>
#include <string>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/skin_tooltip.h"

#include "monster_sheet_parts.h"
#include "table_cells.h"

namespace ro::ui::monster_sheet {
namespace {

// La largeur d'une bulle d'aide, en caractères de la police.
constexpr float kHelpWrapChars = 32.0F;

// La plus longue ligne qu'on imprime ; au-delà, elle est tronquée.
constexpr std::size_t kPrintedBytes = 256;

}  // namespace

float Px(float pixels) { return pixels * ImGui::GetStyle().FontScaleMain; }

const char* BossBadge(std::uint8_t rank) {
  if (rank == kBossRankMini) {
    return core::Tr("Mini-boss");
  }
  if (rank == kBossRankMvp) {
    return core::Tr("MVP");
  }
  return nullptr;
}

void DrawBossBadge(std::uint8_t rank) {
  if (const char* badge = BossBadge(rank); badge != nullptr) {
    ImGui::SameLine();
    Colored(rank == kBossRankMvp ? kRedColor : kAmberColor, Printed("[%s]", badge).c_str());
  }
}

std::string Printed(const char* format, ...) {
  char text[kPrintedBytes] = {};
  std::va_list arguments;
  va_start(arguments, format);
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
  std::vsnprintf(text, sizeof(text), format, arguments);
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
  va_end(arguments);
  return text;
}

void Label(const char* text) { Colored(kLabelColor, text); }

void LabelWrapped(const char* text) {
  ImGui::PushStyleColor(ImGuiCol_Text, kLabelColor);
  ImGui::PushTextWrapPos(0.0F);
  ImGui::TextUnformatted(text);
  ImGui::PopTextWrapPos();
  ImGui::PopStyleColor();
}

ImU32 LinkColor() {
  return ImGui::ColorConvertFloat4ToU32(ImGui::GetStyleColorVec4(ImGuiCol_TextLink));
}

void Colored(ImU32 color, const char* text) {
  // Coupé par sa colonne, le texte se lit entier au survol.
  CellText(text, color);
}

void Help(const char* text) {
  ImGui::SameLine();
  Colored(kLabelColor, "(?)");
  if (!ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort) || !BeginSkinTooltip()) {
    return;
  }
  ImGui::PushTextWrapPos(ImGui::GetFontSize() * kHelpWrapChars);
  ImGui::TextUnformatted(text);
  ImGui::PopTextWrapPos();
  EndSkinTooltip();
}

void DecorateLink(bool hovered) {
  if (!hovered) {
    return;
  }
  ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
  const ImVec2 least = ImGui::GetItemRectMin();
  const ImVec2 most = ImGui::GetItemRectMax();
  ImGui::GetWindowDrawList()->AddLine(ImVec2(least.x, most.y), ImVec2(most.x, most.y),
                                      ImGui::GetColorU32(ImGuiCol_TextLink));
}

}  // namespace ro::ui::monster_sheet
