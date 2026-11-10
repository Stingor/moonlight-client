#include "palette_retouches.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

#include "ro/core/i18n.h"
#include "ro/ui/skin_popup.h"
#include "ro/ui/skin_widgets.h"

#include "palette_pieces.h"
#include "ui_scale.h"

namespace ro::ui {
namespace {

constexpr float kFullChannel = 255.0F;
// La pastille d'une retouche, en points d'interface : celle d'une pièce.
constexpr float kSwatchPoints = 20.0F;
constexpr const char* kPopupId = "retouche";

[[nodiscard]] std::array<float, 3> Channels(ImU32 color) {
  return {static_cast<float>((color >> IM_COL32_R_SHIFT) & 0xFFU) / kFullChannel,
          static_cast<float>((color >> IM_COL32_G_SHIFT) & 0xFFU) / kFullChannel,
          static_cast<float>((color >> IM_COL32_B_SHIFT) & 0xFFU) / kFullChannel};
}

[[nodiscard]] std::array<float, 3> Channels(const std::array<std::uint8_t, 3>& rgb) {
  return {static_cast<float>(rgb[0]) / kFullChannel, static_cast<float>(rgb[1]) / kFullChannel,
          static_cast<float>(rgb[2]) / kFullChannel};
}

[[nodiscard]] std::uint8_t ToByte(float channel) {
  return static_cast<std::uint8_t>(std::clamp(std::lround(channel * kFullChannel), 0L, 255L));
}

// La couleur qu'un index montre : sa retouche, sinon la palette peinte.
[[nodiscard]] std::array<float, 3> ShownColor(const PaletteEditorView& view, int index) {
  for (const render::PaletteRetouch& retouch : view.retouches) {
    if (retouch.index == index) {
      return Channels(retouch.rgb);
    }
  }
  if (index >= 0 && static_cast<std::size_t>(index) < view.palette_colors.size()) {
    return Channels(view.palette_colors[static_cast<std::size_t>(index)]);
  }
  return {1.0F, 1.0F, 1.0F};
}

// Le nuancier de l'index visé. Rend vrai quand il est ouvert.
bool DrawRetouchPopup(PaletteEditorState& state, const PaletteEditorView& view, const SkinArt& art,
                      PaletteEditorAction& action) {
  if (state.retouch_opening) {
    ImGui::OpenPopup(kPopupId);
    state.retouch_opening = false;
  }
  if (!BeginSkinPopup(kPopupId)) {
    state.retouch_index = -1;
    return false;
  }
  char title[64] = {};
  std::snprintf(title, sizeof(title), core::Tr("Couleur n° %d"), state.retouch_index);
  ImGui::TextUnformatted(title);
  std::array<float, 3> rgb = ShownColor(view, state.retouch_index);
  if (DrawPaletteColorPicker(rgb, art)) {
    action.retouch_index = state.retouch_index;
    action.retouch_color = {ToByte(rgb[0]), ToByte(rgb[1]), ToByte(rgb[2])};
  }
  if (SkinButton(core::Tr("Fermer"), art)) {
    ImGui::CloseCurrentPopup();
  }
  EndSkinPopup();
  return true;
}

}  // namespace

bool DrawPaletteRetouches(PaletteEditorState& state, const PaletteEditorView& view,
                          const SkinArt& art, PaletteEditorAction& action) {
  if (!view.retouches_allowed) {
    state.retouch_mode = false;
    state.retouch_index = -1;
    return false;
  }
  // 🔴 L'ONGLET DANS L'IDENTIFIANT : la liste d'un chapeau ne devient pas celle
  // du corps quand l'onglet change sous elle.
  ImGui::PushID("retouches");
  ImGui::PushID(static_cast<int>(view.tab));
  char label[96] = {};
  std::snprintf(label, sizeof(label), "%s (%zu / %d)###mode", core::Tr("Retouche exacte"),
                view.retouches.size(), view.retouches_most);
  static_cast<void>(SkinCheckBox(label, &state.retouch_mode, art));
  if (ImGui::IsItemHovered()) {
    SkinTooltip(
        core::Tr("Clique une couleur sur le pantin : elle se repeint exactement, et seule, "
                 "par-dessus les pièces et le reste. Pour un détail qu'aucune pièce ne "
                 "sépare."));
  }
  const float side = kSwatchPoints * Scale();
  for (const render::PaletteRetouch& retouch : view.retouches) {
    ImGui::PushID(retouch.index);
    const std::array<float, 3> rgb = Channels(retouch.rgb);
    if (ImGui::ColorButton("##pastille", ImVec4(rgb[0], rgb[1], rgb[2], 1.0F),
                           ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoDragDrop |
                               ImGuiColorEditFlags_NoTooltip,
                           ImVec2(side, side))) {
      state.retouch_index = retouch.index;
      state.retouch_opening = true;
    }
    ImGui::SameLine();
    char name[64] = {};
    std::snprintf(name, sizeof(name), core::Tr("Couleur n° %d"), retouch.index);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(name);
    ImGui::SameLine();
    if (SkinSmallButton("x", art)) {
      action.remove_retouch = retouch.index;
    }
    if (ImGui::IsItemHovered()) {
      SkinTooltip(core::Tr("Retirer cette retouche"));
    }
    ImGui::PopID();
  }
  const bool open = state.retouch_index >= 0 && DrawRetouchPopup(state, view, art, action);
  ImGui::PopID();
  ImGui::PopID();
  return open;
}

}  // namespace ro::ui
