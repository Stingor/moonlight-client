#pragma once

#include <string>
#include <string_view>

#include <yaml-cpp/yaml.h>

#include "ro/mapedit/map_edit.h"

namespace ro::mapedit::detail {

// LA SECTION « blocked_cells » DU DOCUMENT : une raison, et des cases
// [colonne, rangée, sorte], la sorte valant « wall » ou « fence ». Voir
// docs/adr/0131.
//
// Lit la section de `whole` dans `edit`. Refuse une case négative, une sorte
// inconnue, une case qui revient, et la section dans le document des lumières
// de modèle. Rend le motif du refus, vide quand la section est bonne ou absente.
[[nodiscard]] std::string ReadBlockedCells(const YAML::Node& whole, std::string_view map_name,
                                           MapEdit& edit);

// Le texte de la section, sa clé comprise ; rien sans case bloquée.
[[nodiscard]] std::string BlockedCellsYaml(const MapEdit& edit);

}  // namespace ro::mapedit::detail
