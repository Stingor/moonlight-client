#pragma once

#include <vector>

#include "ro/ui/map_editor_panel.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// LA SECTION « Sons » du panneau de l'éditeur de carte : les sources de son de
// la carte, les sons ajoutés, le choix d'un `.wav` de l'installation et le son
// désigné. Voir map_editor_sounds_panel.cpp.
void DrawSounds(const MapEditorView& view, MapEditorPanelState& panel,
                std::vector<MapEditorIntent>& intents, const SkinArt& art);

}  // namespace ro::ui
