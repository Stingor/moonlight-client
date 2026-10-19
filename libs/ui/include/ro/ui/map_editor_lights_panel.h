#pragma once

#include <vector>

#include "ro/ui/map_editor_panel.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// LES LUMIÈRES DU PANNEAU DE L'ÉDITEUR DE CARTE : voir
// map_editor_lights_panel.cpp.

// La section « Lumières » : la liste des lumières du document, l'ajout, et la
// lumière désignée — ses glissières, sa raison, son retrait.
void DrawLights(const MapEditorView& view, MapEditorPanelState& panel,
                std::vector<MapEditorIntent>& intents, const SkinArt& art);

// LES LUMIÈRES DU TYPE DU MODÈLE DÉSIGNÉ, sous lui : les assigner, les régler
// telles qu'elles luisent sur cette pose, et les éteindre sur elle seule. Voir
// docs/adr/0126.
void DrawModelLights(const MapEditorView& view, MapEditorPanelState& panel,
                     std::vector<MapEditorIntent>& intents, const SkinArt& art);

}  // namespace ro::ui
