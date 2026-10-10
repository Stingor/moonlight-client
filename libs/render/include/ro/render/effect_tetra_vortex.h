#pragma once

#include <array>
#include <cstdint>

#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE TOURBILLON DE TETRA VORTEX : le sous-type 140, les effets 804 et 970 à 973.
//
// ⚠ AUCUNE FIGURE NEUVE : le dessin envoie le cas 140 au même dessinateur que
// le sous-type 82 (l'arc de ruban teinté, qui sert les cas 82, 95, 128 et 140),
// et `ComposeArcRibbonQuads` le porte déjà. Ce qui est neuf, c'est le
// MOUVEMENT : le répartiteur envoie le cas 140 au mouvement d'animation des
// quatre particules en panneau, et non à celui de l'arc de ruban.
//
// Ce mouvement ne déplace rien. Il fait DÉFILER huit images — `tv-1.bmp` à
// `tv-8.bmp` — une tous les trois tics, puis, arrivé à la dernière, pâlit la
// piste de quinze tous les trois tics jusqu'à l'éteindre. Le tourbillon est donc
// une courte animation posée au point visé, et non une figure qui tourne.

// DE COMBIEN DE TICS une image se tient : `compteur % 3`.
inline constexpr std::int32_t kTetraVortexTicksPerFrame = 3;

// DE COMBIEN L'ALPHA TOMBE à chaque cran passé sur la dernière image.
inline constexpr std::int16_t kTetraVortexFadePerStep = 15;

// LE MOUVEMENT DU SOUS-TYPE 140, d'un tic.
//
// ⚠ LES DEUX ANGLES DE PANNEAU SE RANGENT D'ABORD, hors de la boucle et sur les
// deux premières pistes même éteintes : c'est là que la figure les relit pour
// tourner les coins de ses quads. Ils arrivent ici déjà complétés, et se
// rangent tels quels.
//
// Puis, pour chaque piste ALLUMÉE :
//   - son compteur avance d'un ;
//   - tous les trois tics, l'image avance d'un cran ; au cran qui dépasse la
//     dernière — `frame_count`, que le nœud porte —, elle y revient et l'alpha
//     perd quinze ;
//   - l'alpha est enfin borné À CHAQUE TIC entre zéro et 255. La borne basse
//     arrête l'alpha à zéro au quatorzième cran, qui le porterait sinon à moins
//     dix.
//
// ⚠ LE CRAN D'IMAGE EST UN OCTET SIGNÉ, comparé à un
// entier : sans conséquence tant que le nœud a moins de cent vingt-huit images.
void AdvanceTetraVortexTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                              std::int16_t billboard_turn_degrees,
                              std::int16_t billboard_tilt_degrees, std::int32_t frame_count);

}  // namespace ro::render
