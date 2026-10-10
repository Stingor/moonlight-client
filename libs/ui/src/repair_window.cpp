// La fenêtre de réparation. Voir l'en-tête.

#include "ro/ui/repair_window.h"

#include <string>
#include <string_view>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/bitmap_button.h"
#include "ro/ui/skin_palette.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skinned_window.h"

namespace ro::ui {
namespace {

[[nodiscard]] float Scale() { return ImGui::GetStyle().FontScaleMain; }

// UNE LIGNE : l'icône et le nom à gauche, le matériau et sa quantité à droite.
void DrawRow(const RepairRowView& row, ImVec2 top, float right_edge, float scale, ImU32 text) {
  ImDrawList* const into = ImGui::GetWindowDrawList();
  const float row_height = kRepairRowHeight * scale;
  const float text_drop = (row_height - ImGui::GetTextLineHeight()) * 0.5F;
  float left = top.x;
  if (row.icon.usable()) {
    into->AddImage(row.icon.id, top, ImVec2(top.x + row_height, top.y + row_height));
  }
  left += row_height + ImGui::GetStyle().ItemSpacing.x;
  const std::string name(row.name);
  into->AddText(ImVec2(left, top.y + text_drop), text, name.c_str());
  if (row.material.empty()) {
    return;
  }
  const std::string owned = std::to_string(row.material_owned);
  const float owned_width = ImGui::CalcTextSize(owned.c_str()).x;
  const float right = right_edge - ImGui::GetStyle().ItemSpacing.x;
  into->AddText(ImVec2(right - owned_width, top.y + text_drop),
                row.material_owned > 0 ? text : kRepairMissingColor, owned.c_str());
  if (row.material_icon.usable()) {
    const float icon_right = right - owned_width - ImGui::GetStyle().ItemSpacing.x;
    into->AddImage(row.material_icon.id, ImVec2(icon_right - row_height, top.y),
                   ImVec2(icon_right, top.y + row_height));
  }
}

}  // namespace

RepairWindowAction DrawRepairWindow(RepairWindowState& state, const RepairView& view,
                                    const SkinArt& art) {
  RepairWindowAction action;
  if (!state.open) {
    state.was_open = false;
    state.selected.reset();
    return action;
  }
  if (!state.was_open) {
    state.was_open = true;
    state.selected.reset();
    ImGui::SetNextWindowFocus();
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_FirstUseEver, ImVec2(0.5F, 0.5F));
  }
  const float scale = Scale();
  const float width = kRepairWindowWidth * scale;
  const float height = kRepairWindowHeight * scale;
  ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);
  std::string name = core::Tr("Items réparables");
  name += "###";
  name += kRepairWindowId;
  bool open = true;
  const bool begun =
      BeginSkinnedWindow(name.c_str(), &open, art, SkinnedTitle::kBar,
                         ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  // LA CROIX VAUT « ANNULER ».
  if (!open) {
    action.decided = true;
  }
  if (!begun) {
    EndSkinnedWindow();
    return action;
  }

  // LA LISTE, sur son fond, qui défile au-delà de ce qu'elle montre.
  const ImVec2 corner = ImGui::GetWindowPos();
  const ImVec2 list_top(corner.x + kRepairListOrigin.x * scale,
                        corner.y + kRepairListOrigin.y * scale);
  const ImVec2 list_size(width - kRepairListWidthMargin * scale,
                         height - kRepairListHeightMargin * scale);
  ImGui::SetCursorScreenPos(list_top);
  ImGui::PushStyleColor(ImGuiCol_ChildBg, kRepairListColor);
  ImGui::PushStyleColor(ImGuiCol_Text, art.look().text);
  if (ImGui::BeginChild("##repairables", list_size)) {
    const float row_height = kRepairRowHeight * scale;
    for (std::size_t rank = 0; rank < view.rows.size(); ++rank) {
      const RepairRowView& row = view.rows[rank];
      ImGui::PushID(static_cast<int>(rank));
      const ImVec2 top = ImGui::GetCursorScreenPos();
      const float right_edge = top.x + ImGui::GetContentRegionAvail().x;
      if (ImGui::Selectable("##row", state.selected == rank, ImGuiSelectableFlags_AllowDoubleClick,
                            ImVec2(0.0F, row_height))) {
        state.selected = rank;
        // LE DOUBLE-CLIC RÉPARE d'un geste, sans passer par « OK ».
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
          action.decided = true;
          action.chosen = rank;
        }
      }
      if (!row.material.empty() && ImGui::IsItemHovered()) {
        SkinTooltipFormat(core::Tr("Consomme : %.*s (%d en stock)"),
                          static_cast<int>(row.material.size()), row.material.data(),
                          row.material_owned);
      }
      DrawRow(row, top, right_edge, scale, art.look().text);
      ImGui::PopID();
    }
  }
  ImGui::EndChild();
  ImGui::PopStyleColor(2);

  // LES DEUX BOUTONS, depuis le coin bas-droit.
  const float buttons_top = height - kRepairButtonsFromBottom * scale;
  const ImVec2 button(kRepairButtonSize.x * scale, kRepairButtonSize.y * scale);
  ImGui::SetCursorScreenPos(
      ImVec2(corner.x + width - kRepairOkFromRight * scale, corner.y + buttons_top));
  // ENTRÉE VAUT « OK » quand la fenêtre a la main.
  const bool entered =
      ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
      (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter));
  if ((BitmapButton("##repair_ok", art, kOkFaces, core::Tr("OK"), button) || entered) &&
      state.selected.has_value() && *state.selected < view.rows.size()) {
    action.decided = true;
    action.chosen = state.selected;
  }
  ImGui::SetCursorScreenPos(
      ImVec2(corner.x + width - kRepairCancelFromRight * scale, corner.y + buttons_top));
  if (BitmapButton("##repair_cancel", art, kCancelFaces, core::Tr("Annuler"), button)) {
    action.decided = true;
    action.chosen.reset();
  }
  EndSkinnedWindow();
  return action;
}

}  // namespace ro::ui
