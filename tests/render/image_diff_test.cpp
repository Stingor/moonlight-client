#include "ro/render/image_diff.h"

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/image.h"

using ro::formats::Image;
using ro::render::CompareImages;
using ro::render::ImageComparison;
using ro::render::kNoticeableChannelGap;

namespace {

// Une image unie, dont on pourra changer quelques pixels.
Image Flat(std::int32_t width, std::int32_t height, std::uint8_t level) {
  Image image;
  image.width = width;
  image.height = height;
  image.pixels.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4, 255);
  for (std::size_t offset = 0; offset + 3 < image.pixels.size(); offset += 4) {
    image.pixels[offset] = level;
    image.pixels[offset + 1] = level;
    image.pixels[offset + 2] = level;
  }
  return image;
}

void SetPixel(Image& image, std::int32_t x, std::int32_t y, std::uint8_t level) {
  const std::size_t offset = (static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width) +
                              static_cast<std::size_t>(x)) *
                             4;
  image.pixels[offset] = level;
  image.pixels[offset + 1] = level;
  image.pixels[offset + 2] = level;
}

}  // namespace

TEST_CASE("Comparaison : deux images identiques ne diffèrent en rien", "[render][diff]") {
  const Image left = Flat(9, 9, 100);
  const Image right = Flat(9, 9, 100);

  const auto comparison = CompareImages(left, right);
  REQUIRE(comparison.has_value());
  CHECK(comparison->compared == 81);
  CHECK(comparison->differing == 0);
  CHECK(comparison->worst_gap == 0);
}

TEST_CASE("Comparaison : un écart sous le seuil ne compte pas", "[render][diff]") {
  // Deux rendus du même contenu ne sont jamais identiques au bit près : le
  // filtrage et l'ordre des additions flottantes font varier le dernier
  // chiffre. Compter ces pixels-là noierait le signal.
  const Image left = Flat(9, 9, 100);
  Image right = Flat(9, 9, 100);
  SetPixel(right, 4, 4, static_cast<std::uint8_t>(100 + kNoticeableChannelGap - 1));

  const auto comparison = CompareImages(left, right);
  REQUIRE(comparison.has_value());
  CHECK(comparison->differing == 0);
  CHECK(comparison->worst_gap == kNoticeableChannelGap - 1);
}

TEST_CASE("Comparaison : un écart franc est compté et localisé", "[render][diff]") {
  // Ce qui rend la mesure utile n'est pas le total, c'est la RÉGION : elle dit
  // où regarder, là où une moyenne globale dit seulement qu'il y a un écart.
  const Image left = Flat(9, 9, 100);
  Image right = Flat(9, 9, 100);
  SetPixel(right, 1, 1, 200);

  const auto comparison = CompareImages(left, right);
  REQUIRE(comparison.has_value());
  CHECK(comparison->differing == 1);
  CHECK(comparison->worst_gap == 100);
  // Le pixel (1, 1) tombe dans la région du haut à gauche, la première.
  CHECK(comparison->region_mean[0] > 0.0);
  for (std::size_t region = 1; region < comparison->region_mean.size(); ++region) {
    CHECK(comparison->region_mean[region] == 0.0);
  }
}

TEST_CASE("Comparaison : l'alpha est ignoré", "[render][diff]") {
  // Une capture d'écran est opaque, un rendu hors écran aussi. Les comparer
  // ferait du bruit sur un canal qui ne porte aucune information.
  const Image left = Flat(4, 4, 100);
  Image right = Flat(4, 4, 100);
  right.pixels[3] = 0;

  const auto comparison = CompareImages(left, right);
  REQUIRE(comparison.has_value());
  CHECK(comparison->differing == 0);
}

TEST_CASE("Comparaison : des tailles différentes se refusent", "[render][diff]") {
  // Rééchantillonner pour les rapprocher fabriquerait des pixels, et l'écart
  // mesuré serait en partie le nôtre.
  CHECK_FALSE(CompareImages(Flat(4, 4, 100), Flat(8, 8, 100)).has_value());
  CHECK_FALSE(CompareImages(Image(), Image()).has_value());
}

TEST_CASE("Comparaison : l'image de différence montre où ça diffère", "[render][diff]") {
  // Elle sert à SITUER, jamais à mesurer : l'amplification y sature, et les
  // chiffres sont dans le résultat.
  const Image left = Flat(6, 6, 100);
  Image right = Flat(6, 6, 100);
  SetPixel(right, 2, 3, 150);

  ro::formats::Image difference;
  const auto comparison = CompareImages(left, right, &difference);
  REQUIRE(comparison.has_value());
  REQUIRE(difference.width == 6);
  REQUIRE(difference.height == 6);

  const std::size_t marked = (3 * 6 + 2) * 4;
  CHECK(difference.pixels[marked] == 255);
  // Partout ailleurs, le noir : rien ne diffère.
  CHECK(difference.pixels[0] == 0);
}
