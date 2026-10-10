#include "ro/render/post_fx_reference.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace ro::render {
namespace {

// Trois canaux en flottant, de zéro à un — le `vec3` des nuanceurs.
struct Rgb {
  float r = 0.0F;
  float g = 0.0F;
  float b = 0.0F;
};

Rgb operator+(Rgb left, Rgb right) {
  return {left.r + right.r, left.g + right.g, left.b + right.b};
}
Rgb operator-(Rgb left, Rgb right) {
  return {left.r - right.r, left.g - right.g, left.b - right.b};
}
Rgb operator*(Rgb color, float factor) {
  return {color.r * factor, color.g * factor, color.b * factor};
}
Rgb operator+(Rgb color, float amount) {
  return {color.r + amount, color.g + amount, color.b + amount};
}

float Saturate(float value) { return std::clamp(value, 0.0F, 1.0F); }
Rgb Saturate(Rgb color) { return {Saturate(color.r), Saturate(color.g), Saturate(color.b)}; }

float Luma(Rgb color) {
  return color.r * post_fx::kLuminance[0] + color.g * post_fx::kLuminance[1] +
         color.b * post_fx::kLuminance[2];
}

Rgb Mix(Rgb from, Rgb to, float amount) { return from + (to - from) * amount; }

constexpr std::size_t kChannels = 4;
constexpr float kByteMax = 255.0F;
// Un texel est centré : son milieu est à un demi-pas de son bord.
constexpr float kTexelCenter = 0.5F;
// Le gris moyen, autour duquel le contraste écarte les couleurs.
constexpr float kMidGray = 0.5F;
// Le centre de l'écran, en coordonnées de texture.
constexpr float kScreenCenter = 0.5F;
// Le milieu du bruit, que le grain recentre sur zéro.
constexpr float kNoiseMiddle = 0.5F;

// Un texel de la source, bornée aux bords comme `GL_CLAMP_TO_EDGE`.
Rgb Texel(const formats::Image& image, std::int32_t x, std::int32_t y) {
  x = std::clamp(x, 0, image.width - 1);
  y = std::clamp(y, 0, image.height - 1);
  const std::size_t at = (static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width) +
                          static_cast<std::size_t>(x)) *
                         kChannels;
  return {static_cast<float>(image.pixels[at]) / kByteMax,
          static_cast<float>(image.pixels[at + 1]) / kByteMax,
          static_cast<float>(image.pixels[at + 2]) / kByteMax};
}

// Une lecture en coordonnées de texture, bilinéaire comme `GL_LINEAR`.
Rgb Sample(const formats::Image& image, float u, float v) {
  const float x = u * static_cast<float>(image.width) - kTexelCenter;
  const float y = v * static_cast<float>(image.height) - kTexelCenter;
  const float left = std::floor(x);
  const float top = std::floor(y);
  const float across = x - left;
  const float down = y - top;
  const auto column = static_cast<std::int32_t>(left);
  const auto row = static_cast<std::int32_t>(top);
  const Rgb upper = Mix(Texel(image, column, row), Texel(image, column + 1, row), across);
  const Rgb lower = Mix(Texel(image, column, row + 1), Texel(image, column + 1, row + 1), across);
  return Mix(upper, lower, down);
}

// L'écriture dans une cible de huit bits : bornée, puis arrondie au plus près.
void Store(formats::Image& image, std::int32_t x, std::int32_t y, Rgb color) {
  const std::size_t at = (static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width) +
                          static_cast<std::size_t>(x)) *
                         kChannels;
  const Rgb bounded = Saturate(color);
  image.pixels[at] = static_cast<std::uint8_t>(std::lround(bounded.r * kByteMax));
  image.pixels[at + 1] = static_cast<std::uint8_t>(std::lround(bounded.g * kByteMax));
  image.pixels[at + 2] = static_cast<std::uint8_t>(std::lround(bounded.b * kByteMax));
  image.pixels[at + 3] = static_cast<std::uint8_t>(kByteMax);
}

formats::Image SameSize(const formats::Image& source) {
  formats::Image result;
  result.width = source.width;
  result.height = source.height;
  result.pixels.assign(source.pixels.size(), 0);
  return result;
}

