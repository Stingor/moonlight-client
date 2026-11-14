#include "ro/formats/icon_tint.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace ro::formats {
namespace {

// Les poids de la luminance de la recommandation 601 : ceux d'une image en
// gamma, comme le sont les bitmaps du jeu.
constexpr float kRedWeight = 0.299F;
constexpr float kGreenWeight = 0.587F;
constexpr float kBlueWeight = 0.114F;
constexpr float kChannelMost = 255.0F;
constexpr std::size_t kRgbaBytes = 4;
constexpr std::size_t kAlphaRank = 3;

// Un canal de la teinte porté à la luminance `luma` : du noir à la couleur
// pleine jusqu'au ton moyen, puis de la couleur pleine au blanc.
[[nodiscard]] std::uint8_t Ramp(std::uint8_t shade, float luma) {
  const float full = static_cast<float>(shade);
  const float value =
      luma <= kIconTintMidLuma
          ? full * luma / kIconTintMidLuma
          : full + (kChannelMost - full) * (luma - kIconTintMidLuma) / (1.0F - kIconTintMidLuma);
  return static_cast<std::uint8_t>(std::lround(std::clamp(value, 0.0F, kChannelMost)));
}

}  // namespace

std::optional<IconTint> IconTintFromWord(std::string_view word) noexcept {
  if (word.empty()) {
    return IconTint::kNone;
  }
  for (const IconTintShade& shade : kIconTintShades) {
    if (shade.word == word) {
      return shade.tint;
    }
  }
  return std::nullopt;
}

std::string_view IconTintWord(IconTint tint) noexcept {
  const IconTintShade* const shade = FindIconTintShade(tint);
  return shade != nullptr ? shade->word : std::string_view();
}

const IconTintShade* FindIconTintShade(IconTint tint) noexcept {
  for (const IconTintShade& shade : kIconTintShades) {
    if (shade.tint == tint) {
      return &shade;
    }
  }
  return nullptr;
}

void ApplyIconTint(Image& image, IconTint tint) {
  const IconTintShade* const shade = FindIconTintShade(tint);
  if (shade == nullptr) {
    return;
  }
  for (std::size_t at = 0; at + kRgbaBytes <= image.pixels.size(); at += kRgbaBytes) {
    std::uint8_t* const pixel = &image.pixels[at];
    if (pixel[kAlphaRank] == 0) {
      continue;
    }
    const float luma =
        (kRedWeight * static_cast<float>(pixel[0]) + kGreenWeight * static_cast<float>(pixel[1]) +
         kBlueWeight * static_cast<float>(pixel[2])) /
        kChannelMost;
    pixel[0] = Ramp(shade->red, luma);
    pixel[1] = Ramp(shade->green, luma);
    pixel[2] = Ramp(shade->blue, luma);
  }
}

}  // namespace ro::formats
