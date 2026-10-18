#pragma once

#include <array>
#include <string>
#include <string_view>

#include <yaml-cpp/yaml.h>

#include "ro/mapedit/map_edit.h"

namespace ro::mapedit::detail {

// LA SECTION « ground » DU DOCUMENT : une raison, et des points
// [colonne, rangée, levée] de la grille des coins.
//
// Lit `ground` dans `edit`, deux points au même endroit additionnant leurs
// levées. Rend le motif du refus, vide quand la section est bonne.
[[nodiscard]] std::string ReadGround(const YAML::Node& ground, MapEdit& edit);

// Le texte de la section, sa clé comprise ; rien sans retouche du relief.
[[nodiscard]] std::string GroundYaml(const MapEdit& edit);

// Une chaîne entre guillemets, ses guillemets, barres obliques inverses et
// sauts de ligne échappés : l'écriture de toute raison du document.
[[nodiscard]] std::string DoubleQuoted(std::string_view text);

// Une chaîne entre apostrophes : rien ne s'y échappe, sauf l'apostrophe, qui
// se double. C'est ce qui garde lisibles les barres obliques inverses d'un
// chemin.
[[nodiscard]] std::string SingleQuoted(std::string_view text);

// Un vecteur, chaque composante à son écriture la plus courte qui se relit à
// l'identique.
[[nodiscard]] std::string VectorText(const std::array<float, 3>& vector);

}  // namespace ro::mapedit::detail
