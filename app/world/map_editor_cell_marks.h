#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

#include "ro/mapedit/map_edit.h"
#include "ro/render/camera.h"
#include "ro/ui/map_editor_panel.h"

#include "world/map_editor_session.h"
#include "world/map_stage.h"

namespace moonclient {

// L'OUTIL CASES DU MODE ÉDITEUR, à l'écran : voir docs/adr/0131 et
// map_editor_cells.cpp.

// La case de la grille de marche sous le point d'écran (x, y), en pixels de
// fenêtre ; rien hors du sol, hors de la grille, ou sur une carte sans grille.
// C'est le sol où l'on marche qui se vise : sur un pont, le tablier.
[[nodiscard]] std::optional<std::array<std::int32_t, 2>> PointedCell(
    const MapStage& stage, const ro::render::Camera& camera, float aspect_ratio, float screen_x,
    float screen_y, float viewport_width, float viewport_height);

// LES CASES TRACÉES AU SOL quand l'outil Cases est en main : autour du pinceau,
// ou de ce que regarde la caméra, celles que la carte bloque en gris, celles du
// document en rouge ou en orange, puis celles que le pinceau couvre. Rien sous
// un autre outil.
[[nodiscard]] std::vector<ro::ui::MapEditorLine> CellMarks(const MapStage& stage,
                                                           const ro::mapedit::MapEdit& document,
                                                           const MapEditorSession& session,
                                                           const ro::render::Camera& camera,
                                                           float aspect_ratio, float viewport_width,
                                                           float viewport_height);

}  // namespace moonclient
