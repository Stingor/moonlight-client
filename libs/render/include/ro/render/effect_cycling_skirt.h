#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_node_world.h"
#include "ro/render/effect_track_figures.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LA JUPE DONT L'IMAGE DÉFILE : le dessinateur du sous-type 144.
//
// C'est la JUPE D'ANNEAU — `ComposeEffectTrackSkirt` — à UNE différence près :
// l'image du quad n'est pas fixe, elle est choisie par le compteur de tics de
// la piste, modulo le nombre d'images du nœud.
//
// ⚠ CE N'EST PAS UNE RESSEMBLANCE, C'EST LE MÊME DESSIN : les mêmes constantes
// — y compris les bornes de boucle 20, 21, 360 et 180, et les deux clés de
// soulèvement 6,001 et 24,000999 — et les mêmes champs, PLUS le nombre
// d'images. La seule différence est le modulo.
//
// ⚠ ET LE NOMBRE D'IMAGES EST CELUI DU NŒUD, non de la piste. Les quatre pistes
// d'un même nœud défilent donc dans le même jeu, chacune à son propre tic : le
// bâtisseur les échelonne par leurs alphas, pas par leurs images.

// LES QUADS D'UNE PISTE, image comprise, ou un lot VIDE.
//
// `texture_count` est le nombre d'images que le nœud porte. À zéro, aucune image
// ne peut être choisie et la figure rend un lot vide plutôt que de diviser par
// zéro — ce que le client ne fait jamais, son bâtisseur en chargeant toujours.
[[nodiscard]] std::vector<EffectWorldQuad> ComposeCyclingSkirtQuads(
    const EffectTrack& track, std::uint8_t variant, std::size_t texture_count, glm::vec3 anchor,
    const EffectRingOrientation& orientation = kEffectRingUpright);

}  // namespace ro::render
