#pragma once

#include "ro/formats/image.h"

namespace ro::render {

// Étale la couleur des pixels opaques sur les pixels transparents voisins.
//
// À QUOI ÇA SERT. Une texture RO marque ses parties invisibles en magenta pur,
// que la clé chromatique passe à alpha 0 — mais SANS toucher au rouge et au
// bleu, qui restent à 255. Tant que l'image est vue à sa taille native, le
// nuanceur rejette ces pixels et personne ne voit le magenta.
//
// Le filtrage change tout : dès que l'image n'est pas vue à sa taille exacte,
// chaque pixel est la moyenne de plusieurs texels. Un texel de feuille moyenné
// avec un texel magenta donne un alpha intermédiaire — au-dessus du seuil de
// rejet, donc affiché — et une couleur tirée vers le magenta. C'est ce qui a
// peint en rose toutes les frondaisons de prontera alors que le décodeur et la
// clé étaient corrects.
//
// La parade est de remplir les zones transparentes avec la couleur opaque la
// plus proche AVANT de téléverser. L'alpha n'est pas modifié : les pixels
// restent invisibles, mais la moyenne n'a plus de magenta à tirer. C'est un
// correctif de RENDU, pas de format — le fichier, lui, est fidèlement décodé.
[[nodiscard]] formats::Image BleedTransparentEdges(const formats::Image& image);

// L'image a-t-elle au moins un texel d'alpha NUL ? Sans lui, il n'y a rien à
// remplir, et `BleedTransparentEdges` la rendrait telle quelle.
//
// ⚠ C'EST LE CAS ORDINAIRE : de 65 à 76 % des textures qu'une carte téléverse
// n'ont aucun texel transparent — tout le sol, l'eau, et une bonne part du
// décor. Le parcours y coûterait autant qu'ailleurs, pour ne rien changer.
[[nodiscard]] bool HasTransparentTexel(const formats::Image& image);

}  // namespace ro::render