// Les bornes du filtre, telles que le nuanceur compare un rang passé en flottant.
constexpr float kFilterGrayscaleBelow = 1.5F;
constexpr float kFilterSepiaBelow = 2.5F;
constexpr float kFilterNegativeBelow = 3.5F;
constexpr float kFilterAnyAbove = 0.5F;

Rgb Filtered(Rgb color, float filter) {
  if (filter > kFilterAnyAbove && filter < kFilterGrayscaleBelow) {
    const float gray = Luma(color);
    return {gray, gray, gray};
  }
  if (filter > kFilterGrayscaleBelow && filter < kFilterSepiaBelow) {
    const float gray = Luma(color);
    return Saturate(Rgb{gray * post_fx::kSepiaTint[0], gray * post_fx::kSepiaTint[1],
                        gray * post_fx::kSepiaTint[2]});
  }
  if (filter > kFilterSepiaBelow && filter < kFilterNegativeBelow) {
    return {1.0F - color.r, 1.0F - color.g, 1.0F - color.b};
  }
  if (filter > kFilterNegativeBelow) {
    return Saturate(Rgb{color.r * post_fx::kColorblindTint[0],
                        color.g * post_fx::kColorblindTint[1],
                        color.b * post_fx::kColorblindTint[2]});
  }
  return color;
}

Rgb ColorPassAt(const formats::Image& source, const ColorPassConstants& constants, float u,
                float v) {
  const float texel_u = constants.texel[0];
  const float texel_v = constants.texel[1];
  const float aberration = constants.effects[0];
  const float sharpen = constants.effects[1];
  const float seconds = constants.effects[2];

  Rgb color;
  if (aberration > 0.0F) {
    const float offset_u = (u - kScreenCenter) * aberration * post_fx::kAberrationScale;
    const float offset_v = (v - kScreenCenter) * aberration * post_fx::kAberrationScale;
    color.r = Sample(source, u + offset_u, v + offset_v).r;
    color.g = Sample(source, u, v).g;
    color.b = Sample(source, u - offset_u, v - offset_v).b;
  } else {
    color = Sample(source, u, v);
  }
  if (sharpen > 0.0F) {
    // La moyenne des quatre voisins en croix, lus dans la source NON aberrée.
    constexpr float kQuarter = 0.25F;
    const Rgb blurred = (Sample(source, u + texel_u, v) + Sample(source, u - texel_u, v) +
                         Sample(source, u, v + texel_v) + Sample(source, u, v - texel_v)) *
                        kQuarter;
    color = color + (color - blurred) * (sharpen * post_fx::kSharpenGain);
  }

  const float brightness = constants.grade[0];
  const float contrast = constants.grade[1];
  const float inverse_gamma = constants.grade[2];
  const float saturation = constants.grade[3];
  color = (color + (-kMidGray)) * contrast + kMidGray;
  color = color + brightness;
  const Rgb bounded = Saturate(color);
  color = {std::pow(bounded.r, inverse_gamma), std::pow(bounded.g, inverse_gamma),
           std::pow(bounded.b, inverse_gamma)};
  const float luma = Luma(color);
  color = Mix(Rgb{luma, luma, luma}, color, saturation);

  const float temperature = constants.tone[0];
  color.r += temperature * post_fx::kTemperatureStep;
  color.b -= temperature * post_fx::kTemperatureStep;
  color = Filtered(color, constants.tone[1]);

  const float vignette = constants.tone[2];
  if (vignette > 0.0F) {
    const float away = std::hypot(u - kScreenCenter, v - kScreenCenter);
    // `smoothstep(0.8, 0.35, d)` de HLSL : la rampe DESCEND de la zone claire
    // à la zone sombre.
    const float edge = Saturate((away - post_fx::kVignetteFull) /
                                (post_fx::kVignetteClear - post_fx::kVignetteFull));
    constexpr float kSmoothA = 3.0F;
    constexpr float kSmoothB = 2.0F;
    const float shade = edge * edge * (kSmoothA - kSmoothB * edge);
    color = color * (1.0F + (shade - 1.0F) * vignette);
  }
  const float grain = constants.tone[3];
  if (grain > 0.0F) {
    const float phase = (u * (seconds + 1.0F)) * post_fx::kNoiseAxis[0] +
                        (v * (seconds + 1.0F)) * post_fx::kNoiseAxis[1];
    const float scaled = std::sin(phase) * post_fx::kNoiseScale;
    const float noise = scaled - std::floor(scaled);
    color = color + (noise - kNoiseMiddle) * grain * post_fx::kGrainSwing;
  }
  return color;
}

