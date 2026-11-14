#include "ro/render/lightmap_atlas.h"

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/gnd.h"

#include "support/map_builder.h"

using ro::formats::GndMap;
using ro::render::BuildLightmapAtlas;
using ro::render::LightmapAtlas;
using ro::test_support::BuildGnd;
using ro::test_support::GndBuildOptions;

namespace {

LightmapAtlas AtlasOf(const GndBuildOptions& options) {
  auto gnd = GndMap::Parse(BuildGnd(options));
  REQUIRE(gnd.has_value());
  return BuildLightmapAtlas(*gnd);
}

}  // namespace

TEST_CASE("Atlas : sans lightmap, il reste vide", "[render][lightmap]") {
  GndBuildOptions options;
  options.lightmap_count = 0;

  const LightmapAtlas atlas = AtlasOf(options);
  CHECK(atlas.empty());
  // Un index hors table ne doit pas sortir du tableau : un GND peut nommer une
  // lightmap qu'il ne porte pas.
  CHECK(atlas.MinimumOf(0) == std::array<float, 2>{0.0F, 0.0F});
  CHECK(atlas.MaximumOf(99) == std::array<float, 2>{0.0F, 0.0F});
}

TEST_CASE("Atlas : le côté vaut deux fois la plus grande dimension", "[render][lightmap]") {
  // La plus petite grille carrée qui range toutes les lightmaps : dix en
  // demandent quatre de côté, soit trente-deux texels. Les dimensions de la
  // carte n'y entrent pas.
  GndBuildOptions options;
  options.width = 4;
  options.height = 2;
  options.lightmap_count = 10;
  options.lightmap_width = 8;
  options.lightmap_height = 8;

  const LightmapAtlas atlas = AtlasOf(options);
  REQUIRE_FALSE(atlas.empty());
  CHECK(atlas.image.width == 32);
  CHECK(atlas.image.height == 32);
}

TEST_CASE("Atlas : les lightmaps se suivent en rangées", "[render][lightmap]") {
  GndBuildOptions options;
  options.width = 4;
  options.height = 4;
  options.lightmap_count = 10;
  options.lightmap_width = 8;
  options.lightmap_height = 8;

  const LightmapAtlas atlas = AtlasOf(options);
  REQUIRE(atlas.minimum.size() == 10);

  // Atlas de 32 de côté, lightmaps de 8 : quatre par rangée.
  // La cinquième repart donc à gauche, une rangée plus bas.
  CHECK(atlas.minimum[0][0] < atlas.minimum[1][0]);
  CHECK(atlas.minimum[0][1] == atlas.minimum[3][1]);
  CHECK(atlas.minimum[4][0] == atlas.minimum[0][0]);
  CHECK(atlas.minimum[4][1] > atlas.minimum[0][1]);
  // Et la dixième tient encore dans l'atlas, sur la troisième rangée.
  CHECK(atlas.maximum[9][1] <= 1.0F);
}

TEST_CASE("Atlas : un carreau occupe SIX texels sur huit", "[render][lightmap]") {
  // Le tour d'une lightmap est un BORD, qui reprend les texels des lightmaps
  // voisines : mesuré sur gon_fild01, la colonne 6 d'un carreau vaut
  // exactement la colonne 0 de son voisin de droite, et sa colonne 7 la
  // colonne 1 de celui-ci. C'est ce bord qui rend l'ombre continue d'un
  // carreau à l'autre, et le parcourir le compterait deux fois.
  //
  // ⚠ La marge d'un texel et l'étendue de six ne se déduisent PAS l'une de
  // l'autre : les arrondir en un retrait partagé — un demi texel de chaque
  // côté, sept parcourus — couvre le sol d'un quadrillage fin.
  GndBuildOptions options;
  options.width = 4;
  options.height = 4;
  options.lightmap_count = 1;
  options.lightmap_width = 8;
  options.lightmap_height = 8;

  const LightmapAtlas atlas = AtlasOf(options);
  REQUIRE_FALSE(atlas.minimum.empty());

  const float side = static_cast<float>(atlas.image.width);
  CHECK(atlas.minimum[0][0] == 1.0F / side);
  CHECK(atlas.maximum[0][0] - atlas.minimum[0][0] == 6.0F / side);
}

