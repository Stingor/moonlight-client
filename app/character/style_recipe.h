#pragma once

#include <optional>
#include <span>
#include <vector>

#include "ro/render/palette_ramps.h"
#include "ro/render/sprite_catalog.h"
#include "ro/render/style_set.h"
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

// LES TEINTURES DE CALQUES, dans les deux sens. Un calque que le rendu ne
// connaît pas ne se traduit pas : la teinture est ignorée, et la pièce garde
// ses couleurs d'origine.
[[nodiscard]] std::optional<ro::render::LayerDye> FromWire(const ro::sim::StyleLayerRecipe& told);
[[nodiscard]] ro::sim::StyleLayerRecipe ToWire(const ro::render::LayerDye& dye);

// LES TEINTURES QUE PORTE CETTE APPARENCE, parmi celles du joueur : celle de
// sa tête, de ses coiffes, de sa cape, et sa peau. Les autres restent au
// livre et reviendront avec la pièce.
//
// 🔴 TRIÉES PAR (CALQUE, CLÉ) : deux joueurs qui portent les mêmes teintures
// dans un autre ordre partagent ainsi leur style.
[[nodiscard]] std::vector<ro::render::LayerDye> WornDyes(
    std::span<const ro::sim::StyleLayerRecipe> told, const ro::render::ActorLook& look);

// ⚠ CHAMP PAR CHAMP : comparer des champs ne collisionne jamais.
[[nodiscard]] bool SameDye(const ro::render::LayerDye& left, const ro::render::LayerDye& right);
// Deux ensembles qui peignent pareil : la recette du corps par ses seuls
// réglages, et les mêmes teintures dans le même ordre. ⚠ UNE PALETTE TOUTE
// FAITE ne se compare pas : un ensemble qui en porte une n'est jamais le même.
[[nodiscard]] bool SameSet(const ro::render::StyleSet& left, const ro::render::StyleSet& right);

// ⚠ CHAMP PAR CHAMP : comparer des champs ne collisionne jamais.
[[nodiscard]] bool SameRecipe(const ro::render::PaletteRecipe& left,
                              const ro::render::PaletteRecipe& right);
// Les seuls réglages des rampes : c'est tout ce que la palette d'un style en
// tire, ses trois numéros agissant avant elle.
[[nodiscard]] bool SameRamps(const ro::render::PaletteRecipe& left,
                             const ro::render::PaletteRecipe& right);

}  // namespace moonclient
