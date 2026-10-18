#pragma once

#include <vector>

#include "ro/render/camera.h"
#include "ro/render/map_scene.h"
#include "ro/ui/map_editor_panel.h"

#include "world/map_editor_session.h"

namespace moonclient {

// LES SONS D'AMBIANCE DANS LA VUE DE L'ÉDITEUR, projetés à l'écran.
//
// Le repère de chaque son de la vue — sources du RSW là où l'édition les pose,
// puis sons ajoutés —, en pixels de fenêtre. Un son derrière l'œil n'en a pas.
[[nodiscard]] std::vector<MapEditorSession::SoundDot> SoundDots(const ro::ui::MapEditorView& view,
                                                                const ro::render::MapScene& scene,
                                                                const ro::render::Camera& camera,
                                                                float aspect_ratio,
                                                                float viewport_width,
                                                                float viewport_height);

// CE QUE LE SON DÉSIGNÉ OCCUPE : le cercle de sa portée, dans le plan
// horizontal qui passe par lui, et son emprise, le rectangle d'où son bruit
// part. Un segment dont un bout passe derrière l'œil ne se trace pas.
[[nodiscard]] std::vector<ro::ui::MapEditorLine> SoundReach(const ro::ui::MapEditorSound& sound,
                                                            const ro::render::MapScene& scene,
                                                            const ro::render::Camera& camera,
                                                            float aspect_ratio,
                                                            float viewport_width,
                                                            float viewport_height);

}  // namespace moonclient
