// LE DÉCODEUR PNG, ET CE QU'IL REFUSE.
//
// Comme pour le JPEG, on n'éprouve pas la conformité d'un décodeur — `stb_image`
// s'en charge — mais la FRONTIÈRE : ce qui entre, ce qui sort, l'alpha du
// fichier gardé tel quel, et ce qui est refusé plutôt que deviné.
//
// ⚠ L'IMAGE D'ESSAI EST LA NÔTRE, fabriquée pour l'occasion : aucun octet de
// Gravity n'est versionné dans ce dépôt.

#include "ro/formats/png.h"

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/image.h"

using ro::formats::DecodeImage;
using ro::formats::DecodePng;
using ro::formats::EncodePng;
using ro::formats::Image;
using ro::formats::ImageFile;

namespace {

// Deux pixels sur deux, RGBA : rouge opaque, vert à demi, bleu transparent,
// blanc opaque.
constexpr std::array<std::uint8_t, 78> kTinyPng = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52,
    0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x08, 0x06, 0x00, 0x00, 0x00, 0x72, 0xB6, 0x0D,
    0x24, 0x00, 0x00, 0x00, 0x15, 0x49, 0x44, 0x41, 0x54, 0x78, 0xDA, 0x63, 0xF8, 0xCF, 0xC0, 0xF0,
    0x1F, 0x08, 0x1B, 0x18, 0xC0, 0xF4, 0xFF, 0xFF, 0xFF, 0x01, 0x3F, 0xD7, 0x08, 0x79, 0x6E, 0x0C,
    0xE2, 0x91, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82};

constexpr std::array<std::uint8_t, 16> kExpected = {
    255, 0, 0,   255, 0,   255, 0,   128,  // rangée du haut
    0,   0, 255, 0,   255, 255, 255, 255,  // rangée du bas
};

}  // namespace

TEST_CASE("PNG : les pixels et l'alpha du fichier, rangée du haut d'abord", "[formats][png]") {
  const auto image = DecodePng(kTinyPng);
  REQUIRE(image.has_value());
  CHECK(image->width == 2);
  CHECK(image->height == 2);
  REQUIRE(image->pixels.size() == kExpected.size());
  for (std::size_t index = 0; index < kExpected.size(); ++index) {
    INFO("octet " << index);
    CHECK(image->pixels[index] == kExpected[index]);
  }
}

TEST_CASE("PNG : l'extension l'aiguille, et l'image dit d'où elle vient", "[formats][png]") {
  const auto image = DecodeImage("data\\texture\\illust\\portrait.PNG", kTinyPng);
  REQUIRE(image.has_value());
  CHECK(image->file == ImageFile::kPng);
  CHECK(image->width == 2);
}

TEST_CASE("PNG : ce qui n'en est pas un est refusé, pas deviné", "[formats][png]") {
  // 🔴 UN BMP PARFAITEMENT VALIDE, un pixel en vingt-quatre bits : `stb_image` le
  // lirait sans broncher. C'est la signature, et elle seule, qui le refuse.
  constexpr std::array<std::uint8_t, 58> kValidBmp = {
      0x42, 0x4D, 0x3A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x36, 0x00, 0x00, 0x00, 0x28,
      0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x18, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x13, 0x0B, 0x00, 0x00, 0x13, 0x0B, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x28, 0x1E, 0xC8, 0x00};
  CHECK_FALSE(DecodePng(kValidBmp).has_value());
  CHECK_FALSE(DecodePng(std::span<const std::uint8_t>()).has_value());
  // La signature seule, sans image derrière.
  CHECK_FALSE(DecodePng(std::span(kTinyPng).first(8)).has_value());
}

TEST_CASE("PNG : l'encodage se relit à l'identique, alpha compris", "[formats][png]") {
  Image written;
  written.width = 2;
  written.height = 2;
  written.pixels.assign(kExpected.begin(), kExpected.end());
  const auto encoded = EncodePng(written);
  REQUIRE(encoded.has_value());
  const auto read = DecodePng(*encoded);
  REQUIRE(read.has_value());
  CHECK(read->width == 2);
  CHECK(read->height == 2);
  CHECK(read->pixels == written.pixels);
}

TEST_CASE("PNG : une image incohérente ne s'encode pas", "[formats][png]") {
  Image short_of_pixels;
  short_of_pixels.width = 2;
  short_of_pixels.height = 2;
  short_of_pixels.pixels.assign(4, 0);
  CHECK_FALSE(EncodePng(short_of_pixels).has_value());
  CHECK_FALSE(EncodePng(Image{}).has_value());
}
