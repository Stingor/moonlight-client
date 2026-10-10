#include "ro/core/ink.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace ro::core {
namespace {

constexpr int kChannelMax = 255;
constexpr float kChannelScale = 255.0F;

// LA CLARTÉ DU PAPIER SOMBRE autour duquel une encre se reflète, à la façon
// d'Oklab : celle d'un gris de 34 sur 255. Un papier un peu plus clair ou plus
// sombre ne fait que rapprocher ou éloigner l'encre de lui.
constexpr float kDarkPaperLightness = 0.25F;

// La courbe du sRGB : droite près du noir, puissance au-delà.
constexpr float kLinearKnee = 0.04045F;
constexpr float kEncodedKnee = 0.0031308F;
constexpr float kLinearSlope = 12.92F;
constexpr float kCurveOffset = 0.055F;
constexpr float kCurveScale = 1.055F;
constexpr float kCurveExponent = 2.4F;

using Triple = std::array<float, 3>;
using Matrix = std::array<Triple, 3>;

// LES MATRICES D'OKLAB, telles que l'espace les définit : du sRGB linéaire aux
// trois cônes, des cônes — racine cubique prise — à la clarté et aux deux axes
// de teinte ; puis le chemin inverse.
constexpr Matrix kLinearToCone = {{{0.4122214708F, 0.5363325363F, 0.0514459929F},
                                   {0.2119034982F, 0.6806995451F, 0.1073969566F},
                                   {0.0883024619F, 0.2817188376F, 0.6299787005F}}};
constexpr Matrix kConeToLab = {{{0.2104542553F, 0.7936177850F, -0.0040720468F},
                                {1.9779984951F, -2.4285922050F, 0.4505937099F},
                                {0.0259040371F, 0.7827717662F, -0.8086757660F}}};
constexpr Matrix kLabToCone = {{{1.0F, 0.3963377774F, 0.2158037573F},
                                {1.0F, -0.1055613458F, -0.0638541728F},
                                {1.0F, -0.0894841775F, -1.2914855480F}}};
constexpr Matrix kConeToLinear = {{{4.0767416621F, -3.3077115913F, 0.2309699292F},
                                   {-1.2684380046F, 2.6097574011F, -0.3413193965F},
                                   {-0.0041960863F, -0.7034186147F, 1.7076147010F}}};

[[nodiscard]] Triple Times(const Matrix& matrix, const Triple& vector) {
  Triple product{};
  for (std::size_t row = 0; row < product.size(); ++row) {
    product[row] =
        matrix[row][0] * vector[0] + matrix[row][1] * vector[1] + matrix[row][2] * vector[2];
  }
  return product;
}

[[nodiscard]] float ToLinear(int channel) {
  const float value = static_cast<float>(channel) / kChannelScale;
  return value <= kLinearKnee ? value / kLinearSlope
                              : std::pow((value + kCurveOffset) / kCurveScale, kCurveExponent);
}

[[nodiscard]] std::uint8_t ToEncoded(float linear) {
  const float value = std::clamp(linear, 0.0F, 1.0F);
  const float encoded = value <= kEncodedKnee
                            ? value * kLinearSlope
                            : kCurveScale * std::pow(value, 1.0F / kCurveExponent) - kCurveOffset;
  return static_cast<std::uint8_t>(std::lround(encoded * kChannelScale));
}

// LA COULEUR NETTE d'un canal qu'une encre d'opacité `gap` (sur 255) a posé sur
// du blanc : celle qui, à cette opacité, le redonne.
[[nodiscard]] int Unblended(std::uint8_t channel, int gap) {
  const int clean = (kChannelMax * (channel - kChannelMax + gap) + gap / 2) / gap;
  return std::clamp(clean, 0, kChannelMax);
}

}  // namespace

Rgba8 InkForDarkPaper(Rgba8 color) {
  const int gap = kChannelMax - std::min({color.red, color.green, color.blue});
  if (gap <= 0 || color.alpha == 0) {
    return {};
  }
  Triple cone = Times(kLinearToCone,
                      {ToLinear(Unblended(color.red, gap)), ToLinear(Unblended(color.green, gap)),
                       ToLinear(Unblended(color.blue, gap))});
  for (float& value : cone) {
    value = std::cbrt(value);
  }
  Triple lab = Times(kConeToLab, cone);
  // ⚠ LE REFLET N'ASSOMBRIT JAMAIS : une encre déjà claire — un or, un orange
  // vif — se lit telle sur le sombre, et la refléter l'éteindrait.
  lab[0] = std::max(lab[0], kDarkPaperLightness + (1.0F - lab[0]) * (1.0F - kDarkPaperLightness));
  cone = Times(kLabToCone, lab);
  for (float& value : cone) {
    value = value * value * value;
  }
  const Triple turned = Times(kConeToLinear, cone);
  return {.red = ToEncoded(turned[0]),
          .green = ToEncoded(turned[1]),
          .blue = ToEncoded(turned[2]),
          .alpha = static_cast<std::uint8_t>((gap * color.alpha + kChannelMax / 2) / kChannelMax)};
}

}  // namespace ro::core
