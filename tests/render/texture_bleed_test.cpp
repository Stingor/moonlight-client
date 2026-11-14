#include "ro/render/texture_bleed.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/image.h"
#include "ro/render/effect_texture_touchup.h"
#include "ro/render/gl_texture.h"
#include "ro/render/scene_cutout.h"

#include "support/image_builder.h"

using ro::formats::Image;
using ro::render::BleedTransparentEdges;
using ro::render::GlTexture;
using ro::render::HasTransparentTexel;
using ro::test_support::PixelAt;

namespace {

struct Rgba {
  std::uint8_t red = 0;
  std::uint8_t green = 0;
  std::uint8_t blue = 0;
  std::uint8_t alpha = 255;
};

// Construit une image à partir de pixels rangés de haut en bas.
Image MakeImage(std::int32_t width, std::int32_t height, const std::vector<Rgba>& pixels) {
  Image image;
  image.width = width;
  image.height = height;
  for (const Rgba& pixel : pixels) {
    image.pixels.insert(image.pixels.end(), {pixel.red, pixel.green, pixel.blue, pixel.alpha});
  }
  return image;
}

// Le fond magenta d'une texture RO, une fois passé par la clé chromatique :
// invisible, mais sa couleur est toujours là.
constexpr Rgba kKeyedOutMagenta{.red = 255, .green = 0, .blue = 255, .alpha = 0};

}  // namespace

TEST_CASE("Débordement : le magenta transparent perd sa couleur", "[render][bleed]") {
  // Le cas réel : une feuille verte opaque à côté d'un fond magenta déjà passé
  // à alpha 0. Sans ce traitement, le niveau de mipmap suivant moyenne le vert
  // avec du (255, 0, 255) et rosit la frondaison.
  const Image source = MakeImage(2, 1, {Rgba{.red = 0, .green = 200, .blue = 0}, kKeyedOutMagenta});

  const Image bled = BleedTransparentEdges(source);

  // Le pixel opaque n'est pas touché.
  CHECK(PixelAt(bled, 0, 0) == std::vector<std::uint8_t>{0, 200, 0, 255});
  // Le pixel transparent a pris la couleur du voisin, et RESTE transparent.
  CHECK(PixelAt(bled, 1, 0) == std::vector<std::uint8_t>{0, 200, 0, 0});
}

TEST_CASE("Débordement : la couleur atteint les pixels éloignés", "[render][bleed]") {
  // Un mipmap grossier mélange des texels très distants ; ne remplir que la
  // bordure immédiate laisserait la contamination réapparaître de loin —
  // exactement la distance à laquelle le défaut se voyait sur la carte.
  std::vector<Rgba> pixels{Rgba{.red = 10, .green = 20, .blue = 30}};
  pixels.insert(pixels.end(), 7, kKeyedOutMagenta);

  const Image bled = BleedTransparentEdges(MakeImage(8, 1, pixels));

  for (std::int32_t x = 0; x < 8; ++x) {
    CHECK(PixelAt(bled, x, 0)[0] == 10);
    CHECK(PixelAt(bled, x, 0)[1] == 20);
    CHECK(PixelAt(bled, x, 0)[2] == 30);
  }
  // Et le pixel le plus lointain est toujours invisible.
  CHECK(PixelAt(bled, 7, 0)[3] == 0);
}

TEST_CASE("Débordement : la couleur la plus PROCHE l'emporte", "[render][bleed]") {
  // Entre deux sources, le pixel vide prend celle qui le touche, sinon deux
  // objets de teintes différentes baveraient l'un sur l'autre.
  const Image source = MakeImage(3, 1, {Rgba{.red = 255}, Rgba{.alpha = 0}, Rgba{.blue = 255}});

  const Image bled = BleedTransparentEdges(source);
  const std::vector<std::uint8_t> middle = PixelAt(bled, 1, 0);
  // Les deux voisins sont à égale distance : le résultat doit être l'UN des
  // deux, jamais une moyenne violette qui n'existe dans aucun des deux objets.
  const bool took_a_neighbour =
      (middle[0] == 255 && middle[2] == 0) || (middle[0] == 0 && middle[2] == 255);
  CHECK(took_a_neighbour);
}

