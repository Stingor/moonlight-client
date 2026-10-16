#pragma once

#include <cstddef>
#include <vector>

#include "ro/render/camera.h"
#include "ro/render/map_scene.h"
#include "ro/ui/map_editor_panel.h"

namespace moonclient {

// CE QUE LE MODE ÉDITEUR TRACE PAR-DESSUS LA VUE, projeté à l'écran.
//
// Les douze arêtes de la boîte du modèle `rsw_rank`, en pixels de fenêtre ;
// rien pour un modèle qui ne se dessine pas. Une arête dont un bout passe
// derrière l'œil ne se trace pas.
[[nodiscard]] std::vector<ro::ui::MapEditorLine> SelectionOutline(
    const ro::render::MapScene& scene, std::size_t rsw_rank, const ro::render::Camera& camera,
    float aspect_ratio, float viewport_width, float viewport_height);

}  // namespace moonclient
