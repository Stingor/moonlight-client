// L'encodeur GIF à fond transparent, jugé par le même lecteur écrit à part.
//
// 🔴 CE QU'ON VÉRIFIE : qu'un pixel peu opaque redevient TRANSPARENT dans la
// toile, qu'un pixel opaque garde sa couleur, que chaque image remet le fond
// avant la suivante — sans quoi un bras levé resterait peint sous le bras
// baissé —, et que chaque image porte SON délai.

#include <cstddef>
#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/gif.h"

#include "support/gif_reader.h"

namespace {

struct Rgba {
  std::uint8_t red = 0;
  std::uint8_t green = 0;
  std::uint8_t blue = 0;
  std::uint8_t alpha = 0;
};

constexpr Rgba kClear{0, 0, 0, 0};
constexpr Rgba kRed{255, 0, 0, 255};
constexpr Rgba kBlue{0, 0, 255, 255};
// Sous la moitié d'opacité : transparent dans le GIF.
constexpr Rgba kFaintGreen{0, 255, 0, 100};
// Au-dessus : opaque, couleur pleine.
constexpr Rgba kDenseWhite{255, 255, 255, 200};

[[nodiscard]] ro::formats::Image Filled(int width, int height, Rgba color) {
  ro::formats::Image image;
  image.width = width;
  image.height = height;
  image.pixels.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U);
  for (std::size_t at = 0; at < image.pixels.size(); at += 4) {
    image.pixels[at + 0] = color.red;
    image.pixels[at + 1] = color.green;
    image.pixels[at + 2] = color.blue;
    image.pixels[at + 3] = color.alpha;
  }
  return image;
}

[[nodiscard]] std::size_t At(const ro::formats::Image& image, int x, int y) {
  return (static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width) +
          static_cast<std::size_t>(x)) *
         4U;
}

void Paint(ro::formats::Image& image, int x, int y, Rgba color) {
  const std::size_t at = At(image, x, y);
  image.pixels[at + 0] = color.red;
  image.pixels[at + 1] = color.green;
  image.pixels[at + 2] = color.blue;
  image.pixels[at + 3] = color.alpha;
}

[[nodiscard]] bool Shows(const ro::formats::Image& canvas, int x, int y, Rgba color) {
  const std::size_t at = At(canvas, x, y);
  return canvas.pixels[at] == color.red && canvas.pixels[at + 1] == color.green &&
         canvas.pixels[at + 2] == color.blue && canvas.pixels[at + 3] == 255;
}

[[nodiscard]] bool Empty(const ro::formats::Image& canvas, int x, int y) {
  return canvas.pixels[At(canvas, x, y) + 3] == 0;
}

[[nodiscard]] ro::testing::GifClip Encoded(const std::vector<ro::formats::Image>& frames,
                                           const std::vector<int>& delays) {
  const auto bytes = ro::formats::EncodeTransparentGif(frames, delays);
  REQUIRE(bytes.has_value());
  const auto clip = ro::testing::ReadGif(*bytes);
  REQUIRE(clip.has_value());
  return *clip;
}

}  // namespace

TEST_CASE("GIF transparent : l'alpha décide de ce qui est peint", "[formats][gif]") {
  ro::formats::Image frame = Filled(6, 4, kClear);
  Paint(frame, 1, 1, kRed);
  Paint(frame, 2, 1, kFaintGreen);
  Paint(frame, 3, 2, kDenseWhite);

  const ro::testing::GifClip clip = Encoded({frame}, {0});
  REQUIRE(clip.frames.size() == 1);
  CHECK(clip.width == 6);
  CHECK(clip.height == 4);
  CHECK(clip.loops_forever);
  const ro::formats::Image& canvas = clip.frames[0].canvas;
  CHECK(Shows(canvas, 1, 1, kRed));
  CHECK(Empty(canvas, 2, 1));
  // Une couleur opaque à 200 ressort PLEINE : le format n'a pas de demi-teinte.
  CHECK(Shows(canvas, 3, 2, Rgba{255, 255, 255, 255}));
  CHECK(Empty(canvas, 0, 0));
  CHECK(Empty(canvas, 5, 3));
  // L'image est rognée à ce qu'elle peint.
  CHECK(clip.frames[0].left == 1);
  CHECK(clip.frames[0].top == 1);
  CHECK(clip.frames[0].width == 3);
  CHECK(clip.frames[0].height == 2);
  CHECK(clip.frames[0].disposal == ro::testing::kGifRestoreBackground);
  CHECK(clip.frames[0].transparent_index == 0);
}