TEST_CASE("Débordement : une image opaque est rendue telle quelle", "[render][bleed]") {
  const Image source = MakeImage(
      2, 1, {Rgba{.red = 1, .green = 2, .blue = 3}, Rgba{.red = 4, .green = 5, .blue = 6}});
  const Image bled = BleedTransparentEdges(source);
  CHECK(bled.pixels == source.pixels);
}

TEST_CASE("Débordement : les cas dégénérés ne plantent pas", "[render][bleed]") {
  SECTION("image vide") {
    const Image bled = BleedTransparentEdges(Image{});
    CHECK(bled.pixels.empty());
  }
  SECTION("entièrement transparente : aucune couleur d'où partir") {
    const Image source = MakeImage(2, 1, {kKeyedOutMagenta, kKeyedOutMagenta});
    const Image bled = BleedTransparentEdges(source);
    CHECK(bled.pixels == source.pixels);
  }
  SECTION("tampon plus court que les dimensions annoncées") {
    Image source = MakeImage(1, 1, {Rgba{}});
    source.width = 4;
    source.height = 4;
    const Image bled = BleedTransparentEdges(source);
    CHECK(bled.pixels.size() == 4);
  }
}

TEST_CASE("Débordement : seul un texel d'alpha NUL demande le remplissage", "[render][bleed]") {
  // ⚠ UN ALPHA PARTIEL NE COMPTE PAS : le parcours ne repeint que les texels
  // d'alpha nul, et part de tous les autres.
  CHECK_FALSE(HasTransparentTexel(
      MakeImage(2, 1, {Rgba{.red = 1, .green = 2, .blue = 3}, Rgba{.red = 4, .alpha = 1}})));
  CHECK(HasTransparentTexel(MakeImage(2, 1, {Rgba{.red = 1}, kKeyedOutMagenta})));
  CHECK(HasTransparentTexel(MakeImage(1, 1, {kKeyedOutMagenta})));
  CHECK_FALSE(HasTransparentTexel(Image{}));
}

TEST_CASE("Téléversement : l'atlas et les BMP du sol et du décor gardent leurs bords tels quels",
          "[render][bleed]") {
  // 🔴 L'ART AU PIXEL AUSSI SE REMPLIT : ImGui le dessine par son propre
  // sampler, LINÉAIRE, qui écrase le « au plus proche » de la texture. Sans
  // remplissage, le magenta de la clé borderait les icônes et les cadres.
  CHECK(GlTexture::FillsTransparentEdges(GlTexture::Filtering::kSharp));
  CHECK(GlTexture::FillsTransparentEdges(GlTexture::Filtering::kSprite));
  CHECK(GlTexture::FillsTransparentEdges(GlTexture::Filtering::kPixelArt));
  CHECK_FALSE(GlTexture::FillsTransparentEdges(GlTexture::Filtering::kAtlas));
  // Le noir de leurs texels vides est voulu : c'est lui qui ombre le bord.
  CHECK_FALSE(GlTexture::FillsTransparentEdges(GlTexture::Filtering::kKeyedImage));
}

TEST_CASE("Découpe : une BMP se découpe à 207/255, le reste au plus près", "[render][bleed]") {
  using ro::formats::ImageFile;
  using ro::render::SceneCutoutOf;
  const auto bitmap = SceneCutoutOf(ImageFile::kBmp);
  CHECK(bitmap.filtering == GlTexture::Filtering::kKeyedImage);
  CHECK(bitmap.alpha_reference == ro::render::kBitmapCutoutAlphaReference);
  // Une TGA se mêle au fond chez le jeu : elle garde le remplissage de ses bords.
  const auto targa = SceneCutoutOf(ImageFile::kTga);
  CHECK(targa.filtering == GlTexture::Filtering::kSprite);
  CHECK(targa.alpha_reference == ro::render::kBlendedCutoutAlphaReference);
}

TEST_CASE("Découpe : le sol et le décor se bornent à leurs bords, comme le jeu",
          "[render][bleed]") {
  // 🔴 UNE PLANCHE DU SOL RÉPÉTÉE va chercher, au bord d'un carreau qui touche
  // le bord de sa planche, la colonne ou la rangée d'en face : un filet de
  // couleur à chaque carreau. Le jeu ne répète que ce qui glisse.
  using ro::formats::ImageFile;
  for (const ImageFile file : {ImageFile::kBmp, ImageFile::kTga, ImageFile::kOther}) {
    CHECK(ro::render::SceneCutoutOf(file).filtering != GlTexture::Filtering::kSharp);
  }
}

