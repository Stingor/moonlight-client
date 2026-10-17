#pragma once

#include <vector>

#include "ro/ui/map_editor_panel.h"
#include "ro/ui/skin_art.h"
#include "ro/ui/wheel_drag.h"

namespace ro::ui {

// La section « Effets » du panneau de l'éditeur de carte : voir
// map_editor_effects_panel.cpp.
// LA PLAGE D'HEURES d'un objet « la nuit seulement », sous sa case : une case
// qui la choisit, puis ses deux bornes. Vrai quand la case a basculé ; les
// bornes disent leur glissé dans `moved`.
[[nodiscard]] bool LitHoursFields(MapEditorHours& hours, const SkinArt& art,
                                  WheelDragResult& moved);

void DrawEffects(const MapEditorView& view, MapEditorPanelState& panel,
                 std::vector<MapEditorIntent>& intents, const SkinArt& art);

}  // namespace ro::ui
