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
// ⚠ ET CES FONCTIONS-LÀ SONT PARTAGÉES, parce que plusieurs fenêtres et le
// registre des acteurs les emploient. Deux copies divergeraient au premier
// champ ajouté à la recette, et la moitié des styles arriverait tronquée sans
// que rien ne le dise.

[[nodiscard]] ro::render::PaletteRecipe FromWire(const ro::sim::StyleRecipe& told);
[[nodiscard]] ro::sim::StyleRecipe ToWire(const ro::render::PaletteRecipe& recipe);

// CE QUE LA RECETTE IMPOSE, posé sur l'apparence : ses trois numéros.
//
// ⚠ POUR CE QUI NE VIENT PAS DU MONDE : le pantin de l'éditeur, l'aperçu d'un
// code, la tête d'un membre de groupe ou de guilde. Moins un veut dire « rien
// d'imposé », et le champ du personnage reste alors en place. Un pantin doit
// montrer la coupe qu'on ESSAIE, sans revendiquer un choix qu'on n'a pas fait.
void Impose(const ro::render::PaletteRecipe& recipe, ro::render::ActorLook& look);
void Impose(const ro::sim::StyleRecipe& recipe, ro::render::ActorLook& look);

// LA SEULE TEINTE DE VÊTEMENT, pour un acteur du monde.
//
// 🔴 LA COIFFURE ET LA COULEUR DE CHEVEUX NE SE POSENT PAS SUR UN ACTEUR DU
// MONDE : le serveur les a déjà appliquées par son propre changement
// d'apparence, et le lot ne les porte que pour que l'écho rendu au
// propriétaire soit complet. Après un passage chez le styliste, celles du lot
// remettraient l'ancienne coupe.
void ImposeBodyPalette(const ro::sim::StyleRecipe& recipe, ro::render::ActorLook& look);

// ⚠ CHAMP PAR CHAMP : comparer des champs ne collisionne jamais.
[[nodiscard]] bool SameRecipe(const ro::render::PaletteRecipe& left,
                              const ro::render::PaletteRecipe& right);
// Les seuls réglages des rampes : c'est tout ce que la palette d'un style en
// tire, ses trois numéros agissant avant elle.
[[nodiscard]] bool SameRamps(const ro::render::PaletteRecipe& left,
                             const ro::render::PaletteRecipe& right);

}  // namespace moonclient
