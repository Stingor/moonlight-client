#include "ro/render/image_diff.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace ro::render {
namespace {

// Le pire écart des trois canaux de couleur d'un pixel.
//
// L'alpha est IGNORÉ : une capture d'écran est opaque, un rendu hors écran
// aussi, et les comparer ferait du bruit sur un canal sans information.
int ChannelGap(const formats::Image& left, const formats::Image& right, std::size_t offset) {
  int worst = 0;
  for (std::size_t channel = 0; channel < 3; ++channel) {
    const int gap = std::abs(static_cast<int>(left.pixels[offset + channel]) -
                             static_cast<int>(right.pixels[offset + channel]));
    worst = std::max(worst, gap);
  }
  return worst;
}

}  // namespace

std::optional<ImageComparison> CompareImages(const formats::Image& left,
                                             const formats::Image& right,
                                             formats::Image* difference) {
  if (left.width != right.width || left.height != right.height) {
    return std::nullopt;
  }
  if (left.width <= 0 || left.height <= 0) {
    return std::nullopt;
  }
  const std::size_t needed =
      static_cast<std::size_t>(left.width) * static_cast<std::size_t>(left.height) * 4;
  if (left.pixels.size() < needed || right.pixels.size() < needed) {
    return std::nullopt;
  }

  if (difference != nullptr) {
    difference->width = left.width;
    difference->height = left.height;
    difference->pixels.assign(needed, 255);
  }

  ImageComparison result;
  const auto width = static_cast<std::size_t>(left.width);
  const auto height = static_cast<std::size_t>(left.height);
  double total = 0.0;
  std::array<double, kRegionsPerSide * kRegionsPerSide> region_total{};
  std::array<std::size_t, kRegionsPerSide * kRegionsPerSide> region_count{};

  for (std::size_t y = 0; y < height; ++y) {
    for (std::size_t x = 0; x < width; ++x) {
      const std::size_t offset = (y * width + x) * 4;
      const int gap = ChannelGap(left, right, offset);
      total += gap;
      result.worst_gap = std::max(result.worst_gap, gap);
      result.compared += 1;
      if (gap >= kNoticeableChannelGap) {
        result.differing += 1;
      }

      const std::size_t region =
          (y * kRegionsPerSide / height) * kRegionsPerSide + (x * kRegionsPerSide / width);
      region_total[region] += gap;
      region_count[region] += 1;

      if (difference != nullptr) {
        const auto shade = static_cast<std::uint8_t>(std::min(gap * kDifferenceGain, 255));
        difference->pixels[offset] = shade;
        difference->pixels[offset + 1] = shade;
        difference->pixels[offset + 2] = shade;
      }
    }
  }

  result.mean_gap = total / static_cast<double>(result.compared);
  for (std::size_t region = 0; region < region_total.size(); ++region) {
    if (region_count[region] > 0) {
      result.region_mean[region] = region_total[region] / static_cast<double>(region_count[region]);
    }
  }
  return result;
}

}  // namespace ro::render
