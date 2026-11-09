#include "ro/render/hue_classes.h"

#include <algorithm>
#include <cstdlib>

namespace ro::render {
namespace {

constexpr int kWholePercent = 100;
constexpr int kChannelTop = 255;
// La conversion HSV découpe le cercle en six secteurs.
constexpr int kHsvSectors = 6;
// Une image en couleurs vraies : rouge, vert, bleu, alpha.
constexpr std::size_t kRgbaChannels = 4;
constexpr std::size_t kAlphaChannel = 3;

}  // namespace

std::size_t HueClassOf(int red, int green, int blue, int grey_percent) {
  const int high = std::max({red, green, blue});
  const int low = std::min({red, green, blue});
  const int spread = high - low;
  // La saturation TSL vaut écart / (255 − |haut + bas − 255|).
  const int lightness_room = kChannelTop - std::abs(high + low - kChannelTop);
  if (spread == 0 || lightness_room == 0 ||
      kWholePercent * spread < grey_percent * lightness_room) {
    return kGreyHueClass;
  }
  // La teinte en pas de 1/(6 × écart) de tour, comme les coupes du profil 1.
  int steps = 0;
  if (red == high) {
    steps = green - blue;
  } else if (green == high) {
    steps = 2 * spread + blue - red;
  } else {
    steps = 4 * spread + red - green;
  }
  if (steps < 0) {
    steps += kHsvSectors * spread;
  }
  // Des secteurs centrés sur 0°, 30°, 60°… : un demi-secteur de décalage.
  const int sector_count = static_cast<int>(kHueSectorClasses);
  const int half_sectors = 2 * steps * sector_count / (kHsvSectors * spread);
  return static_cast<std::size_t>(((half_sectors + 1) / 2) % sector_count);
}

formats::Palette ApplyHueClasses(const formats::Palette& base, const HueClassAdjusts& adjusts) {
  formats::Palette painted = base;
  for (std::size_t index = 1; index < formats::kPaletteEntryCount; ++index) {
    const auto& entry = base.entries[index];
    const RampAdjust& adjust = adjusts[HueClassOf(entry[0], entry[1], entry[2])];
    if (!adjust.neutral()) {
      AdjustColor(entry.data(), adjust, painted.entries[index].data());
    }
  }
  return painted;
}

void TintImage(formats::Image& image, const HueClassAdjusts& adjusts) {
  for (std::size_t at = 0; at + kRgbaChannels <= image.pixels.size(); at += kRgbaChannels) {
    std::uint8_t* pixel = &image.pixels[at];
    if (pixel[kAlphaChannel] == 0) {
      continue;
    }
    const RampAdjust& adjust = adjusts[HueClassOf(pixel[0], pixel[1], pixel[2])];
    if (!adjust.neutral()) {
      AdjustColor(pixel, adjust, pixel);
    }
  }
}

}  // namespace ro::render