TEST_CASE("GIF transparent : chaque image remet le fond avant la suivante", "[formats][gif]") {
  // Un point qui se déplace : à la seconde image, l'ancienne place doit être
  // VIDE, et non garder le point de la première.
  ro::formats::Image first = Filled(5, 5, kClear);
  Paint(first, 0, 0, kRed);
  ro::formats::Image second = Filled(5, 5, kClear);
  Paint(second, 4, 4, kBlue);

  const ro::testing::GifClip clip = Encoded({first, second}, {7, 13});
  REQUIRE(clip.frames.size() == 2);
  CHECK(Shows(clip.frames[0].canvas, 0, 0, kRed));
  CHECK(Shows(clip.frames[1].canvas, 4, 4, kBlue));
  CHECK(Empty(clip.frames[1].canvas, 0, 0));
  // Le délai est celui de CHAQUE image.
  CHECK(clip.frames[0].delay_centiseconds == 7);
  CHECK(clip.frames[1].delay_centiseconds == 13);
}

TEST_CASE("GIF transparent : une image vide brûle quand même son délai", "[formats][gif]") {
  ro::formats::Image painted = Filled(4, 4, kClear);
  Paint(painted, 2, 2, kRed);
  const ro::formats::Image blank = Filled(4, 4, kClear);

  const ro::testing::GifClip clip = Encoded({painted, blank}, {5, 9});
  REQUIRE(clip.frames.size() == 2);
  CHECK(clip.frames[1].width == 1);
  CHECK(clip.frames[1].height == 1);
  CHECK(clip.frames[1].delay_centiseconds == 9);
  CHECK(Empty(clip.frames[1].canvas, 2, 2));
}

TEST_CASE("GIF transparent : plus de couleurs que la palette, aucun opaque ne disparaît",
          "[formats][gif]") {
  // 🔴 L'INDEX ZÉRO EST LE TROU. Une quantification qui y rangerait une couleur
  // — parce que la table est pleine — percerait le personnage là où il est
  // peint. Mille couleurs distinctes, un pixel transparent sur sept.
  constexpr int kSide = 32;
  ro::formats::Image frame = Filled(kSide, kSide, kClear);
  for (int y = 0; y < kSide; ++y) {
    for (int x = 0; x < kSide; ++x) {
      if ((x + y * kSide) % 7 == 0) {
        continue;
      }
      Paint(frame, x, y,
            Rgba{static_cast<std::uint8_t>(x * 8), static_cast<std::uint8_t>(y * 8),
                 static_cast<std::uint8_t>((x * y) % 256), 255});
    }
  }
  const ro::testing::GifClip clip = Encoded({frame}, {0});
  REQUIRE(clip.frames.size() == 1);
  const ro::formats::Image& canvas = clip.frames[0].canvas;
  for (int y = 0; y < kSide; ++y) {
    for (int x = 0; x < kSide; ++x) {
      const bool painted = (x + y * kSide) % 7 != 0;
      INFO("x " << x << ", y " << y);
      CHECK(Empty(canvas, x, y) == !painted);
    }
  }
}

TEST_CASE("GIF transparent : un délai par image, sinon refus", "[formats][gif]") {
  const std::vector<ro::formats::Image> frames = {Filled(2, 2, kRed), Filled(2, 2, kBlue)};
  const std::vector<int> one_delay = {4};
  CHECK_FALSE(ro::formats::EncodeTransparentGif(frames, one_delay).has_value());
  CHECK_FALSE(ro::formats::EncodeTransparentGif({}, {}).has_value());
  const std::vector<ro::formats::Image> uneven = {Filled(2, 2, kRed), Filled(3, 2, kBlue)};
  const std::vector<int> two_delays = {4, 4};
  CHECK_FALSE(ro::formats::EncodeTransparentGif(uneven, two_delays).has_value());
}
