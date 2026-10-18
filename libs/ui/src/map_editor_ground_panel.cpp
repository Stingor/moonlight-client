// LA SECTION « RELIEF » DU PANNEAU DE L'ÉDITEUR DE CARTE : la brosse — son
// mode, son rayon, sa force —, les points désignés du mode Sommets, ce que le
// document retouche, sa raison, et le retour au relief de la carte.

#include "ro/ui/map_editor_ground_panel.h"

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
                                .lowest = MapEditorBrush::kLeastRadiusCells,
                                .highest = MapEditorBrush::kGreatestRadiusCells,
                                .format = "%.1f"};
// La force : un pixel glissé vaut un dixième d'unité par seconde, un cran de
// molette une unité.
constexpr WheelDrag kStrengthDrag{.drag_step = 0.1F,
                                  .wheel_step = 1.0F,
                                  .lowest = MapEditorBrush::kLeastUnitsPerSecond,
                                  .highest = MapEditorBrush::kGreatestUnitsPerSecond,
                                  .format = "%.1f"};
// La hauteur d'un point : un pixel glissé vaut un dixième d'unité du monde, un
// cran de molette une unité, dix avec Majuscule.
constexpr WheelDrag kHeightDrag{.drag_step = 0.1F, .wheel_step = 1.0F, .format = "%.2f"};

// LA BROSSE : son mode, son rayon, sa force. Un réglage n'est pas un geste du
// document : il ne compte pas dans l'historique. En mode Sommets, le rayon
// borne les points montrés, et la force ne sert pas.
void DrawBrush(const MapEditorView& view, std::vector<MapEditorIntent>& intents) {
  MapEditorBrush brush = view.ground.brush;
  bool changed = false;
  const auto offer = [&](MapEditorBrushMode mode, const char* label) {
    if (ImGui::RadioButton(label, brush.mode == mode)) {
      brush.mode = mode;
      changed = true;
    }
  };
  offer(MapEditorBrushMode::kRaise, core::Tr("Lever"));
  ImGui::SameLine();
  offer(MapEditorBrushMode::kFlatten, core::Tr("Aplanir"));
  ImGui::SameLine();
  offer(MapEditorBrushMode::kSmooth, core::Tr("Adoucir"));
  ImGui::SameLine();
  offer(MapEditorBrushMode::kPoints, core::Tr("Sommets"));
  changed |= WheelDragFloat(core::Tr("Rayon (cases)"), brush.radius_cells, kRadiusDrag).changed;
  ImGui::BeginDisabled(brush.mode == MapEditorBrushMode::kPoints);
  changed |=
      WheelDragFloat(core::Tr("Force (unités par seconde)"), brush.units_per_second, kStrengthDrag)
          .changed;
  ImGui::EndDisabled();
  if (changed) {
    intents.emplace_back(map_editor::SetBrush{brush});
  }
}

// LE MODE SOMMETS : le point désigné — sa case et sa hauteur —, la taille de
// la sélection, et le retour de ses points au relief de la carte.
void DrawPoints(const MapEditorView& view, std::vector<MapEditorIntent>& intents,
                const SkinArt& art) {
  const MapEditorGround& ground = view.ground;
  if (ground.designated.has_value()) {
    const MapEditorGroundPoint& point = *ground.designated;
    const std::string edited =
        point.edited ? std::format(" — {}", core::Tr("retouché")) : std::string();
    ImGui::TextUnformatted(std::format("{} : {:.0f}, {:.0f}{}", core::Tr("Point désigné (cases)"),
                                       point.cells[0], point.cells[1], edited)
                               .c_str());
    float height = point.height;
    const WheelDragResult result = WheelDragFloat(core::Tr("Hauteur"), height, kHeightDrag);
    if (result.changed || result.released) {
      intents.emplace_back(
          map_editor::SetGroundPointHeight{.height = height, .released = result.released});
    }
  } else {
    ImGui::TextDisabled("%s", core::Tr("Aucun point désigné."));
  }
  ImGui::TextDisabled(
      "%s", std::format("{} : {} ({} {})", core::Tr("Points sélectionnés"), ground.selected_points,
                        ground.selected_edited, core::Tr("retouché(s)"))
                .c_str());
  ImGui::BeginDisabled(ground.selected_edited == 0);
  if (SkinButton(core::Tr("Rétablir les points"), art)) {
    intents.emplace_back(map_editor::RevertGroundPoints{});
  }
  ImGui::EndDisabled();
}

// LA RAISON DU RELIEF : une pour toutes ses retouches, qui devient une
// intention quand le champ se quitte.
void DrawGroundWhy(const MapEditorView& view, MapEditorPanelState& panel,
                   std::vector<MapEditorIntent>& intents) {
  const std::string_view typed(panel.ground_why.data(),
                               strnlen(panel.ground_why.data(), panel.ground_why.size()));
  if (!panel.ground_why_typing && typed != view.ground.why) {
    panel.ground_why.fill('\0');
    const std::size_t kept = std::min(view.ground.why.size(), panel.ground_why.size() - 1);
    std::copy_n(view.ground.why.begin(), kept, panel.ground_why.begin());
  }
  static_cast<void>(
      ImGui::InputText(core::Tr("Pourquoi"), panel.ground_why.data(), panel.ground_why.size()));
  panel.ground_why_typing = ImGui::IsItemActive();
  if (ImGui::IsItemDeactivatedAfterEdit()) {
    intents.emplace_back(map_editor::SetGroundWhy{std::string(
        panel.ground_why.data(), strnlen(panel.ground_why.data(), panel.ground_why.size()))});
  }
}

}  // namespace

void DrawGround(const MapEditorView& view, MapEditorPanelState& panel,
                std::vector<MapEditorIntent>& intents, const SkinArt& art) {
  ImGui::SeparatorText(core::Tr("Relief"));
  // SES IDENTIFIANTS À PART : « Sommets » nomme aussi l'outil des modèles, et
  // « Pourquoi » le champ du modèle désigné, dans la même fenêtre ; ImGui
  // confondrait les deux.
  ImGui::PushID("relief");
  const bool points = view.ground.brush.mode == MapEditorBrushMode::kPoints;
  ImGui::TextDisabled(
      "%s", points ? core::Tr("Un clic désigne un point du sol ; Maj+clic l'ajoute à la sélection "
                              "ou l'en retire ; Échap la vide. La poignée verte lève la sélection, "
                              "Maj l'aimante à l'unité ; Ctrl+flèches haut et bas la poussent. "
                              "Ctrl+clic fait toujours marcher le personnage.")
                   : core::Tr("Glisser sur le sol le lève ; Maj tenue, l'abaisse. Ctrl+clic fait "
                              "toujours marcher le personnage."));
  DrawBrush(view, intents);
  if (points) {
    DrawPoints(view, intents, art);
  }
  ImGui::TextDisabled(
      "%s",
      std::format("{} : {}", core::Tr("Points retouchés"), view.ground.edited_points).c_str());
  if (view.ground.remesh_ms.has_value()) {
    ImGui::TextDisabled(
        "%s",
        std::format("{} : {:.0f} ms — {}", core::Tr("Remaillage du sol"), *view.ground.remesh_ms,
                    view.ground.remesh_live ? core::Tr("en direct") : core::Tr("au relâcher"))
            .c_str());
  }
  ImGui::BeginDisabled(view.ground.edited_points == 0);
  DrawGroundWhy(view, panel, intents);
  if (SkinButton(core::Tr("Rétablir le relief"), art)) {
    intents.emplace_back(map_editor::RevertGround{});
  }
  ImGui::EndDisabled();
  ImGui::PopID();
}

}  // namespace ro::ui
