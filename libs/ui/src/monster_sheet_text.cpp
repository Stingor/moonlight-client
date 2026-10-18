// Les écritures que les fichiers de la fiche de monstre se partagent : mesures,
// libellés, couleurs et aide. Voir `monster_sheet_parts.h`.

#include <cstdarg>
#include <cstdio>
#include <string>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/core/printed.h"
#include "ro/ui/skin_tooltip.h"

#include "monster_sheet_parts.h"
#include "table_cells.h"

namespace ro::ui::monster_sheet {
namespace {

// La largeur d'une bulle d'aide, en caractères de la police.
constexpr float kHelpWrapChars = 32.0F;

}  // namespace

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
    Colored(rank == kBossRankMvp ? kRedColor : kAmberColor, core::Printed("[%s]", badge).c_str());
  }
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

}  // namespace ro::ui::monster_sheet
