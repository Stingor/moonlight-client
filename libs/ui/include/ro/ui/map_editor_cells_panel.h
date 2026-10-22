#pragma once

#include <vector>

#include "ro/ui/map_editor_panel.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// La section « Cases » du panneau de l'éditeur de carte, quand l'outil Cases
// est en main : voir map_editor_cells_panel.cpp.
void DrawCells(const MapEditorView& view, MapEditorPanelState& panel,
               std::vector<MapEditorIntent>& intents, const SkinArt& art);

}  // namespace ro::ui
