#pragma once

#include <string>

#include <yaml-cpp/yaml.h>

#include "ro/mapedit/map_edit.h"

namespace ro::mapedit::detail {

// LA SECTION « sprites » DU DOCUMENT : les sprites de décor, voir
// ro/mapedit/map_sprite.h.
//
// Lit `sprites` dans `edit`. Rend le motif du refus, vide quand la section est
// bonne.
[[nodiscard]] std::string ReadSprites(const YAML::Node& sprites, MapEdit& edit);

// Le texte de la section, sa clé comprise ; rien sans sprite.
[[nodiscard]] std::string SpritesYaml(const MapEdit& edit);

}  // namespace ro::mapedit::detail
