#pragma once

#include <cstddef>
#include <vector>

#include "ro/render/camera.h"
#include "ro/render/map_scene.h"
#include "ro/ui/map_editor_panel.h"

#include "world/map_editor_session.h"

namespace moonclient {

// CE QUE LE MODE ÉDITEUR TRACE PAR-DESSUS LA VUE, projeté à l'écran.
//
// Les douze arêtes de la boîte du modèle `rsw_rank`, en pixels de fenêtre ;
// rien pour un modèle qui ne se dessine pas. Une arête dont un bout passe
// derrière l'œil ne se trace pas.
[[nodiscard]] std::vector<ro::ui::MapEditorLine> SelectionOutline(
    const ro::render::MapScene& scene, std::size_t rsw_rank, const ro::render::Camera& camera,
    float aspect_ratio, float viewport_width, float viewport_height);

// Le point de chaque effet de la carte — un objet d'effet du RSW, là où
// l'édition le pose —, en pixels de fenêtre. Un effet derrière l'œil n'en a
// pas.
[[nodiscard]] std::vector<MapEditorSession::LightDot> MapEffectDots(
    const ro::ui::MapEditorView& view, const ro::render::MapScene& scene,
    const ro::render::Camera& camera, float aspect_ratio, float viewport_width,
    float viewport_height);

// Le point de chaque effet de carte de la vue, en pixels de fenêtre. Un effet
// derrière l'œil n'en a pas.
[[nodiscard]] std::vector<MapEditorSession::LightDot> EffectDots(const ro::ui::MapEditorView& view,
                                                                 const ro::render::MapScene& scene,
                                                                 const ro::render::Camera& camera,
                                                                 float aspect_ratio,
                                                                 float viewport_width,
                                                                 float viewport_height);

// Le point de chaque lumière ponctuelle de la vue, en pixels de fenêtre. Une
// lumière derrière l'œil n'en a pas.
[[nodiscard]] std::vector<MapEditorSession::LightDot> LightDots(const ro::ui::MapEditorView& view,
                                                                const ro::render::MapScene& scene,
                                                                const ro::render::Camera& camera,
                                                                float aspect_ratio,
                                                                float viewport_width,
                                                                float viewport_height);

// LE CERCLE DE PORTÉE d'une lumière : où elle s'éteint, dans le plan
// horizontal qui passe par elle, en segments projetés. Un segment dont un bout
// passe derrière l'œil ne se trace pas.
[[nodiscard]] std::vector<ro::ui::MapEditorLine> LightRangeRing(const ro::ui::MapEditorLight& light,
                                                                const ro::render::MapScene& scene,
                                                                const ro::render::Camera& camera,
                                                                float aspect_ratio,
                                                                float viewport_width,
                                                                float viewport_height);

}  // namespace moonclient
