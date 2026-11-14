// LES PIXELS D'UNE VIGNETTE DE CARTE : la taille, la réduction, la silhouette.

#include "ro/render/card_thumbnail_pixels.h"

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

using ro::formats::Image;
using ro::render::FitThumbnail;
using ro::render::MakeSilhouette;
using ro::render::ShrinkImage;
using ro::render::ThumbnailSize;

namespace {

constexpr int kChannels = 4;

[[nodiscard]] Image Solid(int width, int height, std::uint8_t r, std::uint8_t g, std::uint8_t b,
                          std::uint8_t a) {
  Image image;
  image.width = width;
  image.height = height;
  for (int i = 0; i < width * height; ++i) {
    image.pixels.push_back(r);
    image.pixels.push_back(g);
    image.pixels.push_back(b);
    image.pixels.push_back(a);
  }
  return image;
}

}  // namespace

TEST_CASE("Vignette : une illustration de carte tient dans sa pochette", "[render][album]") {
  // 300 sur 400 dans 120 sur 160 : le facteur deux et demi, exact.
  const ThumbnailSize size = FitThumbnail(300, 400, 120, 160);
  CHECK(size.width == 120);
  CHECK(size.height == 160);
  // Une illustration plus courte garde son ratio et se centre ensuite.
  const ThumbnailSize short_one = FitThumbnail(300, 240, 120, 160);
  CHECK(short_one.width == 120);
  CHECK(short_one.height == 96);
  // Jamais agrandie.
  const ThumbnailSize small = FitThumbnail(50, 60, 120, 160);
  CHECK(small.width == 50);
  CHECK(small.height == 60);
}

TEST_CASE("Vignette : la transparence ne bave pas dans la couleur", "[render][album]") {
  // Une colonne blanche opaque et une colonne magenta TRANSPARENTE : la
  // moyenne pondérée par l'alpha reste blanche, à moitié couvrante.
  Image image = Solid(2, 1, 255, 255, 255, 255);
  image.pixels[kChannels + 0] = 255;
  image.pixels[kChannels + 1] = 0;
  image.pixels[kChannels + 2] = 255;
  image.pixels[kChannels + 3] = 0;
  const Image shrunk = ShrinkImage(image, ThumbnailSize{.width = 1, .height = 1});
  REQUIRE(shrunk.pixels.size() == static_cast<std::size_t>(kChannels));
  CHECK(shrunk.pixels[0] == 255);
  CHECK(shrunk.pixels[1] == 255);
  CHECK(shrunk.pixels[2] == 255);
  CHECK(shrunk.pixels[3] > 0);
  CHECK(shrunk.pixels[3] < 255);
}

TEST_CASE("Vignette : la silhouette est grise et garde sa transparence", "[render][album]") {
  Image image = Solid(1, 2, 200, 40, 40, 255);
  image.pixels[kChannels + 3] = 0;
  MakeSilhouette(image);
  CHECK(image.pixels[0] == image.pixels[1]);
  CHECK(image.pixels[1] == image.pixels[2]);
  CHECK(image.pixels[3] == 255);
  // Le pixel transparent n'est pas touché.
  CHECK(image.pixels[kChannels + 0] == 200);
}
