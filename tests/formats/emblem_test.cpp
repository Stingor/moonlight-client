#include "ro/formats/emblem.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "support/grf_builder.h"
#include "support/image_builder.h"

using ro::formats::CheckEmblemBmp;
using ro::formats::DecodeEmblemImage;
using ro::formats::EmblemFault;
using ro::formats::EmblemPixels;
using ro::formats::EmblemPixelsFromBmp;
using ro::formats::EmblemPixelsFromImage;
using ro::formats::EmblemTransparencyPercent;
using ro::formats::EncodeEmblemBmp;
using ro::formats::Image;
using ro::formats::InflateEmblem;
using ro::formats::kEmblemClearColor;
using ro::formats::kEmblemPixelCount;
using ro::formats::kEmblemSide;
using ro::test_support::BmpBuildOptions;
using ro::test_support::BmpPixel;
using ro::test_support::BuildBmp;

namespace {

constexpr std::uint32_t kRed = 0xD92B2BU;
constexpr std::size_t kEncodedBytes = 1782;
// Où l'en-tête d'un BMP range sa largeur.
constexpr std::size_t kBmpWidthAt = 18;

[[nodiscard]] EmblemPixels Filled(std::uint32_t color) {
  EmblemPixels pixels;
  pixels.fill(color);
  return pixels;
}

void PatchU32(std::vector<std::uint8_t>& bytes, std::size_t at, std::uint32_t value) {
  for (std::size_t index = 0; index < 4; ++index) {
    bytes[at + index] = static_cast<std::uint8_t>(value >> (8 * index));
  }
}

}  // namespace

TEST_CASE("Un emblème encodé pèse 1782 octets et passe les contrôles", "[formats][emblem]") {
  const std::vector<std::uint8_t> bmp = EncodeEmblemBmp(Filled(kRed));
  REQUIRE(bmp.size() == kEncodedBytes);
  CHECK(CheckEmblemBmp(bmp).usable());
  // L'aller-retour garde chaque pixel à sa place : haut en haut, rouge en rouge.
  EmblemPixels pixels = Filled(kRed);
  pixels[0] = 0x0000FFU;
  pixels[kEmblemPixelCount - 1] = 0x00FF00U;
  const auto back = EmblemPixelsFromBmp(EncodeEmblemBmp(pixels));
  REQUIRE(back.has_value());
  CHECK((*back)[0] == 0x0000FFU);
  CHECK((*back)[kEmblemPixelCount - 1] == 0x00FF00U);
  CHECK((*back)[1] == kRed);
}

TEST_CASE("Les contrôles d'emblème refusent dans l'ordre du serveur", "[formats][emblem]") {
  SECTION("trop court") {
    const std::vector<std::uint8_t> bytes(10, 0);
    CHECK(CheckEmblemBmp(bytes).fault == EmblemFault::kTooShort);
  }
  SECTION("taille annoncée menteuse") {
    std::vector<std::uint8_t> bmp = EncodeEmblemBmp(Filled(kRed));
    PatchU32(bmp, 2, 1000);
    CHECK(CheckEmblemBmp(bmp).fault == EmblemFault::kDeclaredSize);
  }
  SECTION("mauvaises dimensions") {
    const std::vector<BmpPixel> pixels(32 * 32, BmpPixel{10, 20, 30});
    const auto bmp = BuildBmp(32, 32, pixels, BmpBuildOptions{.bits_per_pixel = 24});
    const auto verdict = CheckEmblemBmp(bmp);
    CHECK(verdict.fault == EmblemFault::kWrongSize);
    CHECK(verdict.width == 32);
    CHECK(verdict.height == 32);
  }
  SECTION("32 bits refusé") {
    const std::vector<BmpPixel> pixels(24 * 24, BmpPixel{10, 20, 30});
    const auto bmp = BuildBmp(24, 24, pixels, BmpBuildOptions{.bits_per_pixel = 32});
    CHECK(CheckEmblemBmp(bmp).fault == EmblemFault::kDepth);
  }
  SECTION("256 couleurs accepté") {
    const std::vector<BmpPixel> pixels(24 * 24, BmpPixel{10, 20, 30});
    const auto bmp = BuildBmp(24, 24, pixels, BmpBuildOptions{.bits_per_pixel = 8});
    CHECK(CheckEmblemBmp(bmp).usable());
  }
}

