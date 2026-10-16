#pragma once

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ro/formats/rsm.h"
#include "ro/mapedit/map_edit.h"
#include "ro/render/camera.h"
#include "ro/render/map_scene.h"
#include "ro/vfs/vfs.h"

#include "world/map_editor_session.h"

namespace moonclient {

// LES FICHIERS DE MODÈLES QUE L'OUTIL SOMMETS A LUS, tels que l'installation
// les porte : un par fichier, lu à sa première désignation, et jamais retouché
// en place — l'édition s'applique à une copie.
class RsmCache {
 public:
  // Le modèle du fichier `raw_file`, nommé comme le RSW le cite ; rien quand il
  // ne se lit pas.
  [[nodiscard]] std::shared_ptr<const ro::formats::RsmModel> Get(ro::vfs::Vfs& vfs,
                                                                 const std::string& raw_file);

 private:
  std::map<std::string, std::shared_ptr<const ro::formats::RsmModel>> by_file_;
};

// CE QUE L'OUTIL SOMMETS SAIT DU MODÈLE DE RANG `rank` : ses nœuds, leurs
// sommets d'origine, et où l'édition `document` les met dans le monde. Rien
// pour un modèle qui ne se dessine pas, ou dont le fichier ne se lit pas.
[[nodiscard]] std::optional<MapEditorSession::VertexContext> BuildVertexContext(
    const ro::render::MapScene& scene, std::size_t rank, const ro::mapedit::MapEdit& document,
    RsmCache& cache, ro::vfs::Vfs& vfs);

// Les points de ses sommets à l'écran ; un sommet derrière l'œil n'en a pas.
[[nodiscard]] std::vector<MapEditorSession::VertexDot> VertexDots(
    const MapEditorSession::VertexContext& context, const ro::render::Camera& camera,
    float aspect_ratio, float viewport_width, float viewport_height);

}  // namespace moonclient
