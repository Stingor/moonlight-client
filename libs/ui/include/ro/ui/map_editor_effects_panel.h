#pragma once

#include <array>
#include <string>
#include <string_view>
#include <vector>

#include "ro/ui/map_editor_panel.h"
#include "ro/ui/skin_art.h"
#include "ro/ui/wheel_drag.h"

namespace ro::ui {

// La section « Effets » du panneau de l'éditeur de carte : voir
// map_editor_effects_panel.cpp.
// L'AIDE DE « Projette des ombres », sur une lumière comme sur une lueur.
[[nodiscard]] const char* ShadowHelp();

// LA PLUS FORTE COMPOSANTE de la couleur d'une lumière : quatre fois la pleine
// intensité. La session y ramène ce que l'on tape.
inline constexpr float kLargestLightColor = 4.0F;

// L'INTENSITÉ D'UNE COULEUR DE LUMIÈRE : sa plus forte composante. Un vaut la
// pleine intensité ; au-delà, elle éclaire plus fort.
[[nodiscard]] float LightIntensityOf(const std::array<float, 3>& color);

// `color` portée à l'intensité `intensity`, sa teinte gardée. Une couleur
// noire n'a plus de teinte : elle devient un blanc de cette intensité.
[[nodiscard]] std::array<float, 3> WithLightIntensity(const std::array<float, 3>& color,
                                                      float intensity);

// LA COULEUR D'UNE LUMIÈRE, sur une lumière comme sur une lueur : sa pastille
// et son sélecteur, puis son intensité, qui la renforce ou l'atténue sans
// toucher à sa teinte.
[[nodiscard]] WheelDragResult LightColorFields(std::array<float, 3>& color);

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

// LES EFFETS DU TYPE DU MODÈLE DÉSIGNÉ, sous lui : les assigner, et les régler
// tels qu'ils se lèvent sur cette pose. Voir docs/adr/0129.
void DrawModelEffects(const MapEditorView& view, MapEditorPanelState& panel,
                      std::vector<MapEditorIntent>& intents, const SkinArt& art);

}  // namespace ro::ui
