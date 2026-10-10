#include "ro/render/card_thumbnail_pixels.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace ro::render {
namespace {

constexpr int kChannels = 4;
constexpr int kRed = 0;
constexpr int kGreen = 1;
constexpr int kBlue = 2;
constexpr int kAlpha = 3;

// Points d'échantillonnage par côté de texel.
constexpr int kSamplesPerSide = 3;
constexpr float kSampleCenter = 0.5F;

// La luminance, en poids sur 256 : 77 rouge, 151 vert, 28 bleu.
constexpr unsigned kLumaRed = 77;
constexpr unsigned kLumaGreen = 151;
constexpr unsigned kLumaBlue = 28;
constexpr unsigned kLumaShift = 8;
// La bande de la silhouette : de 28 à 28 + 255 / 2.
constexpr unsigned kSilhouetteFloor = 28;
constexpr unsigned kSilhouetteSpan = 2;

[[nodiscard]] std::size_t At(int x, int y, int width) {
  return (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
          static_cast<std::size_t>(x)) *
         kChannels;
}

}  // namespace

ThumbnailSize FitThumbnail(int width, int height, int most_width, int most_height) {
  if (width <= 0 || height <= 0) {
    return {};
  }
  const float scale =
      std::min({static_cast<float>(std::max(1, most_width)) / static_cast<float>(width),
                static_cast<float>(std::max(1, most_height)) / static_cast<float>(height), 1.0F});
  return ThumbnailSize{
      .width = std::max(1, static_cast<int>(static_cast<float>(width) * scale + kSampleCenter)),
      .height = std::max(1, static_cast<int>(static_cast<float>(height) * scale + kSampleCenter)),
  };
}

formats::Image ShrinkImage(const formats::Image& source, ThumbnailSize size) {
  formats::Image out;
  if (source.width <= 0 || source.height <= 0 || size.width <= 0 || size.height <= 0 ||
      source.pixels.size() < At(0, source.height, source.width)) {
    return out;
  }
  out.width = size.width;
  out.height = size.height;
  out.file = source.file;
  out.pixels.assign(At(0, size.height, size.width), 0);
  const float step_x = static_cast<float>(source.width) / static_cast<float>(size.width);
  const float step_y = static_cast<float>(source.height) / static_cast<float>(size.height);
  for (int y = 0; y < size.height; ++y) {
    for (int x = 0; x < size.width; ++x) {
      unsigned red = 0;
      unsigned green = 0;
      unsigned blue = 0;
      unsigned alpha = 0;
      for (int j = 0; j < kSamplesPerSide; ++j) {
        const float fy =
            (static_cast<float>(y) + (static_cast<float>(j) + kSampleCenter) / kSamplesPerSide) *
            step_y;
        const int sy = std::min(source.height - 1, static_cast<int>(fy));
        for (int i = 0; i < kSamplesPerSide; ++i) {
          const float fx =
              (static_cast<float>(x) + (static_cast<float>(i) + kSampleCenter) / kSamplesPerSide) *
              step_x;
          const int sx = std::min(source.width - 1, static_cast<int>(fx));
          const std::uint8_t* p = &source.pixels[At(sx, sy, source.width)];
          const unsigned a = p[kAlpha];
          red += p[kRed] * a;
          green += p[kGreen] * a;
          blue += p[kBlue] * a;
          alpha += a;
        }
      }
      std::uint8_t* q = &out.pixels[At(x, y, size.width)];
      if (alpha == 0) {
        continue;
      }
      q[kRed] = static_cast<std::uint8_t>(red / alpha);
      q[kGreen] = static_cast<std::uint8_t>(green / alpha);
      q[kBlue] = static_cast<std::uint8_t>(blue / alpha);
      q[kAlpha] = static_cast<std::uint8_t>(alpha / (kSamplesPerSide * kSamplesPerSide));
    }
  }
  return out;
}

void MakeSilhouette(formats::Image& image) {
  for (std::size_t at = 0; at + kAlpha < image.pixels.size(); at += kChannels) {
    std::uint8_t* p = &image.pixels[at];
    if (p[kAlpha] == 0) {
      continue;
    }
    const unsigned luma =
        (kLumaRed * p[kRed] + kLumaGreen * p[kGreen] + kLumaBlue * p[kBlue]) >> kLumaShift;
    const auto value = static_cast<std::uint8_t>(kSilhouetteFloor + luma / kSilhouetteSpan);
    p[kRed] = value;
    p[kGreen] = value;
    p[kBlue] = value;
  }
}

}  // namespace ro::render
