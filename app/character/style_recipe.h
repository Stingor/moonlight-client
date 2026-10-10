#pragma once

#include "ro/render/palette_ramps.h"
#include "ro/render/sprite_catalog.h"
#include "ro/sim/style.h"

namespace moonclient {

// LA RECETTE D'UN STYLE, DE PART ET D'AUTRE DE LA FRONTIÈRE.
//
// ⚠ LE GRAPHE L'IMPOSE, ce n'est pas un goût : `render` ne dépend pas de
// `sim`, et `net` ne dépend pas de `render`. `sim::StyleRecipe` porte ce que
// le fil dit, `render::PaletteRecipe` ce que le calcul manipule, et `app`
// traduit dans les deux sens — comme il traduit déjà `sim::Actor` en
// `render::ActorLook`.
//
// ⚠ ET CES TROIS-LÀ SONT PARTAGÉS, parce que deux fenêtres les emploient :
// l'éditeur et l'aperçu d'un style reçu. Deux copies divergeraient au premier
// champ ajouté à la recette, et la moitié des styles arriverait tronquée sans
// que rien ne le dise.

[[nodiscard]] ro::render::PaletteRecipe FromWire(const ro::sim::StyleRecipe& told);
[[nodiscard]] ro::sim::StyleRecipe ToWire(const ro::render::PaletteRecipe& recipe);

// CE QUE LA RECETTE IMPOSE, posé sur l'apparence.
//
// ⚠ LA MÊME RÈGLE QUE POUR N'IMPORTE QUEL AUTRE ACTEUR — voir
// `world/actor_styles.h` : moins un veut dire « rien d'imposé », et le champ
// du personnage reste alors en place. Un pantin doit montrer la coupe qu'on
// ESSAIE, sans revendiquer un choix qu'on n'a pas fait.
void Impose(const ro::render::PaletteRecipe& recipe, ro::render::ActorLook& look);

// ⚠ CHAMP PAR CHAMP : comparer des champs ne collisionne jamais.
[[nodiscard]] bool SameRecipe(const ro::render::PaletteRecipe& left,
                              const ro::render::PaletteRecipe& right);

}  // namespace moonclient