TEST_CASE("le PIÉDESTAL NOIR d'une texture d'effet tombe à zéro", "[render][texture]") {
  // 🔴 ÉCART VOULU AVEC LE CLIENT DE RÉFÉRENCE, et le seul du dépôt : le natif
  // montre le même liseré, et c'est la demande du joueur de le corriger chez
  // nous. Voir `docs/adr/0049`.
  //
  // Le fond d'une texture d'effet vaut UN À HUIT, jamais zéro. Une couche seule
  // ne le montre pas ; une vingtaine de couches ADDITIVES l'empilent et lèvent
  // tout leur quad, dont le bord droit se lit comme un liseré.
  Image source;
  source.width = 4;
  source.height = 1;
  source.pixels = {
      0,   0,   0,   255,  // déjà noir : il le reste
      1,   4,   8,   255,  // le fond mesuré : il tombe à zéro
      9,   12,  40,  200,  // juste au-dessus : il DESCEND, il ne s'annule pas
      255, 255, 255, 64,   // le contenu vif : intact à huit près
  };
  const Image without = ro::render::WithoutBlackPedestal(source);
  REQUIRE(without.pixels.size() == source.pixels.size());

  CHECK(without.pixels[0] == 0);
  CHECK(without.pixels[1] == 0);
  CHECK(without.pixels[2] == 0);
  // Tout le fond disparaît : c'est lui que vingt couches additionnaient.
  CHECK(without.pixels[4] == 0);
  CHECK(without.pixels[5] == 0);
  CHECK(without.pixels[6] == 0);
  // ⚠ ON SOUSTRAIT, ON NE SEUILLE PAS. Un seuil poserait une marche dans un
  // dégradé qui monte doucement de zéro — un contour visible là où l'art n'en
  // a pas. La soustraction décale la pente sans la couper.
  CHECK(without.pixels[8] == 1);
  CHECK(without.pixels[9] == 4);
  CHECK(without.pixels[10] == 32);
  CHECK(without.pixels[12] == 247);
  CHECK(without.pixels[13] == 247);
  CHECK(without.pixels[14] == 247);

  // ⚠ L'ALPHA N'EST PAS TOUCHÉ : un effet additif ne le lit que pour doser son
  // apport, et l'abaisser éteindrait la couche au lieu d'effacer son fond.
  CHECK(without.pixels[3] == 255);
  CHECK(without.pixels[7] == 255);
  CHECK(without.pixels[11] == 200);
  CHECK(without.pixels[15] == 64);
}

TEST_CASE("le piédestal se retire d'une image VIDE sans rien casser", "[render][texture]") {
  const Image empty_image;
  CHECK(ro::render::WithoutBlackPedestal(empty_image).pixels.empty());
}

TEST_CASE("le CADRE d'un texel prend la couleur d'à côté", "[render][texture]") {
  // 🔴 ÉCART VOULU AVEC LE CLIENT DE RÉFÉRENCE, le même que le piédestal :
  // `plazma_a.bmp` et `plazma_b.bmp` de `StormGust.str` portent un rectangle
  // gris d'un texel tout autour, à 20 sur les côtés et 39 dans les coins — le
  // trait de l'outil de dessin, passé deux fois là où les deux se croisent. Il
  // s'étale sur un quad de 384 pixels, coordonnées de texture (0,0) à (1,1).
  // Voir `docs/adr/0049`.
  const Rgba contenu{.red = 0, .green = 0, .blue = 60};
  const Rgba frame_color{.red = 20, .green = 20, .blue = 20};
  const Rgba corner{.red = 39, .green = 39, .blue = 39};
  const Image source = MakeImage(3, 3,
                                 {corner, frame_color, corner,        //
                                  frame_color, contenu, frame_color,  //
                                  corner, frame_color, corner});

  const Image without = ro::render::WithoutLeftoverFrame(source);

  // Il ne reste plus un seul texel du cadre : tout vient de l'intérieur.
  for (std::int32_t y = 0; y < 3; ++y) {
    for (std::int32_t x = 0; x < 3; ++x) {
      CHECK(PixelAt(without, x, y) == std::vector<std::uint8_t>{0, 0, 60, 255});
    }
  }
}