TEST_CASE("Atlas : alpha et couleur vont dans les bons canaux", "[render][lightmap]") {
  // Une lightmap est faite d'un plan ALPHA suivi d'une couleur RGB par texel.
  // Intervertir l'ombre et la teinte rendrait la carte uniformément grise avec
  // des ombres colorées ; intervertir le rouge et le bleu la fait virer au
  // froid. Quatre valeurs distinctes distinguent les deux fautes.
  GndBuildOptions options;
  options.width = 2;
  options.height = 2;
  // DEUX lightmaps : la grille de deux sur deux garde deux cases vides, dont la
  // dernière, où l'on vérifie que rien n'est écrit.
  options.lightmap_count = 2;
  options.lightmap_width = 4;
  options.lightmap_height = 4;
  constexpr std::uint8_t kShadow = 0x40;
  constexpr std::uint8_t kRed = 0xC0;
  constexpr std::uint8_t kGreen = 0x80;
  constexpr std::uint8_t kBlue = 0x20;
  options.lightmap_shadow = kShadow;
  options.lightmap_rgb = {kRed, kGreen, kBlue};

  const LightmapAtlas atlas = AtlasOf(options);
  REQUIRE_FALSE(atlas.empty());

  // Premier texel de la première lightmap, à l'origine de l'atlas.
  CHECK(atlas.image.pixels[0] == kRed);
  CHECK(atlas.image.pixels[1] == kGreen);
  CHECK(atlas.image.pixels[2] == kBlue);
  CHECK(atlas.image.pixels[3] == kShadow);

  // Au-delà des lightmaps écrites, l'atlas reste à zéro.
  const std::size_t last = atlas.image.pixels.size() - 1;
  CHECK(atlas.image.pixels[last] == 0);
}

TEST_CASE("Atlas : des données tronquées n'écrivent pas hors bornes", "[render][lightmap]") {
  // Le GND annonce plus de lightmaps qu'il n'en porte : la construction doit
  // s'arrêter proprement, pas déborder de l'atlas.
  GndBuildOptions options;
  options.width = 2;
  options.height = 2;
  options.lightmap_count = 2;
  options.lightmap_width = 4;
  options.lightmap_height = 4;

  auto gnd = GndMap::Parse(BuildGnd(options));
  REQUIRE(gnd.has_value());

  const LightmapAtlas atlas = BuildLightmapAtlas(*gnd);
  CHECK(atlas.minimum.size() == 2);
  CHECK(atlas.image.pixels.size() == static_cast<std::size_t>(atlas.image.width) *
                                         static_cast<std::size_t>(atlas.image.height) * 4);
}

TEST_CASE("Atlas : chaque axe suit la dimension de ses lightmaps", "[render][lightmap]") {
  // Cent lightmaps de 16 × 1, dix par rangée. Un atlas carré en demanderait 160².
  GndBuildOptions options;
  options.width = 4;
  options.height = 2;
  options.lightmap_count = 100;
  options.lightmap_width = 16;
  options.lightmap_height = 1;

  const LightmapAtlas atlas = AtlasOf(options);
  REQUIRE_FALSE(atlas.empty());
  CHECK(atlas.image.width == 160);
  CHECK(atlas.image.height == 10);
  CHECK(atlas.image.pixels.size() == 160U * 10U * 4U);
  CHECK(atlas.MinimumOf(10)[0] == atlas.MinimumOf(0)[0]);
  CHECK(atlas.MinimumOf(10)[1] > atlas.MinimumOf(9)[1]);
  CHECK(atlas.MinimumOf(1)[0] > atlas.MinimumOf(0)[0]);
}
