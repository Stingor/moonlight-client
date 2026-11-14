// LE DÉCODAGE D'UN WEBP PAR LIBWEBP, sur deux fichiers minuscules écrits ici
// octet par octet : une image fixe sans perte et une animation de deux images.

#include "web/webp_picture.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include <catch2/catch_test_macros.hpp>

namespace {

constexpr std::size_t kChannels = 4;

// 2 × 1, sans perte : un pixel rouge opaque, puis un pixel bleu à demi
// transparent (alpha 128).
constexpr std::array<std::uint8_t, 40> kStillWebp = {
    0x52, 0x49, 0x46, 0x46, 0x20, 0x00, 0x00, 0x00, 0x57, 0x45, 0x42, 0x50, 0x56, 0x50,
    0x38, 0x4C, 0x13, 0x00, 0x00, 0x00, 0x2F, 0x01, 0x00, 0x00, 0x10, 0x0F, 0x10, 0xFB,
    0x3F, 0xFF, 0x0F, 0xFC, 0x8F, 0x0A, 0x15, 0x88, 0xE8, 0x7F, 0x00, 0x00,
};

// 1 × 1 animé, sans perte : rouge pendant 100 ms, puis vert pendant 250 ms.
constexpr std::array<std::uint8_t, 140> kAnimatedWebp = {
    0x52, 0x49, 0x46, 0x46, 0x84, 0x00, 0x00, 0x00, 0x57, 0x45, 0x42, 0x50, 0x56, 0x50, 0x38, 0x58,
    0x0A, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x41, 0x4E,
    0x49, 0x4D, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x41, 0x4E, 0x4D, 0x46,
    0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x64, 0x00, 0x00, 0x02, 0x56, 0x50, 0x38, 0x4C, 0x0F, 0x00, 0x00, 0x00, 0x2F, 0x00, 0x00, 0x00,
    0x00, 0x07, 0x10, 0xFD, 0x8F, 0xFE, 0x07, 0x22, 0xA2, 0xFF, 0x01, 0x00, 0x41, 0x4E, 0x4D, 0x46,
    0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xFA, 0x00, 0x00, 0x00, 0x56, 0x50, 0x38, 0x4C, 0x0F, 0x00, 0x00, 0x00, 0x2F, 0x00, 0x00, 0x00,
    0x00, 0x07, 0xD0, 0xFF, 0x88, 0xFE, 0x07, 0x22, 0xA2, 0xFF, 0x01, 0x00,
};

constexpr std::uint8_t kOpaque = 255;
constexpr std::uint8_t kHalfAlpha = 128;
constexpr std::uint32_t kRedDelayMs = 100;
constexpr std::uint32_t kGreenDelayMs = 250;

struct Rgba {
  std::uint8_t red = 0;
  std::uint8_t green = 0;
  std::uint8_t blue = 0;
  std::uint8_t alpha = 0;
};

[[nodiscard]] Rgba PixelAt(const ro::formats::Image& image, std::size_t index) {
  const std::size_t at = index * kChannels;
  return Rgba{.red = image.pixels[at],
              .green = image.pixels[at + 1],
              .blue = image.pixels[at + 2],
              .alpha = image.pixels[at + 3]};
}

}  // namespace

TEST_CASE("libwebp : une image fixe sans perte se décode, alpha non prémultiplié", "[app][web]") {
  const auto decoded = moonclient::DecodeWebpPicture(kStillWebp);
  REQUIRE(decoded.has_value());
  REQUIRE(decoded->frames.size() == 1);
  const ro::formats::Image& image = decoded->frames.front();
  CHECK(image.width == 2);
  CHECK(image.height == 1);
  REQUIRE(image.pixels.size() == 2 * kChannels);
  const Rgba red = PixelAt(image, 0);
  CHECK(red.red == kOpaque);
  CHECK(red.green == 0);
  CHECK(red.blue == 0);
  CHECK(red.alpha == kOpaque);
  const Rgba blue = PixelAt(image, 1);
  CHECK(blue.red == 0);
  CHECK(blue.green == 0);
  CHECK(blue.blue == kOpaque);
  CHECK(blue.alpha == kHalfAlpha);
}

TEST_CASE("libwebp : un WebP animé rend chaque image et sa durée", "[app][web]") {
  const auto decoded = moonclient::DecodeWebpPicture(kAnimatedWebp);
  REQUIRE(decoded.has_value());
  REQUIRE(decoded->frames.size() == 2);
  REQUIRE(decoded->delays_ms.size() == 2);
  CHECK(decoded->delays_ms[0] == kRedDelayMs);
  CHECK(decoded->delays_ms[1] == kGreenDelayMs);
  const Rgba first = PixelAt(decoded->frames[0], 0);
  CHECK(first.red == kOpaque);
  CHECK(first.green == 0);
  const Rgba second = PixelAt(decoded->frames[1], 0);
  CHECK(second.red == 0);
  CHECK(second.green == kOpaque);
}

TEST_CASE("libwebp : un WebP tronqué ou un autre format est refusé", "[app][web]") {
  const std::span<const std::uint8_t> still(kStillWebp);
  constexpr std::size_t kHeaderOnly = 20;
  CHECK_FALSE(moonclient::DecodeWebpPicture(still.first(kHeaderOnly)).has_value());
  constexpr std::array<std::uint8_t, 8> kPngSignature = {0x89, 0x50, 0x4E, 0x47,
                                                         0x0D, 0x0A, 0x1A, 0x0A};
  CHECK_FALSE(moonclient::DecodeWebpPicture(kPngSignature).has_value());
}
