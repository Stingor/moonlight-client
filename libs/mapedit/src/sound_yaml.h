#pragma once

#include <string>

#include <yaml-cpp/yaml.h>

#include "ro/mapedit/map_edit.h"

namespace ro::mapedit::detail {

// LES SECTIONS « map_sounds » ET « sounds » DU DOCUMENT : les retouches des
// sources de son du RSW, et les sons ajoutés.
//
// Lit les deux sections de `whole` — absentes, rien — dans `edit`. Rend le
// motif du refus, vide quand elles sont bonnes.
[[nodiscard]] std::string ReadSounds(const YAML::Node& whole, MapEdit& edit);

// Le texte des deux sections, leurs clés comprises ; rien sans son.
[[nodiscard]] std::string SoundsYaml(const MapEdit& edit);

}  // namespace ro::mapedit::detail