TEST_CASE("Le taux de transparence suit la formule du serveur", "[formats][emblem]") {
  // Tout vide : 575 paires consécutives, plus le un de départ, rapportés aux
  // 1728 octets de pixels et multipliés par trois cents — cent pour cent.
  CHECK(EmblemTransparencyPercent(EncodeEmblemBmp(Filled(kEmblemClearColor))) == 100);
  // Tout plein : seul le un de départ compte.
  CHECK(EmblemTransparencyPercent(EncodeEmblemBmp(Filled(kRed))) == 0);
  // La moitié haute vide : un peu moins de la moitié des paires.
  EmblemPixels half = Filled(kRed);
  for (std::size_t index = 0; index < kEmblemPixelCount / 2; ++index) {
    half[index] = kEmblemClearColor;
  }
  const int percent = EmblemTransparencyPercent(EncodeEmblemBmp(half));
  CHECK(percent >= 49);
  CHECK(percent <= 50);
}

TEST_CASE("Une image se centre dans l'emblème, sa transparence reste vide", "[formats][emblem]") {
  Image image;
  image.width = 2;
  image.height = 2;
  image.pixels = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 0, 0, 0, 0, 255};
  const EmblemPixels pixels = EmblemPixelsFromImage(image);
  const std::size_t top = 11;
  const std::size_t left = 11;
  CHECK(pixels[top * kEmblemSide + left] == 0xFF0000U);
  CHECK(pixels[top * kEmblemSide + left + 1] == 0x00FF00U);
  // Le pixel à alpha nul ne devient pas noir : il reste vide.
  CHECK(pixels[(top + 1) * kEmblemSide + left] == kEmblemClearColor);
  CHECK(pixels[(top + 1) * kEmblemSide + left + 1] == 0x000000U);
  CHECK(pixels[0] == kEmblemClearColor);
}

TEST_CASE("Un emblème s'affiche avec son magenta transparent", "[formats][emblem]") {
  EmblemPixels pixels = Filled(kRed);
  pixels[0] = kEmblemClearColor;
  const auto image = DecodeEmblemImage(EncodeEmblemBmp(pixels));
  REQUIRE(image.has_value());
  CHECK(image->width == kEmblemSide);
  CHECK(image->pixels[3] == 0);
  CHECK(image->pixels[7] == 255);
}

TEST_CASE("Un emblème démesuré se refuse sur son en-tête, sans décodage", "[formats][emblem]") {
  // 🔴 TREIZE OCTETS SUFFISENT À ANNONCER SEIZE MILLE PIXELS DE CÔTÉ : décoder
  // d'abord allouerait des gigaoctets chez chaque joueur qui voit la guilde.
  constexpr std::uint8_t kSideLow = 0x00;
  constexpr std::uint8_t kSideHigh = 0x40;  // 0x4000 = 16384
  const std::vector<std::uint8_t> gif = {'G',       'I',      'F',       '8',  '9',  'a',  kSideLow,
                                         kSideHigh, kSideLow, kSideHigh, 0x00, 0x00, 0x00, 0x3B};
  const auto refused_gif = DecodeEmblemImage(gif);
  REQUIRE_FALSE(refused_gif.has_value());
  CHECK(refused_gif.error().detail.find("16384") != std::string::npos);

  // Le même piège par un BMP : l'en-tête se lit avant le décodeur.
  std::vector<std::uint8_t> bmp = EncodeEmblemBmp(Filled(kRed));
  bmp[kBmpWidthAt] = kSideLow;
  bmp[kBmpWidthAt + 1] = kSideHigh;
  const auto refused_bmp = DecodeEmblemImage(bmp);
  REQUIRE_FALSE(refused_bmp.has_value());
  CHECK(refused_bmp.error().detail.find("16384") != std::string::npos);
}

TEST_CASE("Le chemin historique inflate un BMP compressé", "[formats][emblem]") {
  const std::vector<std::uint8_t> bmp = EncodeEmblemBmp(Filled(kRed));
  std::vector<std::uint8_t> packed = ro::test_support::Deflate(bmp);
  REQUIRE_FALSE(packed.empty());
  const auto inflated = InflateEmblem(packed);
  REQUIRE(inflated.has_value());
  CHECK(*inflated == bmp);
  // Tronqué : refusé, jamais rendu à moitié.
  packed.resize(packed.size() / 2);
  CHECK_FALSE(InflateEmblem(packed).has_value());
}
