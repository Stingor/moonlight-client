// UNE IMAGE DU CHAT, PRÊTE À TÉLÉVERSER : la réduction d'une image fixe et d'une
// animation, le budget d'une animation, et la garde qui refuse une bombe avant
// de la décoder.

#include "ro/render/chat_picture.h"

#include <array>
#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/gif.h"

#include "support/image_builder.h"

using ro::formats::DecodedWebPicture;
using ro::formats::Image;
using ro::render::ChatPicture;
using ro::render::ChatPictureLimits;
using ro::render::FitChatPicture;
using ro::render::PrepareChatPicture;

namespace {

constexpr std::size_t kChannels = 4;
constexpr int kDelayCs = 5;
constexpr std::uint32_t kDelayMs = 50;

[[nodiscard]] Image Filled(int width, int height, std::uint8_t shade) {
  Image image;
  image.width = width;
  image.height = height;
  image.pixels.assign(
      static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * kChannels, shade);
  return image;
}

[[nodiscard]] std::size_t PictureBytes(const ChatPicture& picture) {
  std::size_t total = 0;
  for (const Image& frame : picture.frames) {
    total += frame.pixels.size();
  }
  return total;
}

// UN PNG QUI ANNONCE SES CÔTÉS SANS RIEN PORTER : la signature, l'en-tête, et le
// début d'un bloc de données que rien ne remplit.
[[nodiscard]] std::vector<std::uint8_t> AnnouncedPng(std::uint32_t width, std::uint32_t height) {
  std::vector<std::uint8_t> png = {0x89, 'P',  'N',  'G',  0x0D, 0x0A, 0x1A, 0x0A,
                                   0x00, 0x00, 0x00, 0x0D, 'I',  'H',  'D',  'R'};
  for (const std::uint32_t side : {width, height}) {
    png.insert(png.end(),
               {static_cast<std::uint8_t>(side >> 24U), static_cast<std::uint8_t>(side >> 16U),
                static_cast<std::uint8_t>(side >> 8U), static_cast<std::uint8_t>(side)});
  }
  // Huit bits par canal, RGBA, sans entrelacement ; le contrôle n'est pas lu.
  png.insert(png.end(), {0x08, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
  png.insert(png.end(), {0x00, 0x00, 0x00, 0x00, 'I', 'D', 'A', 'T', 0x00, 0x00, 0x00, 0x00});
  return png;
}

[[nodiscard]] std::vector<std::uint8_t> Clip(int frame_count, int width, int height) {
  std::vector<Image> frames;
  for (int index = 0; index < frame_count; ++index) {
    Image frame = Filled(width, height, 255);
    frame.pixels[static_cast<std::size_t>(index) * kChannels] = 0;
    frames.push_back(std::move(frame));
  }
  auto gif = ro::formats::EncodeAnimatedGif(frames, kDelayCs);
  REQUIRE(gif.has_value());
  return *gif;
}

}  // namespace

TEST_CASE("Image du chat : une image fixe se réduit à 512 de côté", "[render][chat]") {
  const std::vector<ro::test_support::BmpPixel> pixels(
      2000U * 1000U, ro::test_support::BmpPixel{.red = 40, .green = 80, .blue = 120});
  const auto bmp = ro::test_support::BuildBmp(2000, 1000, pixels, {.bits_per_pixel = 24});
  const auto picture = PrepareChatPicture(bmp);
  REQUIRE(picture.has_value());
  REQUIRE(picture->frames.size() == 1);
  CHECK_FALSE(picture->animated());
  CHECK(picture->frames[0].width == 512);
  CHECK(picture->frames[0].height == 256);
  // La couleur traverse la réduction.
  CHECK(picture->frames[0].pixels[0] == 40);
  CHECK(picture->frames[0].pixels[2] == 120);
}

TEST_CASE("Image du chat : une petite image n'est jamais agrandie", "[render][chat]") {
  DecodedWebPicture decoded;
  decoded.frames.push_back(Filled(30, 20, 9));
  decoded.delays_ms.push_back(0);
  const auto picture = FitChatPicture(std::move(decoded));
  REQUIRE(picture.has_value());
  CHECK(picture->frames[0].width == 30);
  CHECK(picture->frames[0].height == 20);
}

TEST_CASE("Image du chat : une animation se réduit à 256 de côté", "[render][chat]") {
  const auto gif = Clip(3, 400, 200);
  const auto picture = PrepareChatPicture(gif);
  REQUIRE(picture.has_value());
  CHECK(picture->animated());
  REQUIRE(picture->frames.size() == 3);
  REQUIRE(picture->delays_ms.size() == 3);
  for (const Image& frame : picture->frames) {
    CHECK(frame.width == 256);
    CHECK(frame.height == 128);
  }
  CHECK(picture->delays_ms[0] == kDelayMs);
}

TEST_CASE("Image du chat : une animation trop longue garde ses premières images",
          "[render][chat]") {
  const ChatPictureLimits limits{.most_frames = 3};
  const auto gif = Clip(5, 8, 4);
  const auto picture = PrepareChatPicture(gif, limits);
  REQUIRE(picture.has_value());
  CHECK(picture->frames.size() == 3);
  CHECK(picture->delays_ms.size() == 3);

  DecodedWebPicture decoded;
  for (int index = 0; index < 5; ++index) {
    decoded.frames.push_back(Filled(4, 4, 1));
  }
  const auto fitted = FitChatPicture(std::move(decoded), limits);
  REQUIRE(fitted.has_value());
  CHECK(fitted->frames.size() == 3);
  // Un délai manquant vaut zéro, jamais une case de trop ou de moins.
  CHECK(fitted->delays_ms.size() == 3);
}

TEST_CASE("Image du chat : le budget d'une animation fait baisser son côté", "[render][chat]") {
  // Quatre images de 100 × 100 tiennent sous 256 de côté, mais le budget n'en
  // laisse que 50 × 50 à chacune.
  constexpr std::size_t kBudget = 50U * 50U * kChannels * 4U;
  const ChatPictureLimits limits{.most_animated_bytes = kBudget};
  DecodedWebPicture decoded;
  for (int index = 0; index < 4; ++index) {
    decoded.frames.push_back(Filled(100, 100, 7));
    decoded.delays_ms.push_back(kDelayMs);
  }
  const auto picture = FitChatPicture(std::move(decoded), limits);
  REQUIRE(picture.has_value());
  REQUIRE(picture->frames.size() == 4);
  CHECK(picture->frames[0].width == 50);
  CHECK(picture->frames[0].height == 50);
  CHECK(PictureBytes(*picture) <= kBudget);

  // Le budget d'usine, sur soixante images carrées : sous les 256 de côté.
  DecodedWebPicture long_clip;
  for (int index = 0; index < 60; ++index) {
    long_clip.frames.push_back(Filled(256, 256, 3));
  }
  const auto heavy = FitChatPicture(std::move(long_clip));
  REQUIRE(heavy.has_value());
  CHECK(heavy->frames[0].width < 256);
  CHECK(heavy->frames[0].width == heavy->frames[0].height);
  CHECK(PictureBytes(*heavy) <= ChatPictureLimits().most_animated_bytes);

  // Une image fixe, elle, n'a pas de budget d'animation.
  DecodedWebPicture still;
  still.frames.push_back(Filled(100, 100, 7));
  const auto fixed = FitChatPicture(std::move(still), limits);
  REQUIRE(fixed.has_value());
  CHECK(fixed->frames[0].width == 100);
}

TEST_CASE("Image du chat : une bombe est refusée avant le décodage", "[render][chat]") {
  // 16384 × 16384 : un gigaoctet annoncé par quelques dizaines d'octets.
  const auto bomb = PrepareChatPicture(AnnouncedPng(16384, 16384));
  REQUIRE_FALSE(bomb.has_value());
  CHECK(bomb.error().code == ro::core::ErrorCode::kInvalidArgument);

  // Les mêmes octets, à une taille raisonnable : la mesure passe, et c'est le
  // DÉCODAGE qui échoue, faute de pixels.
  const auto honest = PrepareChatPicture(AnnouncedPng(64, 64));
  REQUIRE_FALSE(honest.has_value());
  CHECK(honest.error().code != ro::core::ErrorCode::kInvalidArgument);

  // Une animation se compte image par image, au plus soixante.
  const ChatPictureLimits tight{.most_decoded_bytes = 8U * 4U * kChannels * 2U};
  CHECK(PrepareChatPicture(Clip(2, 8, 4), tight).has_value());
  CHECK_FALSE(PrepareChatPicture(Clip(3, 8, 4), tight).has_value());
}

TEST_CASE("Image du chat : ce qui n'est pas une image est refusé", "[render][chat]") {
  CHECK_FALSE(PrepareChatPicture(std::vector<std::uint8_t>{'<', 'h', 't', 'm', 'l'}).has_value());
  CHECK_FALSE(FitChatPicture(DecodedWebPicture()).has_value());

  DecodedWebPicture mismatched;
  mismatched.frames.push_back(Filled(4, 4, 1));
  mismatched.frames.push_back(Filled(5, 4, 1));
  CHECK_FALSE(FitChatPicture(std::move(mismatched)).has_value());
}
