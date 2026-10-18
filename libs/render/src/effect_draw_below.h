#pragma once

#include <cstdint>

#include "ro/render/effect_node.h"

// LE TIRAGE SOUS UNE BORNE des nœuds d'effet : le `rand() % borne` du client.
//
// En-tête PRIVÉ : il vit à côté des sources qui l'emploient et ne sort pas de
// `libs/render/src`. Rien de public n'en dépend.
//
// 🔴 AUCUNE COPIE LOCALE SOUS CE NOM. Une copie homonyme dans un espace anonyme
// masquerait celle-ci SANS ERREUR, et divergerait à la première correction. Une
// variante qui ne fait PAS la même chose — une borne nulle qui rend zéro sans
// tirer, un reste signé — porte un autre nom : `DrawBelowOrZero` ne se confond
// pas avec `DrawBelow`.

namespace ro::render {

// Un tirage entier dans [0, exclusive_bound), comme le modulo du client.
//
// ⚠ LA BORNE SE LIT SANS SIGNE, ET RIEN NE LA GARDE : elle doit être positive.
// Une borne nulle divise par zéro.
[[nodiscard]] inline std::int32_t DrawBelow(std::uint32_t& generator,
                                            std::int32_t exclusive_bound) {
  return static_cast<std::int32_t>(NextEffectNodeDraw(generator) %
                                   static_cast<std::uint32_t>(exclusive_bound));
}

// Un tirage sous la borne, ou zéro SANS TIRER quand elle n'est pas positive.
[[nodiscard]] inline std::int32_t DrawBelowIfPositive(std::uint32_t& generator,
                                                      std::int32_t exclusive_bound) {
  return exclusive_bound > 0 ? DrawBelow(generator, exclusive_bound) : 0;
}

}  // namespace ro::render
