#pragma once

namespace ro::core {

// LES OUTILS DE DÉVELOPPEMENT N'EXISTENT QUE DANS LA CONSTRUCTION DEBUG.
//
// Commandes de chat de mise au point, banc d'effets, options de lancement
// d'essai : rien de cela n'est livré au joueur. La valeur vient de CMake
// (`MOONCLIENT_DEV_TOOLS`, posée sur `ro_core`), selon la configuration.
//
// ⚠ ON LA TESTE PAR `if constexpr`, PAS PAR `#if`. Le code d'essai se compile
// alors dans les deux configurations, donc il ne peut pas se casser en
// silence là où on ne le bâtit pas ; en Release, il est simplement
// inatteignable, et l'éditeur de liens le retire.
inline constexpr bool kDevTools = MOONCLIENT_DEV_TOOLS != 0;

}  // namespace ro::core
