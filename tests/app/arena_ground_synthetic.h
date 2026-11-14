#pragma once

#include <string_view>

#include "ro/sim/survivors_rules.h"

#include "support/temp_directory.h"
#include "survivors/arena_ground_look.h"

// UNE PRAIRIE SYNTHÉTIQUE pour peindre le sol généré hors écran : chaque pièce
// de la prairie livrée porte la même pyramide ; une herbe en damier fin, une
// herbe unie, une terre ; la lumière d'une carte d'essai. Aucun asset de
// l'installation : l'archive se bâtit en mémoire.
namespace moonclient::test_support {

// La carte d'essai dont la prairie prend la lumière.
inline constexpr std::string_view kSyntheticReferenceMap = "essai_ref";

// L'aspect de la prairie synthétique, sous la clé du terrain livré.
[[nodiscard]] const BiomeLook& SyntheticPrairieLook();

// Écrit dans `client` l'archive `essai.grf` qui porte la carte d'essai, la
// pyramide et les textures.
void FillSyntheticPrairie(const ro::test_support::TempDirectory& client);

// La règle du terrain de la première arène générée des règles livrées.
[[nodiscard]] const ro::sim::TerrainRule& ShippedPlains();

}  // namespace moonclient::test_support
