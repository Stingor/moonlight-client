// LA TRANSPARENCE DES TEXTURES D'UN FICHIER D'EFFET : le jeu y efface le noir
// en plus du magenta, à la précision de sa texture de seize bits. Le bouclier
// de Smite se peint « un vers zéro » sur une BMP à fond noir ; sans cette clé,
// ce fond devenait un carré opaque autour de lui.

#include "ro/render/effect_catalog.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/vfs/vfs.h"

#include "support/effect_builder.h"
#include "support/grf_builder.h"
#include "support/image_builder.h"
#include "support/temp_directory.h"

using ro::test_support::BmpBuildOptions;
using ro::test_support::BmpPixel;
using ro::test_support::BuildBmp;
using ro::test_support::BuildGrf;
using ro::test_support::BuildStr;
using ro::test_support::BuildTga;
using ro::test_support::StrBuildOptions;
using ro::test_support::StrKeyFrameSpec;
using ro::test_support::StrLayerSpec;
using ro::test_support::TempDirectory;
using ro::test_support::TgaPixel;

namespace {

constexpr std::string_view kEffectPath = "data\\texture\\effect\\bouclier.str";
constexpr int kOpaque = 255;
// D3DBLEND_ONE vers D3DBLEND_ZERO : la couche REMPLACE ce qui est dessous.
constexpr std::int32_t kBlendOne = 2;
constexpr std::int32_t kBlendZero = 1;

// Un client qui ne porte qu'un fichier d'effet d'une couche, et sa texture.
ro::vfs::Vfs ClientWithTexture(const TempDirectory& client, const std::string& texture_name,
                               std::vector<std::uint8_t> texture) {
  StrKeyFrameSpec image;
  image.blend_source = kBlendOne;
  image.blend_destination = kBlendZero;
  StrLayerSpec layer;
  layer.texture_names = {texture_name};
  layer.key_frames = {image};
  StrBuildOptions file;
  file.layers = {layer};
  client.WriteFile("effet.grf",
                   BuildGrf({{.name = std::string(kEffectPath), .content = BuildStr(file)},
                             {.name = "data\\texture\\effect\\" + texture_name,
                              .content = std::move(texture)}}));
  auto mounted = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"effet.grf"}});
  REQUIRE(mounted.has_value());
  return std::move(*mounted);
}

// L'alpha de chaque texel de la première texture, dans l'ordre de l'image. En
// entiers : un échec s'imprime alors en nombres, et non en caractères.
std::vector<int> AlphasOf(const ro::render::LoadedEffect& effect) {
  REQUIRE(effect.textures.size() == 1);
  std::vector<int> alphas;
  const std::vector<std::uint8_t>& pixels = effect.textures.front().pixels;
  for (std::size_t offset = 3; offset < pixels.size(); offset += 4) {
    alphas.push_back(pixels[offset]);
  }
  return alphas;
}

}  // namespace

TEST_CASE("une BMP de fichier d'effet perd son noir à cinq bits près, et son magenta",
          "[effect_catalog]") {
  const std::vector<BmpPixel> pixels = {
      {0, 0, 0},      // le fond du bouclier : transparent
      {7, 7, 7},      // encore noir en cinq bits : transparent
      {8, 0, 0},      // un cran de rouge : opaque
      {248, 7, 248},  // magenta en cinq bits : transparent
  };
  TempDirectory client;
  ro::vfs::Vfs vfs = ClientWithTexture(
      client, "shield.bmp", BuildBmp(2, 2, pixels, BmpBuildOptions{.bits_per_pixel = 24}));
  ro::render::EffectCatalog catalog(vfs);

  const ro::render::LoadedEffect* effect = catalog.Load(kEffectPath);
  REQUIRE(effect != nullptr);
  CHECK(AlphasOf(*effect) == std::vector<int>{0, 0, kOpaque, 0});
}

TEST_CASE("une TGA de fichier d'effet se juge à quatre bits, alpha compris", "[effect_catalog]") {
  const std::vector<TgaPixel> pixels = {
      TgaPixel{.red = 15, .green = 15, .blue = 15, .alpha = 200},    // noir en quatre bits
      TgaPixel{.red = 16, .green = 0, .blue = 0, .alpha = 200},      // un cran de rouge
      TgaPixel{.red = 255, .green = 255, .blue = 255, .alpha = 15},  // alpha nul en quatre bits
      TgaPixel{.red = 240, .green = 15, .blue = 240, .alpha = 255},  // magenta en quatre bits
  };
  TempDirectory client;
  ro::vfs::Vfs vfs = ClientWithTexture(client, "halo.tga", BuildTga(2, 2, pixels));
  ro::render::EffectCatalog catalog(vfs);

  const ro::render::LoadedEffect* effect = catalog.Load(kEffectPath);
  REQUIRE(effect != nullptr);
  // 200 en quatre bits vaut douze, relu 204.
  constexpr int kKeptAlpha = 204;
  CHECK(AlphasOf(*effect) == std::vector<int>{0, kKeptAlpha, 0, 0});
}

TEST_CASE("une texture de fichier d'effet n'a que seize bits : ses dégradés vont par paliers",
          "[effect_catalog]") {
  // Cinq bits par canal pour une BMP, relus en recopiant les bits de poids
  // fort : 100 garde 12, relu 99 ; 200 garde 25, relu 206.
  const std::vector<BmpPixel> bmp = {{100, 64, 200}};
  TempDirectory bmp_client;
  ro::vfs::Vfs bmp_vfs = ClientWithTexture(
      bmp_client, "plat.bmp", BuildBmp(1, 1, bmp, BmpBuildOptions{.bits_per_pixel = 24}));
  ro::render::EffectCatalog bmp_catalog(bmp_vfs);
  const ro::render::LoadedEffect* bmp_effect = bmp_catalog.Load(kEffectPath);
  REQUIRE(bmp_effect != nullptr);
  const std::vector<std::uint8_t>& bmp_texel = bmp_effect->textures.front().pixels;
  CHECK(std::vector<int>(bmp_texel.begin(), bmp_texel.end()) ==
        std::vector<int>{99, 66, 206, kOpaque});

  // Quatre bits par canal pour une TGA, alpha compris : seize fois dix-sept.
  const std::vector<TgaPixel> tga = {TgaPixel{.red = 100, .green = 50, .blue = 200, .alpha = 130}};
  TempDirectory tga_client;
  ro::vfs::Vfs tga_vfs = ClientWithTexture(tga_client, "doux.tga", BuildTga(1, 1, tga));
  ro::render::EffectCatalog tga_catalog(tga_vfs);
  const ro::render::LoadedEffect* tga_effect = tga_catalog.Load(kEffectPath);
  REQUIRE(tga_effect != nullptr);
  const std::vector<std::uint8_t>& tga_texel = tga_effect->textures.front().pixels;
  CHECK(std::vector<int>(tga_texel.begin(), tga_texel.end()) ==
        std::vector<int>{102, 51, 204, 136});
}
