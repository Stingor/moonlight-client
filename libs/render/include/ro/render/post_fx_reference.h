#pragma once

#include "ro/formats/image.h"
#include "ro/render/post_fx.h"

namespace ro::render {

// LES DEUX PASSES DU POST-TRAITEMENT, calculées par le processeur.
//
// ⚠ ELLES NE SERVENT PAS AU JEU. Ce sont les TÉMOINS du portage : les mêmes
// formules que les nuanceurs, instruction par instruction, avec un
// échantillonnage qui imite celui de la carte graphique — bilinéaire, texels
// centrés, bords bornés. Une image passée par la carte graphique se compare à
// la même image passée par ici, et un écart dit quel nuanceur a dérivé.
//
// ⚠ LE GRAIN NE SE COMPARE PAS : son hachage passe par un sinus, que chaque
// carte graphique calcule à sa précision. Une comparaison se fait grain éteint.
//
// Les deux rendent une image de la taille de la source, alpha plein.

// La passe de couleur : aberration, netteté, étalonnage, filtre, vignette,
// grain, dans cet ordre.
[[nodiscard]] formats::Image ColorPassReference(const formats::Image& source,
                                                const ColorPassConstants& constants);

// L'anticrénelage, mélangé à l'image par `strength`.
[[nodiscard]] formats::Image FxaaReference(const formats::Image& source, float strength);

}  // namespace ro::render
