// LES EFFETS QUE `/effect` NE COUPE PAS — la liste blanche du client.
//
// ⚠ Fichier ENGENDRÉ par scripts/gen_client_effect_visibility.py depuis le relevé
// scripts/client_effect_forced_visible.json, pas écrit à la main.
//
// Un effet de cette liste reste affiché même quand le joueur coupe les
// effets.
//
// La liste elle-même est un fichier de DONNÉES du client — data\luafiles514\lua
// files\effecttool\forcerendereffect.lub — et non une constante de l'exécutable.
//
// 114 identifiants, de 35 à 2402.

#include <algorithm>

#include "ro/render/effect_visibility.h"

namespace ro::render {
namespace {

// TRIÉE, et la recherche en dépend.
constexpr std::array<std::uint32_t, 114> kForcedVisibleEffects = {{
    35,   36,   60,   114,  137,  138,  154,  155,  159,  160,  164,  166,  203,  228,  261,
    295,  296,  315,  316,  317,  321,  332,  334,  338,  357,  371,  419,  424,  426,  445,
    446,  449,  475,  476,  477,  478,  479,  480,  481,  482,  483,  484,  542,  544,  548,
    549,  582,  629,  655,  656,  718,  735,  736,  737,  738,  758,  759,  760,  761,  762,
    763,  764,  765,  766,  767,  788,  833,  837,  1072, 1110, 1135, 1186, 1187, 1188, 1189,
    1190, 1191, 1214, 1215, 1216, 1217, 1225, 1262, 1263, 1264, 1265, 1607, 1608, 1609, 1617,
    1618, 1619, 1869, 1870, 1871, 2371, 2372, 2379, 2380, 2381, 2383, 2384, 2385, 2386, 2387,
    2388, 2389, 2395, 2396, 2397, 2398, 2399, 2401, 2402,
}};

}  // namespace

std::span<const std::uint32_t> ForcedVisibleEffects() { return kForcedVisibleEffects; }

bool IsForcedVisibleEffect(std::uint32_t effect_id) {
  return std::ranges::binary_search(kForcedVisibleEffects, effect_id);
}

}  // namespace ro::render
