#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_node_world.h"
#include "ro/render/effect_particle_quad.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE GRAIN ET SA TRACE : le dessinateur du sous-type 52, les feux d'artifice.
//
// Il sort par la voie A DEUX fois par piste vive : le grain à son ancre, puis
// le MÊME grain à la
// position qu'il occupait trois tics plus tôt, plus petit et deux fois moins
// opaque. C'est cette seconde image qui fait la traînée.
//
// ⚠ SES QUATRE COINS TOURNENT AVEC L'AZIMUT DE LA PISTE, et le mouvement
// l'avance de cinq degrés par tic : le carré tourne sur lui-même en tombant. Les
// poser aux quatre index fixes des autres figures le figerait.
//
// ⚠ ET SES COINS SONT ABSOLUS. Le client passe le drapeau qui EMPÊCHE d'ajouter
// la position du nœud : l'ancre d'une piste porte déjà un point du monde, et
// l'ajouter une seconde fois enverrait les grains au double de la distance.
//
// ⚠ LA MARQUE D'IMAGE CHANGE TOUT, ET ELLE NE DURE QU'UN TIC. Quand elle n'est
// pas nulle, le grain prend l'image qu'elle désigne, devient BLANC et perd les
// trois quarts de son alpha — c'est l'étincelle. Le mouvement ne la lève que
// trois tics sur vingt, et seulement dans une fenêtre de vingt tics propre à
// chaque piste. La lire comme un état durable ferait scintiller sans arrêt.

// COMBIEN DE FOIS UNE PISTE SORT, et de combien le second grain est plus petit.
inline constexpr std::size_t kSpiralTrailImagesPerTrack = 2;
inline constexpr float kSpiralTrailTrailShrink = 0.5F;

// DE COMBIEN L'ALPHA DE L'ÉTINCELLE EST DIVISÉ, en plus du rang.
inline constexpr std::int16_t kSpiralTrailSparkAlphaDivider = 4;

// LE DERNIER TIC DE LA MONTÉE INVISIBLE, où le grain s'allume enfin.
//
// ⚠ IL DIT COMBIEN DE TEMPS CETTE FIGURE RESTE INVISIBLE, et c'est pour cela
// qu'il est public : ajouté au retard du semis — de trois cents à trois cent
// cinquante tics —, il donne les quelque 455 tics qu'il faut à la gerbe avant
// de peindre son premier grain. Une vie de nœud plus courte la tue avant, et
// l'effet passe alors pour une figure cassée ; voir ADR 0042.
inline constexpr std::int32_t kLastTickOfSpiralTrailClimb = 154;

// LES CASES DE QUEUE OÙ LE MOUVEMENT RANGE LA POSITION D'IL Y A TROIS TICS.
//
// ⚠ LA PISTE EN GARDE TROIS, ET LE DESSINATEUR N'EN LIT QU'UNE. Le mouvement
// décale anchor vers la première, la première vers la deuxième, la deuxième vers
// la troisième ; seule la première paraît. Les deux autres ne sont pas mortes —
// elles sont le tampon qui rend la première juste.
inline constexpr std::size_t kTailOfSpiralTrailRecent = 3;

// LA MARQUE QUI CHOISIT LA TEINTE, et celle qui lève l'étincelle.
inline constexpr std::size_t kMarkOfSpiralTrailTint = 0;
inline constexpr std::size_t kMarkOfSpiralTrailSpark = 3;

// LES CINQ TEINTES QUE LE CLIENT NOMME, marque par marque.
//
// ⚠ LA CINQUIÈME EST LE CAS PAR DÉFAUT, et elle existe : à la différence du
// grain de pluie, ce dessinateur-ci peint TOUJOURS. Son semis tire sur cinq, et
// la dernière marque tombe dans la branche qui n'a pas de test.
[[nodiscard]] std::array<std::uint8_t, 3> EffectSpiralTrailTintFor(std::uint8_t mark);

// LES QUADS D'UNE PISTE : le grain puis sa trace, ou un lot VIDE.
//
// `turn_degrees` et `tilt_degrees` sont les deux angles de caméra que le
// mouvement a rangés dans les styles des deux premières pistes.
[[nodiscard]] std::vector<EffectWorldQuad> ComposeSpiralTrailQuads(
    const EffectTrack& track, std::int16_t turn_degrees, std::int16_t tilt_degrees,
    const EffectOwnerTint& owner = {});

}  // namespace ro::render