Rgb FxaaAt(const formats::Image& source, float strength, float u, float v) {
  const float step_u = 1.0F / static_cast<float>(source.width);
  const float step_v = 1.0F / static_cast<float>(source.height);
  const Rgb north_west = Sample(source, u - step_u, v - step_v);
  const Rgb north_east = Sample(source, u + step_u, v - step_v);
  const Rgb south_west = Sample(source, u - step_u, v + step_v);
  const Rgb south_east = Sample(source, u + step_u, v + step_v);
  const Rgb middle = Sample(source, u, v);
  const float luma_nw = Luma(north_west);
  const float luma_ne = Luma(north_east);
  const float luma_sw = Luma(south_west);
  const float luma_se = Luma(south_east);
  const float luma_m = Luma(middle);
  const float least =
      std::min(luma_m, std::min(std::min(luma_nw, luma_ne), std::min(luma_sw, luma_se)));
  const float most =
      std::max(luma_m, std::max(std::max(luma_nw, luma_ne), std::max(luma_sw, luma_se)));
  if (most - least < std::max(post_fx::kFxaaEdgeFloor, most * post_fx::kFxaaEdgeShare)) {
    return middle;
  }
  float direction_u = -((luma_nw + luma_ne) - (luma_sw + luma_se));
  float direction_v = (luma_nw + luma_sw) - (luma_ne + luma_se);
  constexpr float kCornerMean = 0.25F;
  const float reduce =
      std::max((luma_nw + luma_ne + luma_sw + luma_se) * kCornerMean * post_fx::kFxaaReduceShare,
               post_fx::kFxaaReduceFloor);
  const float scale = 1.0F / (std::min(std::abs(direction_u), std::abs(direction_v)) + reduce);
  direction_u =
      std::clamp(direction_u * scale, -post_fx::kFxaaSpanMost, post_fx::kFxaaSpanMost) * step_u;
  direction_v =
      std::clamp(direction_v * scale, -post_fx::kFxaaSpanMost, post_fx::kFxaaSpanMost) * step_v;
  constexpr float kHalf = 0.5F;
  const Rgb near_pair = (Sample(source, u + direction_u * post_fx::kFxaaNearTap,
                                v + direction_v * post_fx::kFxaaNearTap) +
                         Sample(source, u + direction_u * post_fx::kFxaaFarTap,
                                v + direction_v * post_fx::kFxaaFarTap)) *
                        kHalf;
  constexpr float kQuarter = 0.25F;
  const Rgb wide_pair = near_pair * kHalf + (Sample(source, u - direction_u * post_fx::kFxaaEndTap,
                                                    v - direction_v * post_fx::kFxaaEndTap) +
                                             Sample(source, u + direction_u * post_fx::kFxaaEndTap,
                                                    v + direction_v * post_fx::kFxaaEndTap)) *
                                                kQuarter;
  const float luma_wide = Luma(wide_pair);
  const Rgb result = (luma_wide < least || luma_wide > most) ? near_pair : wide_pair;
  return Mix(middle, result, strength);
}

template <typename PerPixel>
formats::Image Pass(const formats::Image& source, PerPixel per_pixel) {
  formats::Image result = SameSize(source);
  for (std::int32_t y = 0; y < source.height; ++y) {
    for (std::int32_t x = 0; x < source.width; ++x) {
      const float u = (static_cast<float>(x) + kTexelCenter) / static_cast<float>(source.width);
      const float v = (static_cast<float>(y) + kTexelCenter) / static_cast<float>(source.height);
      Store(result, x, y, per_pixel(u, v));
    }
  }
  return result;
}

}  // namespace

formats::Image ColorPassReference(const formats::Image& source,
                                  const ColorPassConstants& constants) {
  if (source.empty()) {
    return {};
  }
  return Pass(source, [&](float u, float v) { return ColorPassAt(source, constants, u, v); });
}

formats::Image FxaaReference(const formats::Image& source, float strength) {
  if (source.empty()) {
    return {};
  }
  return Pass(source, [&](float u, float v) { return FxaaAt(source, strength, u, v); });
}

}  // namespace ro::render
