#pragma once

#include "ro/formats/image.h"
#include "ro/render/gl_texture.h"

namespace ro::render {

// LE SEUIL DE DÉCOUPE d'une BMP du sol ou du décor : un pixel dont l'alpha
// filtré reste sous 207/255 se rejette.
//
// Le jeu ne mêle pas ces textures au fond : il les TESTE, et il teste haut. Un
// texel vide voisin suffit à faire tomber le bord, si bien que la découpe mord
// d'un tiers de texel dans l'opaque. C'est ce qui garde fin le liseré sombre
// peint autour des feuillages et des statues : un seuil bas le montrait en
// entier, et même épaissi.
inline constexpr float kBitmapCutoutAlphaReference = 207.0F / 255.0F;

// Le seuil d'une texture que le jeu MÊLE au fond au lieu de la découper — une
// TGA. Sans ce mélange, elle se découpe au plus près de son alpha.
inline constexpr float kBlendedCutoutAlphaReference = 0.1F;

// Comment une texture du sol ou du décor se téléverse et se découpe.
//
// ⚠ BORNÉE À SES BORDS DANS TOUS LES CAS, sol compris : le jeu n'échantillonne
// en répétition que les textures qu'un nœud anime — voir `ModelRenderer`. Une
// planche du sol répétée irait chercher, au bord d'un carreau, la colonne ou la
// rangée d'en face de la planche, et le sol se couvrirait d'un filet de
// couleur à chaque carreau qui touche le bord de sa planche.
struct SceneCutout {
  GlTexture::Filtering filtering = GlTexture::Filtering::kSprite;
  float alpha_reference = kBlendedCutoutAlphaReference;
};

// Selon le FICHIER, comme le jeu : une BMP garde ses texels vides noirs et se
// découpe haut ; le reste garde le remplissage de ses bords et un seuil bas.
[[nodiscard]] constexpr SceneCutout SceneCutoutOf(formats::ImageFile file) noexcept {
  if (file == formats::ImageFile::kBmp) {
    return SceneCutout{.filtering = GlTexture::Filtering::kKeyedImage,
                       .alpha_reference = kBitmapCutoutAlphaReference};
  }
  return SceneCutout{};
}

}  // namespace ro::render
