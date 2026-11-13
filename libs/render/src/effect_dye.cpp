#include "ro/render/effect_dye.h"

#include <algorithm>
#include <cmath>

namespace ro::render {
namespace {

constexpr float kChannelMax = 255.0F;
constexpr float kPercent = 100.0F;
constexpr float kFullTurnDegrees = 360.0F;
constexpr float kSextantDegrees = 60.0F;
constexpr float kSextants = 6.0F;

// Les facteurs Direct3D qui gardent toute la destination : « un », et l'alpha
// de la destination, qui vaut un sur la cible opaque.
constexpr std::int32_t kD3dZero = 1;
constexpr std::int32_t kD3dOne = 2;
constexpr std::int32_t kD3dDestinationAlpha = 7;

struct Hsl {
  float hue_degrees = 0.0F;
  float saturation = 0.0F;
  float lightness = 0.0F;
};

// ⚠ LE NUANCEUR EN EST LA COPIE, ligne pour ligne : voir `kFragmentShader`
// d'`effect_renderer.cpp`.
Hsl ToHsl(float red, float green, float blue) {
  const float high = std::max({red, green, blue});
  const float low = std::min({red, green, blue});
  Hsl hsl;
  hsl.lightness = (high + low) * 0.5F;
  const float spread = high - low;
  if (spread <= 0.0F) {
    return hsl;
  }
  hsl.saturation = hsl.lightness > 0.5F ? spread / (2.0F - high - low) : spread / (high + low);
  float sextant = 0.0F;
  if (high == red) {
    sextant = std::fmod((green - blue) / spread + kSextants, kSextants);
  } else if (high == green) {
    sextant = (blue - red) / spread + 2.0F;
  } else {
    sextant = (red - green) / spread + 4.0F;
  }
  hsl.hue_degrees = sextant * kSextantDegrees;
  return hsl;
}

float HueChannel(float low, float high, float hue_degrees) {
  float hue = std::fmod(hue_degrees, kFullTurnDegrees);
  if (hue < 0.0F) {
    hue += kFullTurnDegrees;
  }
  const float sextant = hue / kSextantDegrees;
  if (sextant < 1.0F) {
    return low + (high - low) * sextant;
  }
  if (sextant < 3.0F) {
    return high;
  }
  if (sextant < 4.0F) {
    return low + (high - low) * (4.0F - sextant);
  }
  return low;
}

std::uint8_t ToByte(float unit) {
  return static_cast<std::uint8_t>(std::lround(std::clamp(unit, 0.0F, 1.0F) * kChannelMax));
}

}  // namespace

EffectDye MakeEffectDye(int hue_degrees, int saturation_percent, int value_percent, bool darken) {
  const int hue = std::clamp(hue_degrees, -kEffectDyeHueMost, kEffectDyeHueMost);
  const int saturation =
      std::clamp(saturation_percent, -kEffectDyePercentMost, kEffectDyePercentMost);
  const int value = std::clamp(value_percent, -kEffectDyePercentMost, kEffectDyePercentMost);
  return {.hue_degrees = static_cast<float>(hue),
          .saturation_scale = 1.0F + static_cast<float>(saturation) / kPercent,
          .lightness_scale = 1.0F + static_cast<float>(value) / kPercent,
          .darken = darken};
}

bool IsNeutralEffectDye(const EffectDye& dye) { return dye == EffectDye{}; }

std::array<std::uint8_t, 4> DyeEffectTexel(std::array<std::uint8_t, 4> rgba, const EffectDye& dye) {
  if (IsNeutralEffectDye(dye)) {
    return rgba;
  }
  float red = static_cast<float>(rgba[0]) / kChannelMax;
  float green = static_cast<float>(rgba[1]) / kChannelMax;
  float blue = static_cast<float>(rgba[2]) / kChannelMax;
  float alpha = static_cast<float>(rgba[3]) / kChannelMax;
  if (dye.darken) {
    const float high = std::max({red, green, blue});
    if (high <= 0.0F) {
      return {0, 0, 0, 0};
    }
    alpha *= high;
    red /= high;
    green /= high;
    blue /= high;
  }
  Hsl hsl = ToHsl(red, green, blue);
  hsl.hue_degrees += dye.hue_degrees;
  hsl.saturation = std::clamp(hsl.saturation * dye.saturation_scale, 0.0F, 1.0F);
  hsl.lightness = std::clamp(hsl.lightness * dye.lightness_scale, 0.0F, 1.0F);
  if (hsl.saturation <= 0.0F) {
    const std::uint8_t gray = ToByte(hsl.lightness);
    return {gray, gray, gray, ToByte(alpha)};
  }
  const float high = hsl.lightness <= 0.5F
                         ? hsl.lightness * (1.0F + hsl.saturation)
                         : hsl.lightness + hsl.saturation - hsl.lightness * hsl.saturation;
  const float low = 2.0F * hsl.lightness - high;
  constexpr float kThirdTurn = kFullTurnDegrees / 3.0F;
  return {ToByte(HueChannel(low, high, hsl.hue_degrees + kThirdTurn)),
          ToByte(HueChannel(low, high, hsl.hue_degrees)),
          ToByte(HueChannel(low, high, hsl.hue_degrees - kThirdTurn)), ToByte(alpha)};
}

bool EffectBlendAdds(std::int32_t blend_source, std::int32_t blend_destination) {
  return blend_source != kD3dZero &&
         (blend_destination == kD3dOne || blend_destination == kD3dDestinationAlpha);
}

}  // namespace ro::render
