// La liste des sorts d'Autospell. Voir l'en-tête.

#include "ro/ui/spell_list_window.h"

#include <string>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/skin_palette.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"

namespace ro::ui {
namespace {

[[nodiscard]] float Scale() { return ImGui::GetStyle().FontScaleMain; }

}  // namespace

SpellListWindowAction DrawSpellListWindow(SpellListWindowState& state, const SpellListView& view,
                                          const SkinArt& art) {
  SpellListWindowAction action;
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
  // LA TAILLE EST CELLE DU CLIENT, ET IMPOSÉE : lignes et boutons s'y posent à
  // des coordonnées fixes depuis le coin de la fenêtre.
  const float scale = Scale();
  const float side = kSpellListWindowSide * scale;
  ImGui::SetNextWindowSize(ImVec2(side, side), ImGuiCond_Always);
  std::string name = core::Tr("Liste des sorts");
  name += "###";
  name += kSpellListWindowId;
  bool open = true;
  const bool begun =
      BeginSkinnedWindow(name.c_str(), &open, art, SkinnedTitle::kBar,
                         ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  // LA CROIX VAUT « ANNULER » : le serveur attend une réponse.
  if (!open) {
    action.answer = kSpellListRefusal;
  }
  if (!begun) {
    EndSkinnedWindow();
    return action;
  }

  // LES LIGNES, dans un cadre qui défile au-delà de quatre.
  const ImVec2 corner = ImGui::GetWindowPos();
  ImGui::SetCursorScreenPos(ImVec2(corner.x, corner.y + kSpellListFirstRowTop * scale));
  const float list_height = kSpellListRowStep * scale * static_cast<float>(kSpellListVisibleRows);
  if (ImGui::BeginChild("##spells", ImVec2(side, list_height), ImGuiChildFlags_None,
                        ImGuiWindowFlags_NoBackground)) {
    ImDrawList* const into = ImGui::GetWindowDrawList();
    for (std::size_t rank = 0; rank < view.rows.size(); ++rank) {
      const SpellListRow& row = view.rows[rank];
      const ImVec2 top = ImGui::GetCursorScreenPos();
      ImGui::PushID(static_cast<int>(rank));
      if (ImGui::InvisibleButton("##row", ImVec2(side, kSpellListRowStep * scale))) {
        state.selected = rank;
      }
      // LE DOUBLE-CLIC CHOISIT d'un geste, sans passer par « OK ».
      if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        action.answer = row.skill_id;
      }
      if (row.armed_level != 0 && ImGui::IsItemHovered()) {
        SkinTooltipFormat(core::Tr("Armé au niveau %d"), static_cast<int>(row.armed_level));
      }
      ImGui::PopID();
      if (state.selected == rank) {
        const float highlight_top = top.y - kSpellListHighlightRise * scale;
        into->AddRectFilled(ImVec2(top.x + kSpellListHighlightLeft * scale, highlight_top),
                            ImVec2(top.x + side - kSpellListHighlightRightMargin * scale,
                                   highlight_top + kSpellListHighlightHeight * scale),
                            ImGui::GetColorU32(ImGuiCol_Header));
      }
      if (row.icon.usable()) {
        const ImVec2 icon(top.x + kSpellListIconLeft * scale, top.y);
        into->AddImage(row.icon.id, icon,
                       ImVec2(icon.x + row.icon.width * scale, icon.y + row.icon.height * scale));
      }
      const std::string label(row.name);
      into->AddText(ImVec2(top.x + kSpellListNameLeft * scale, top.y + kSpellListNameDrop * scale),
                    art.look().text, label.c_str());
    }
  }
  ImGui::EndChild();

  // LES DEUX BOUTONS, depuis le coin bas-droit.
  const float buttons_top = side - kSpellListButtonsFromBottom * scale;
  ImGui::SetCursorScreenPos(
      ImVec2(corner.x + side - kSpellListOkFromRight * scale, corner.y + buttons_top));
  ImGui::BeginDisabled(!state.selected.has_value());
  // ENTRÉE VAUT « OK » quand la fenêtre a la main.
  const bool entered =
      ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
      (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter));
  if ((SkinButton(core::Tr("OK"), art) || entered) && state.selected.has_value() &&
      *state.selected < view.rows.size()) {
    action.answer = view.rows[*state.selected].skill_id;
  }
  ImGui::EndDisabled();
  ImGui::SetCursorScreenPos(
      ImVec2(corner.x + side - kSpellListCancelFromRight * scale, corner.y + buttons_top));
  if (SkinButton(core::Tr("Annuler"), art)) {
    action.answer = kSpellListRefusal;
  }
  EndSkinnedWindow();
  return action;
}

}  // namespace ro::ui
