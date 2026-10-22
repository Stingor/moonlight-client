// LA SECTION « CASES » DU PANNEAU DE L'ÉDITEUR DE CARTE : le pinceau — ce qu'il
// bloque, son rayon —, l'emprise du modèle désigné, ce que le document bloque,
// sa raison, et le retour aux cases de la carte. Voir docs/adr/0131.

#include "ro/ui/map_editor_cells_panel.h"

#include <algorithm>
#include <cstring>
#include <format>
#include <string>
#include <string_view>

#include "ro/core/i18n.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/wheel_drag.h"

#include "imgui.h"

namespace ro::ui {
namespace {

// Le rayon : un pixel glissé vaut un dixième de case, un cran de molette une
// case.
constexpr WheelDrag kRadiusDrag{.drag_step = 0.1F,
                                .wheel_step = 1.0F,
                                .lowest = MapEditorCellBrush::kLeastRadiusCells,
                                .highest = MapEditorCellBrush::kGreatestRadiusCells,
                                .format = "%.1f"};

// LE PINCEAU : ce qu'il bloque, et son rayon. Un réglage n'est pas un geste du
// document : il ne compte pas dans l'historique.
void DrawCellBrush(const MapEditorView& view, std::vector<MapEditorIntent>& intents) {
  MapEditorCellBrush brush = view.cells.brush;
  bool changed = false;
  const auto offer = [&](MapEditorCellBlock block, const char* label, const char* tip) {
    if (ImGui::RadioButton(label, brush.block == block)) {
      brush.block = block;
      changed = true;
    }
    SkinTooltip(tip);
  };
  offer(MapEditorCellBlock::kWall, core::Tr("Mur"), core::Tr("Ni la marche ni les tirs."));
  ImGui::SameLine();
  offer(MapEditorCellBlock::kFence, core::Tr("Barrière"),
        core::Tr("La marche seulement : les tirs passent."));
  changed |= WheelDragFloat(core::Tr("Rayon (cases)"), brush.radius_cells, kRadiusDrag).changed;
  if (changed) {
    intents.emplace_back(map_editor::SetCellBrush{brush});
  }
}

// LA RAISON DES CASES : une pour toutes, qui devient une intention quand le
// champ se quitte.
void DrawCellsWhy(const MapEditorView& view, MapEditorCellsPanelState& panel,
                  std::vector<MapEditorIntent>& intents) {
  const std::string_view typed(panel.why.data(), strnlen(panel.why.data(), panel.why.size()));
  if (!panel.why_typing && typed != view.cells.why) {
    panel.why.fill('\0');
    const std::size_t kept = std::min(view.cells.why.size(), panel.why.size() - 1);
    std::copy_n(view.cells.why.begin(), kept, panel.why.begin());
  }
  static_cast<void>(ImGui::InputText(core::Tr("Pourquoi"), panel.why.data(), panel.why.size()));
  panel.why_typing = ImGui::IsItemActive();
  if (ImGui::IsItemDeactivatedAfterEdit()) {
    intents.emplace_back(map_editor::SetCellsWhy{
        std::string(panel.why.data(), strnlen(panel.why.data(), panel.why.size()))});
  }
}

}  // namespace

void DrawCells(const MapEditorView& view, MapEditorPanelState& panel,
               std::vector<MapEditorIntent>& intents, const SkinArt& art) {
  ImGui::SeparatorText(core::Tr("Cases"));
  // SES IDENTIFIANTS À PART : « Rayon » et « Pourquoi » nomment aussi des champs
  // du relief et du modèle désigné, dans la même fenêtre.
  ImGui::PushID("cells");
  ImGui::TextDisabled("%s", core::Tr("Glisser sur le sol bloque les cases ; Maj tenue, les "
                                     "libère. Le serveur les bloque aussi, pour tous. Ctrl+clic "
                                     "fait toujours marcher le personnage."));
  DrawCellBrush(view, intents);
  if (view.cells.pointer.has_value()) {
    ImGui::TextDisabled("%s", std::format("{} : {}, {}", core::Tr("Case sous la souris"),
                                          (*view.cells.pointer)[0], (*view.cells.pointer)[1])
                                  .c_str());
  }
  ImGui::BeginDisabled(!view.cells.model_selected);
  if (SkinButton(core::Tr("Bloquer l'emprise du modèle"), art)) {
    intents.emplace_back(map_editor::BlockModelFootprint{});
  }
  ImGui::EndDisabled();
  SkinTooltip(
      core::Tr("Bloque, avec la sorte choisie, chaque case dont le centre tombe sous la "
               "boîte du modèle désigné. Un point de départ, à retoucher au pinceau."));
  ImGui::TextDisabled(
      "%s", std::format("{} : {}", core::Tr("Cases bloquées"), view.cells.blocked).c_str());
  ImGui::BeginDisabled(view.cells.blocked == 0);
  DrawCellsWhy(view, panel.cells, intents);
  if (SkinButton(core::Tr("Libérer toutes les cases"), art)) {
    intents.emplace_back(map_editor::ClearCells{});
  }
  ImGui::EndDisabled();
  ImGui::PopID();
}

}  // namespace ro::ui
