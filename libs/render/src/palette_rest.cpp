#include "ro/render/palette_rest.h"

#include <algorithm>

namespace ro::render {
namespace {

constexpr int kPercent = 100;
constexpr std::size_t kRgbChannels = 3;

[[nodiscard]] bool IsKeyMagenta(const std::uint8_t* rgb) noexcept {
  return rgb[0] == kKeyMagenta[0] && rgb[1] == kKeyMagenta[1] && rgb[2] == kKeyMagenta[2];
}

}  // namespace

void Cover(PaletteCover& cover, std::span<const PaletteRamp> ramps) {
  for (const PaletteRamp& ramp : ramps) {
    const std::size_t end = std::min<std::size_t>(
        static_cast<std::size_t>(ramp.start) + ramp.length, formats::kPaletteEntryCount);
    for (std::size_t index = ramp.start; index < end; ++index) {
      cover[index] = true;
    }
  }
}

void Cover(PaletteCover& cover, std::span<const std::uint8_t> indices) {
  for (const std::uint8_t index : indices) {
    cover[index] = true;
  }
}

int RestMeasure::ReachablePercent() const noexcept {
  if (total == 0) {
    return -1;
  }
  const long long reached = static_cast<long long>(covered) + reachable;
  return static_cast<int>((reached * kPercent + total / 2) / total);
}

RestMeasure MeasureRest(const formats::Palette& base,
                        std::span<const int, formats::kPaletteEntryCount> usage,
                        const PaletteCover& cover) {
  RestMeasure measure;
  std::array<std::array<long long, kRgbChannels>, kHueClassCount> sums{};
  // L'index zéro est le transparent : il n'est jamais peint.
  for (std::size_t index = 1; index < formats::kPaletteEntryCount; ++index) {
    const int pixels = usage[index];
    if (pixels == 0) {
      continue;
    }
    measure.total += pixels;
    if (cover[index]) {
      measure.covered += pixels;
      continue;
    }
    const auto& entry = base.entries[index];
    const std::size_t hue_class = HueClassOf(entry[0], entry[1], entry[2]);
    measure.pixels[hue_class] += pixels;
    for (std::size_t channel = 0; channel < kRgbChannels; ++channel) {
      sums[hue_class][channel] += static_cast<long long>(entry[channel]) * pixels;
    }
    if (entry[0] + entry[1] + entry[2] >= kPaletteBlackSum) {
      measure.reachable += pixels;
    }
  }
  for (std::size_t hue_class = 0; hue_class < kHueClassCount; ++hue_class) {
    const long long pixels = measure.pixels[hue_class];
    if (pixels == 0) {
      continue;
    }
    for (std::size_t channel = 0; channel < kRgbChannels; ++channel) {
      measure.mean[hue_class][channel] =
          static_cast<std::uint8_t>((sums[hue_class][channel] + pixels / 2) / pixels);
    }
  }
  return measure;
}

void ApplyRest(formats::Palette& painted, const formats::Palette& base, const PaletteCover& cover,
               const HueClassAdjusts& rest) {
  for (std::size_t index = 1; index < formats::kPaletteEntryCount; ++index) {
    const auto& entry = base.entries[index];
    if (cover[index] || IsKeyMagenta(entry.data())) {
      continue;
    }
    const RampAdjust& adjust = rest[HueClassOf(entry[0], entry[1], entry[2])];
    if (!adjust.neutral()) {
      AdjustColor(entry.data(), adjust, painted.entries[index].data());
    }
  }
}

bool RetouchAllowed(const PaletteRetouch& retouch) noexcept {
  return retouch.index != 0 && !IsKeyMagenta(retouch.rgb.data());
}

void ApplyRetouches(formats::Palette& painted, std::span<const PaletteRetouch> retouches) {
  for (const PaletteRetouch& retouch : retouches) {
    if (!RetouchAllowed(retouch)) {
      continue;
    }
    std::copy(retouch.rgb.begin(), retouch.rgb.end(), painted.entries[retouch.index].begin());
  }
}

}  // namespace ro::render
