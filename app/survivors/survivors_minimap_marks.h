#pragma once

#include <vector>

#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/ui/survivors_minimap.h"

namespace moonclient {

// LES SYMBOLES DE LA MINIMAP pour l'état `field` : les plantes, les objets au
// sol (or, viande, manuels…), les cartes, les coffres, les gemmes d'Emperium,
// les mini-boss, puis les boss, dans cet ordre — les derniers se peignent
// par-dessus. Chacun en écart au héros, en cases continues, l'est vers la
// droite et le nord vers le haut. Les monstres communs et les gemmes sous
// l'Emperium n'en ont pas ; un rang se lit dans `rules`.
void SurvivorsMinimapDotsOf(const ro::sim::SurvivorsField& field,
                            const ro::sim::SurvivorsRules& rules,
                            std::vector<ro::ui::SurvivorsMinimapDot>& into);

}  // namespace moonclient