TEST_CASE("le cadre se lit dans la SOURCE, jamais dans la copie", "[render][texture]") {
  // Le coin prend le texel en DIAGONALE. Si on le lisait dans la copie en
  // cours, il prendrait un bord déjà réécrit et le résultat dépendrait de
  // l'ordre du parcours.
  const Image source = MakeImage(3, 3,
                                 {Rgba{.red = 200}, Rgba{.red = 200}, Rgba{.red = 200},  //
                                  Rgba{.red = 200}, Rgba{.green = 7}, Rgba{.red = 200},  //
                                  Rgba{.red = 200}, Rgba{.red = 200}, Rgba{.red = 200}});

  const Image without = ro::render::WithoutLeftoverFrame(source);

  CHECK(PixelAt(without, 0, 0) == std::vector<std::uint8_t>{0, 7, 0, 255});
  CHECK(PixelAt(without, 2, 2) == std::vector<std::uint8_t>{0, 7, 0, 255});
}

TEST_CASE("le cadre se retire SANS TOUCHER l'intérieur", "[render][texture]") {
  // Une texture saine ne doit rien perdre d'autre que l'écart entre deux texels
  // voisins de son bord : mesuré sur les neuf autres textures de Storm Gust, il
  // vaut de zéro à quatre sur 255, quand le cadre en vaut vingt.
  const Image source = MakeImage(4, 4,
                                 {
                                     Rgba{},
                                     Rgba{},
                                     Rgba{},
                                     Rgba{},  //
                                     Rgba{},
                                     Rgba{.red = 10},
                                     Rgba{.red = 20},
                                     Rgba{},  //
                                     Rgba{},
                                     Rgba{.red = 30},
                                     Rgba{.red = 40},
                                     Rgba{},  //
                                     Rgba{},
                                     Rgba{},
                                     Rgba{},
                                     Rgba{},  //
                                 });

  const Image without = ro::render::WithoutLeftoverFrame(source);

  // Les quatre texels d'intérieur sont intacts.
  CHECK(PixelAt(without, 1, 1)[0] == 10);
  CHECK(PixelAt(without, 2, 1)[0] == 20);
  CHECK(PixelAt(without, 1, 2)[0] == 30);
  CHECK(PixelAt(without, 2, 2)[0] == 40);
  // Et chaque bord a pris son voisin vers l'intérieur.
  CHECK(PixelAt(without, 1, 0)[0] == 10);
  CHECK(PixelAt(without, 3, 2)[0] == 40);
  CHECK(PixelAt(without, 0, 0)[0] == 10);
  CHECK(PixelAt(without, 3, 3)[0] == 40);
}

TEST_CASE("une image trop PETITE pour avoir un intérieur sort telle quelle", "[render][texture]") {
  // Deux texels de côté : les quatre sont des bords, il n'y a rien à recopier.
  const Image source =
      MakeImage(2, 2, {Rgba{.red = 1}, Rgba{.red = 2}, Rgba{.red = 3}, Rgba{.red = 4}});
  CHECK(ro::render::WithoutLeftoverFrame(source).pixels == source.pixels);
  CHECK(ro::render::WithoutLeftoverFrame(Image{}).pixels.empty());
}

