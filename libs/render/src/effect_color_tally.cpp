#include "ro/render/effect_color_tally.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace ro::render {
namespace {

constexpr std::size_t kRgbaBytes = 4;
constexpr double kChannelMax = 255.0;

}  // namespace

const EffectColorTally::ImageMean& EffectColorTally::MeanOf(const formats::Image& image) {
  if (const auto known = means_.find(&image); known != means_.end()) {
    return known->second;
  }
  ImageMean mean;
  const std::vector<std::uint8_t>& texels = image.pixels;
  for (std::size_t at = 0; at + kRgbaBytes <= texels.size(); at += kRgbaBytes) {
    const double alpha = texels[at + 3] / kChannelMax;
    const double bright = std::max({texels[at], texels[at + 1], texels[at + 2]}) / kChannelMax;
    const double weight = alpha * bright;
    for (std::size_t channel = 0; channel < mean.sum.size(); ++channel) {
      mean.sum[channel] += weight * texels[at + channel];
    }
    mean.weight += weight;
  }
  return means_.emplace(&image, mean).first->second;
}

void EffectColorTally::Add(const formats::Image& image, std::array<std::uint8_t, 4> tint) {
  const ImageMean& mean = MeanOf(image);
  const double opacity = tint[3] / kChannelMax;
  for (std::size_t channel = 0; channel < sum_.size(); ++channel) {
    sum_[channel] += opacity * mean.sum[channel] * tint[channel] / kChannelMax;
  }
  weight_ += opacity * mean.weight;
}

void EffectColorTally::Add(const formats::Image& image, const std::array<float, 4>& tint) {
  // ⚠ LA COULEUR D'UNE CLÉ EST EN ÉCHELLE D'OCTET, et son opacité n'y entre
  // pas : une clé éteinte reste la couleur que l'effet prend en s'allumant.
  const ImageMean& mean = MeanOf(image);
  for (std::size_t channel = 0; channel < sum_.size(); ++channel) {
    sum_[channel] += mean.sum[channel] * static_cast<double>(tint[channel]) / kChannelMax;
  }
  weight_ += mean.weight;
}

std::optional<std::array<std::uint8_t, 3>> EffectColorTally::Color() const {
  const double high = std::max({sum_[0], sum_[1], sum_[2]});
  if (weight_ <= 0.0 || high <= 0.0) {
    return std::nullopt;
  }
  return std::array<std::uint8_t, 3>{
      static_cast<std::uint8_t>(std::lround(sum_[0] / high * kChannelMax)),
      static_cast<std::uint8_t>(std::lround(sum_[1] / high * kChannelMax)),
      static_cast<std::uint8_t>(std::lround(sum_[2] / high * kChannelMax))};
}

void EffectColorTally::Reset(const EffectDye& new_probe) {
  probe = new_probe;
  means_.clear();
  sum_ = {};
  weight_ = 0.0;
  images_counted_ = 0;
}

}  // namespace ro::render
