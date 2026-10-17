#pragma once

#include <vector>

#include "ro/ui/map_editor_panel.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// La section « Effets » du panneau de l'éditeur de carte : voir
// map_editor_effects_panel.cpp.
void DrawEffects(const MapEditorView& view, MapEditorPanelState& panel,
                 std::vector<MapEditorIntent>& intents, const SkinArt& art);

}  // namespace ro::ui
