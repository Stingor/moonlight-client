#pragma once

#include <vector>

#include "ro/ui/map_editor_panel.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// La section « Relief » du panneau de l'éditeur de carte, quand l'outil Relief
// est en main : voir map_editor_ground_panel.cpp.
void DrawGround(const MapEditorView& view, MapEditorPanelState& panel,
                std::vector<MapEditorIntent>& intents, const SkinArt& art);

}  // namespace ro::ui