TEST_CASE("le cadre n'élève JAMAIS l'alpha d'un bord à zéro", "[render][texture]") {
  // 🔴 UN BORD À ALPHA NUL EST DE LA DONNÉE, PAS UN ARTEFACT D'OUTIL.
  // `alpha_down.tga` est blanche de bout en bout et porte TOUT son dégradé dans
  // l'alpha, avec un texel de garde à ZÉRO en haut et en bas — mesuré dans
  // l'archive : lignes 0 et 63 à zéro, ligne 62 à 235. La jupe d'anneau
  // échantillonne justement V = 1 sur cette dernière ligne, au ras du sol :
  // y recopier l'alpha du voisin y peignait un bandeau opaque, et les colonnes
  // de Safety Wall perdaient le liseré de leur pied dans une base saturée.
  //
  // L'alpha d'un bord ne peut que descendre. Voir `docs/adr/0049`.
  const Rgba garde{.red = 200, .green = 200, .blue = 200, .alpha = 0};
  const Rgba full{.red = 200, .green = 200, .blue = 200, .alpha = 235};
  const Image source = MakeImage(3, 3,
                                 {garde, garde, garde,  //
                                  garde, full, garde,   //
                                  garde, garde, garde});

  const Image without = ro::render::WithoutLeftoverFrame(source);

  // Le bord garde son alpha NUL, sur les quatre côtés comme dans les coins.
  for (std::int32_t y = 0; y < 3; ++y) {
    for (std::int32_t x = 0; x < 3; ++x) {
      if (x == 1 && y == 1) {
        continue;
      }
      INFO("texel " << x << ", " << y);
      CHECK(PixelAt(without, x, y)[3] == 0);
    }
  }
  // ⚠ ET LA COULEUR, ELLE, EST BIEN RECOPIÉE : la retouche fait toujours son
  // travail, elle ne le fait plus dans le mauvais canal.
  const Image grey_frame =
      MakeImage(3, 3,
                {Rgba{.red = 39, .green = 39, .blue = 39}, Rgba{.red = 20, .green = 20, .blue = 20},
                 Rgba{.red = 39, .green = 39, .blue = 39}, Rgba{.red = 20, .green = 20, .blue = 20},
                 Rgba{.blue = 60}, Rgba{.red = 20, .green = 20, .blue = 20},
                 Rgba{.red = 39, .green = 39, .blue = 39}, Rgba{.red = 20, .green = 20, .blue = 20},
                 Rgba{.red = 39, .green = 39, .blue = 39}});
  const Image nettoye = ro::render::WithoutLeftoverFrame(grey_frame);
  CHECK(PixelAt(nettoye, 0, 0) == std::vector<std::uint8_t>{0, 0, 60, 255});
  CHECK(PixelAt(nettoye, 1, 0) == std::vector<std::uint8_t>{0, 0, 60, 255});
}

TEST_CASE("un cadre d'outil dans l'ALPHA tombe à zéro", "[render][texture]") {
  // 🔴 LE CARRÉ GRIS DES PILIERS DE BUTIN. `dro_glow_sub_bottom_01.tga`, la lueur
  // au pied des cinq piliers, porte un texel de bord à alpha 11..18, couleur 13
  // ou 26, autour d'un pourtour à ZÉRO — mesuré dans l'archive : 123 texels de
  // bord sur 252. Sa couche du `.str` se mélange `SRCALPHA / INVSRCALPHA` : ce
  // cadre sombre y dessinait un carré à arête franche, que le sol blanc montre.
  const Rgba frame_color{.red = 13, .green = 13, .blue = 13, .alpha = 11};
  const Rgba corner{.red = 26, .green = 26, .blue = 26, .alpha = 18};
  const Rgba empty_image{.red = 0, .green = 0, .blue = 0, .alpha = 0};
  const Image source = MakeImage(3, 3,
                                 {corner, frame_color, corner,            //
                                  frame_color, empty_image, frame_color,  //
                                  corner, frame_color, corner});

  const Image without = ro::render::WithoutLeftoverFrame(source);

  for (std::int32_t y = 0; y < 3; ++y) {
    for (std::int32_t x = 0; x < 3; ++x) {
      INFO("texel " << x << ", " << y);
      CHECK(PixelAt(without, x, y)[3] == 0);
    }
  }
}

TEST_CASE("un bord MOINS opaque que son voisin garde son alpha", "[render][texture]") {
  // Le minimum, dans l'autre sens : un dégradé qui s'éteint vers le bord n'est
  // pas un cadre, et le bord ne prend pas l'opacité de l'intérieur.
  const Rgba edge{.red = 90, .green = 90, .blue = 90, .alpha = 40};
  const Rgba dedans{.red = 90, .green = 90, .blue = 90, .alpha = 200};
  const Image source = MakeImage(3, 3,
                                 {edge, edge, edge,    //
                                  edge, dedans, edge,  //
                                  edge, edge, edge});

  const Image without = ro::render::WithoutLeftoverFrame(source);

  CHECK(PixelAt(without, 0, 1)[3] == 40);
  CHECK(PixelAt(without, 2, 2)[3] == 40);
  CHECK(PixelAt(without, 1, 1)[3] == 200);
}
