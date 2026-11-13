#pragma once

#include <cstdint>

#include "ro/render/actor_effect_table.h"
#include "ro/render/effect_class.h"
#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/effect_table.h"

namespace moonclient {

// UN EFFET QUI NE FAIT QUE CHANGER SON PORTEUR : ni fichier, ni nœuds, ni
// classe, mais une entrée de la table des changements d'acteur. Le client le
// crée comme un autre, dans la liste d'effets de l'acteur, et c'est la mise à
// jour de cette instance qui, à chaque tic, lève son drapeau de rendu, teint
// le corps ou sème ses copies. Le jouer comme un dessin ne montrerait rien.
//
// ⚠ À PART DU RESTE DES CHANGEMENTS D'ACTEUR, qui tire le moteur d'effets
// entier : l'éditeur de style le demande aussi, pour dire qu'un tel effet de
// chapeau n'a rien à teindre.
[[nodiscard]] inline bool ChangesOnlyItsBearer(std::uint32_t effect_id) {
  return ro::render::ActorEffectChangeFor(effect_id) != nullptr &&
         ro::render::EffectFile(effect_id).empty() &&
         !ro::render::KnowsProceduralEffect(effect_id) &&
         !ro::render::IsPortedEffectClass(effect_id);
}

}  // namespace moonclient
