#pragma once

#include <vector>

#include "ro/ui/map_editor_panel.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// La section « Sprites » du panneau de l'éditeur de carte : la saisie d'un
// sprite de décor, la liste, et le sprite désigné. Voir
// map_editor_sprites_panel.cpp.
void DrawSprites(const MapEditorView& view, MapEditorPanelState& panel,
                 std::vector<MapEditorIntent>& intents, const SkinArt& art);

}  // namespace ro::ui
