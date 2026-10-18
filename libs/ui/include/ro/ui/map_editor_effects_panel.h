#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "ro/ui/map_editor_panel.h"
#include "ro/ui/skin_art.h"
#include "ro/ui/wheel_drag.h"

namespace ro::ui {

// La section « Effets » du panneau de l'éditeur de carte : voir
// map_editor_effects_panel.cpp.
// LA RAISON D'UNE RETOUCHE au bout de sa ligne de liste, « — raison » ; rien
// sans raison. Un « ## » y est rompu : ImGui y lirait la fin du libellé.
[[nodiscard]] std::string WhyTag(std::string_view why);

// LA PLAGE D'HEURES d'un objet « la nuit seulement », sous sa case : une case
// qui la choisit, puis ses deux bornes ; le clic droit sur elles copie ou colle
// la plage en texte, voir `HourRangeText`. Vrai quand la case a basculé ou
// qu'une plage a été collée ; les bornes disent leur glissé dans `moved`.
[[nodiscard]] bool LitHoursFields(MapEditorHours& hours, const SkinArt& art,
                                  WheelDragResult& moved);

void DrawEffects(const MapEditorView& view, MapEditorPanelState& panel,
                 std::vector<MapEditorIntent>& intents, const SkinArt& art);

}  // namespace ro::ui
