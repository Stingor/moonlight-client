#pragma once

#include <cstdint>

#include "ro/formats/image.h"

namespace ro::render {

// Les deux retouches d'une texture d'effet, appliquées au TÉLÉVERSEMENT.
//
// 🔴 C'EST LE SEUL ÉCART VOULU DU DÉPÔT AVEC LE CLIENT DE RÉFÉRENCE, et il est
// demandé : le client natif montre les deux mêmes défauts, et le joueur veut
// qu'on les corrige de notre côté. Voir `docs/adr/0049`.

// Ramène à ZÉRO le fond presque noir d'une texture d'effet.
//
// CE QUE ÇA CORRIGE. Le fond d'une texture d'effet n'est pas noir : il vaut UN
// À HUIT sur deux cent cinquante-cinq. Une couche seule ne le montre pas — un
// mélange ADDITIF de huit sur un fond sombre ne se voit pas. Mais un fichier
// d'effet en tient une vingtaine vivantes à la fois, chacune ajoutant son fond
// à quatre-vingts pour cent de teinte : StormGust.str en a QUARANTE ET UNE, et
// la somme lève tout son quad de vingt à quarante. Le quad a des bords droits,
// et la marche à ce bord se lit comme un LISERÉ.
//
// ⚠ ET LE BORD EST LE PIRE ENDROIT. Les trois textures d'éclair de Storm Gust
// — `shockwave_c`, `_d`, `_e` — sont encore lumineuses AU BORD MÊME de l'image,
// à gauche et à droite : leur quad tranche son contenu net, sans dégradé.
//
// POURQUOI HUIT, ET PAS UN NOMBRE CHOISI. C'est le QUANTUM DE CINQ BITS. L'art
// a été fait pour un affichage de seize bits, cinq bits par canal, où toute
// valeur sous huit VAUT ZÉRO : ce fond-là était noir pour qui l'a peint. À
// trente-deux bits il ne l'est plus, et c'est tout le défaut.
//
// La soustraction se fait par canal et l'alpha n'est pas touché. On SOUSTRAIT
// au lieu de seuiller pour ne pas poser de marche dans un dégradé : une
// texture qui monte doucement de zéro monterait sinon par un contour visible.
inline constexpr std::uint8_t kEffectBlackPedestal = 8;

[[nodiscard]] formats::Image WithoutBlackPedestal(const formats::Image& image);

// Recopie sur le TEXEL DE BORD celui d'à côté, vers l'intérieur.
//
// CE QUE ÇA CORRIGE. `plazma_a.bmp` et `plazma_b.bmp`, deux des onze textures
// de `StormGust.str`, portent un rectangle GRIS d'un texel tout autour de
// l'image — valeur 20 sur les côtés, 39 dans les coins, soit exactement le
// trait qui a été passé deux fois là où les deux traits se croisent. C'est un
// reste d'outil de dessin, étranger au contenu : celui-ci est bleuté, ce cadre
// ne l'est pas, et il ne suit en rien le texel voisin.
//
// Ces deux couches s'étalent sur un quad de 384 pixels avec des coordonnées de
// texture qui prennent l'image ENTIÈRE, `(0, 0)` à `(1, 1)` : le cadre est donc
// dessiné, agrandi d'une fois et demie, et c'est le rectangle clair d'un pixel
// que l'on voit autour de l'effet.
//
// POURQUOI UN TEXEL, SANS SEUIL ET SANS CONDITION. Le défaut fait exactement un
// texel, et un seuil demanderait un nombre que rien ne mesure. Recopier sans
// condition coûte, sur une texture SAINE, l'écart entre deux texels voisins
// d'un dégradé : mesuré sur les neuf autres textures de Storm Gust, il vaut de
// zéro à quatre sur 255, quand le cadre en vaut vingt.
//
// 🔴 LA COULEUR EST RECOPIÉE ; L'ALPHA NE PEUT QUE DESCENDRE. Le bord prend le
// plus transparent de son alpha et de celui de son voisin, et jamais plus.
//
// Les deux cas mesurés vont en sens contraires, et le minimum tient les deux :
//   · UN CADRE D'OUTIL DANS L'ALPHA : `dro_glow_sub_bottom_01.tga`, la lueur au
//     pied des piliers de butin, porte un texel de bord à 11..18 autour d'un
//     pourtour à ZÉRO — 123 texels sur 252. Sa couche se mélange
//     `SRCALPHA / INVSRCALPHA` : ce cadre sombre y peint un CARRÉ GRIS à arête
//     franche. Le minimum le ramène à zéro ;
//   · UNE GARDE À ZÉRO, qui est de la donnée : `alpha_down.tga` est blanche de
//     bout en bout et porte tout son dégradé dans l'alpha, avec un texel à ZÉRO
//     en haut et en bas, sous un voisin à 235. Y recopier l'alpha du voisin
//     peindrait un bandeau opaque au ras du sol, là où la jupe échantillonne
//     justement V = 1 : les colonnes de Safety Wall perdraient le liseré de
//     leur pied. Le minimum garde le zéro.
// Et les BMP de Storm Gust, dont l'alpha vaut 255 partout, n'y perdent rien.
//
// Une image de moins de trois texels de côté n'a pas d'intérieur d'où recopier,
// et sort telle quelle.
[[nodiscard]] formats::Image WithoutLeftoverFrame(const formats::Image& image);

}  // namespace ro::render
