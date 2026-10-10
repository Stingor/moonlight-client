#pragma once

#include "ro/formats/image.h"

namespace ro::render {

// LES PIXELS D'UNE VIGNETTE DE CARTE, sans la carte graphique.
//
// Une illustration de carte fait 300 sur 400 : 480 Ko de texture chacune, soit
// plus de 400 Mo pour les neuf cents cartes de l'album. Elle se RÉDUIT donc à
// la taille EXACTE de sa pochette avant d'être téléversée — et elle se dessine
// alors texel pour pixel, nette, là où une réduction au dessin la rendrait
// floue.

// La taille qui fait entrer (width, height) dans (most_width, most_height) sans
// déformer ni jamais agrandir. Jamais moins d'un pixel.
struct ThumbnailSize {
  int width = 0;
  int height = 0;
};
[[nodiscard]] ThumbnailSize FitThumbnail(int width, int height, int most_width, int most_height);

// Réduit une image RGBA8 à la taille donnée, par moyenne de surface
// sur-échantillonnée (trois par trois points par texel).
//
// 🔴 LA COULEUR EST PONDÉRÉE PAR L'ALPHA : sans cela le magenta rendu
// transparent aux coins arrondis de la carte baverait dans sa bordure.
[[nodiscard]] formats::Image ShrinkImage(const formats::Image& source, ThumbnailSize size);

// La SILHOUETTE d'une pochette scellée : la luminance ramenée dans une bande
// sombre, assez sombre pour dire « pas à vous », assez contrastée pour qu'on
// reconnaisse le monstre.
//
// ⚠ C'EST UNE SECONDE IMAGE, et non une teinte au dessin : une teinte grise sur
// une image en couleur reste en couleur.
void MakeSilhouette(formats::Image& image);

}  // namespace ro::render
