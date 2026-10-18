#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <vector>

#include <glm/glm.hpp>

#include "ro/formats/gnd.h"
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

// LE CENTRE DE LA BOÎTE du modèle `rsw_rank`, dans le repère du RSW ; rien
// pour un modèle qui ne se dessine pas.
[[nodiscard]] std::optional<std::array<float, 3>> SelectionCenter(const ro::render::MapScene& scene,
                                                                  std::size_t rsw_rank);

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

// Le point de chaque sprite de décor de la vue — ses pieds —, en pixels de
// fenêtre. Un sprite derrière l'œil n'en a pas.
[[nodiscard]] std::vector<MapEditorSession::LightDot> SpriteDots(const ro::ui::MapEditorView& view,
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

// LE POINT DE LA GRILLE DES COINS sous un point du monde : colonne et rangée,
// fractionnaires — voir `ro::mapedit::GroundEdit`.
[[nodiscard]] std::array<float, 2> CornerPointOf(const ro::formats::GndMap& gnd, float world_x,
                                                 float world_z);

// LE CERCLE DE LA BROSSE DE RELIEF : son rayon, en cubes, autour de
// `corner_point`, posé sur le sol, en segments projetés. Un segment dont un
// bout passe derrière l'œil ne se trace pas.
[[nodiscard]] std::vector<ro::ui::MapEditorLine> BrushRing(const ro::render::MapScene& scene,
                                                           const std::array<float, 2>& corner_point,
                                                           float radius_cubes,
                                                           const ro::render::Camera& camera,
                                                           float aspect_ratio, float viewport_width,
                                                           float viewport_height);

// LES POINTS DE LA GRILLE DES COINS du mode Sommets, à l'écran : ceux qui
// tombent dans `radius_cubes` autour de `corner_point`, plus ceux de `always`
// où qu'ils soient — la sélection ne disparaît pas quand la souris s'éloigne.
// Chacun se pose à la hauteur du sol dessiné ; un point derrière l'œil n'en a
// pas. `edited` dit ceux que `document_ground` retouche.
[[nodiscard]] std::vector<MapEditorSession::GroundDot> GroundPointDots(
    const ro::render::MapScene& scene, std::span<const ro::mapedit::GroundEdit> document_ground,
    const std::array<float, 2>& corner_point, float radius_cubes,
    std::span<const MapEditorSession::CornerPoint> always, const ro::render::Camera& camera,
    float aspect_ratio, float viewport_width, float viewport_height);

// Le point du monde du point (x, y) de la grille des coins, à la hauteur du
// sol dessiné ; rien hors de la grille.
[[nodiscard]] std::optional<glm::vec3> CornerPointWorld(const ro::formats::GndMap& gnd,
                                                        const MapEditorSession::CornerPoint& point);

}  // namespace moonclient
