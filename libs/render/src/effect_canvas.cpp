#include "ro/render/effect_canvas.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>

namespace ro::render {
namespace {

constexpr std::size_t kChannels = 4;
constexpr float kMaxChannel = 255.0F;
// Le seuil d'alpha du nuanceur des effets : un sur deux cent cinquante-cinq.
constexpr float kAlphaReference = 1.0F / kMaxChannel;

// Les facteurs de mélange de Direct3D, dans l'ordre de D3DBLEND.
constexpr std::int32_t kBlendZero = 1;
constexpr std::int32_t kBlendOne = 2;
constexpr std::int32_t kBlendSourceColor = 3;
constexpr std::int32_t kBlendInverseSourceColor = 4;
constexpr std::int32_t kBlendSourceAlpha = 5;
constexpr std::int32_t kBlendInverseSourceAlpha = 6;
constexpr std::int32_t kBlendDestinationAlpha = 7;
constexpr std::int32_t kBlendInverseDestinationAlpha = 8;
constexpr std::int32_t kBlendDestinationColor = 9;
constexpr std::int32_t kBlendInverseDestinationColor = 10;
constexpr std::int32_t kBlendSourceAlphaSaturate = 11;

using Color = std::array<float, kChannels>;

// Le facteur d'un canal, de zéro à un.
//
// ⚠ Le fond des effets du jeu a une opacité PLEINE : le mélange dominant des
// fichiers — source par son alpha, destination par l'alpha du fond — n'est
// additif que sous cette condition. Un fond transparent l'éteindrait.
float Factor(std::int32_t blend, std::size_t channel, const Color& source,
             const Color& destination) {
  switch (blend) {
    case kBlendZero:
      return 0.0F;
    case kBlendOne:
      return 1.0F;
    case kBlendSourceColor:
      return source[channel];
    case kBlendInverseSourceColor:
      return 1.0F - source[channel];
    case kBlendSourceAlpha:
      return source[3];
    case kBlendInverseSourceAlpha:
      return 1.0F - source[3];
    case kBlendDestinationAlpha:
      return destination[3];
    case kBlendInverseDestinationAlpha:
      return 1.0F - destination[3];
    case kBlendDestinationColor:
      return destination[channel];
    case kBlendInverseDestinationColor:
      return 1.0F - destination[channel];
    case kBlendSourceAlphaSaturate:
      return std::min(source[3], 1.0F - destination[3]);
    default:
      // Zéro n'est pas un facteur de Direct3D : c'est ce que les clés
      // d'interpolation laissent, et rien ne se peint alors.
      return 0.0F;
  }
}

// Le rectangle qu'un quad couvre, en pixels entiers de l'image.
struct PixelBox {
  std::int32_t min_x = 0;
  std::int32_t min_y = 0;
  std::int32_t max_x = 0;
  std::int32_t max_y = 0;
};

PixelBox BoxOf(const EffectQuad& quad, glm::vec2 origin, std::int32_t width, std::int32_t height) {
  glm::vec2 low = quad.corners[0] + origin;
  glm::vec2 high = low;
  for (const glm::vec2& corner : quad.corners) {
    low = glm::min(low, corner + origin);
    high = glm::max(high, corner + origin);
  }
  PixelBox box;
  box.min_x = std::max<std::int32_t>(0, static_cast<std::int32_t>(std::floor(low.x)));
  box.min_y = std::max<std::int32_t>(0, static_cast<std::int32_t>(std::floor(low.y)));
  box.max_x = std::min<std::int32_t>(width, static_cast<std::int32_t>(std::ceil(high.x)));
  box.max_y = std::min<std::int32_t>(height, static_cast<std::int32_t>(std::ceil(high.y)));
  return box;
}

// Les coordonnées d'un point DANS le quad, de zéro à un sur chaque bord, ou
// rien s'il tombe dehors.
//
// Le quad est peint comme deux triangles : c'est ce que fait la carte
// graphique, et un quad d'effet n'est pas toujours un rectangle — il se
// déforme d'une clé à l'autre.
std::optional<glm::vec2> InsideQuad(const std::array<glm::vec2, 4>& corners, glm::vec2 point) {
  // Les deux triangles du quad : (0,1,2) et (0,2,3). Les coordonnées locales
  // s'expriment dans le repère (haut-gauche → haut-droit, haut-gauche →
  // bas-gauche) reconstruit sur chacun.
  constexpr std::array<std::array<std::size_t, 3>, 2> kTriangles = {{{0, 1, 2}, {0, 2, 3}}};
  constexpr std::array<std::array<glm::vec2, 3>, 2> kLocal = {
      {{glm::vec2{0, 0}, glm::vec2{1, 0}, glm::vec2{1, 1}},
       {glm::vec2{0, 0}, glm::vec2{1, 1}, glm::vec2{0, 1}}}};

  for (std::size_t triangle = 0; triangle < kTriangles.size(); ++triangle) {
    const glm::vec2& a = corners[kTriangles[triangle][0]];
    const glm::vec2& b = corners[kTriangles[triangle][1]];
    const glm::vec2& c = corners[kTriangles[triangle][2]];
    const glm::vec2 edge_ab = b - a;
    const glm::vec2 edge_ac = c - a;
    const float area = edge_ab.x * edge_ac.y - edge_ab.y * edge_ac.x;
    if (std::abs(area) < 1e-6F) {
      continue;
    }
    const glm::vec2 offset = point - a;
    const float weight_b = (offset.x * edge_ac.y - offset.y * edge_ac.x) / area;
    const float weight_c = (edge_ab.x * offset.y - edge_ab.y * offset.x) / area;
    if (weight_b < 0.0F || weight_c < 0.0F || weight_b + weight_c > 1.0F) {
      continue;
    }
    const glm::vec2 local = kLocal[triangle][0] +
                            weight_b * (kLocal[triangle][1] - kLocal[triangle][0]) +
                            weight_c * (kLocal[triangle][2] - kLocal[triangle][0]);
    return local;
  }
  return std::nullopt;
}

// Le texel au plus proche voisin, comme un sprite à l'écran.
Color Sample(const formats::Image& texture, glm::vec2 uv) {
  const auto column = std::clamp(
      static_cast<std::int32_t>(uv.x * static_cast<float>(texture.width)), 0, texture.width - 1);
  const auto row = std::clamp(static_cast<std::int32_t>(uv.y * static_cast<float>(texture.height)),
                              0, texture.height - 1);
  const std::size_t at = (static_cast<std::size_t>(row) * static_cast<std::size_t>(texture.width) +
                          static_cast<std::size_t>(column)) *
                         kChannels;
  Color color{};
  for (std::size_t channel = 0; channel < kChannels; ++channel) {
    color[channel] = static_cast<float>(texture.pixels[at + channel]) / kMaxChannel;
  }
  return color;
}

}  // namespace

EffectCanvas::EffectCanvas(std::int32_t width, std::int32_t height) {
  image_.width = std::max(width, 0);
  image_.height = std::max(height, 0);
  image_.pixels.assign(
      static_cast<std::size_t>(image_.width) * static_cast<std::size_t>(image_.height) * kChannels,
      0);
  // Noire et OPAQUE : voir l'en-tête. Sans cette opacité, le mélange dominant
  // des fichiers cesse d'être une addition.
  for (std::size_t at = 3; at < image_.pixels.size(); at += kChannels) {
    image_.pixels[at] = 255;
  }
}

void EffectCanvas::Paint(std::span<const EffectQuad> quads,
                         std::span<const formats::Image> textures, glm::vec2 origin) {
  for (const EffectQuad& quad : quads) {
    if (quad.texture >= textures.size() || textures[quad.texture].empty()) {
      continue;
    }
    const formats::Image& texture = textures[quad.texture];
    const PixelBox box = BoxOf(quad, origin, image_.width, image_.height);

    for (std::int32_t row = box.min_y; row < box.max_y; ++row) {
      for (std::int32_t column = box.min_x; column < box.max_x; ++column) {
        // Le CENTRE du pixel : prendre son coin déplacerait l'image d'un demi.
        const glm::vec2 point{static_cast<float>(column) + 0.5F - origin.x,
                              static_cast<float>(row) + 0.5F - origin.y};
        const std::optional<glm::vec2> local = InsideQuad(quad.corners, point);
        if (!local.has_value()) {
          continue;
        }
        const glm::vec2 uv = quad.uv_min + *local * (quad.uv_max - quad.uv_min);
        Color source = Sample(texture, uv);
        for (std::size_t channel = 0; channel < kChannels; ++channel) {
          source[channel] *= static_cast<float>(quad.tint[channel]) / kMaxChannel;
        }
        // 🔴 LE SEUIL DU NUANCEUR, sur l'alpha FINAL, texture et teinte
        // multipliées : en deçà, rien ne se peint. Sans lui, un mélange qui prend
        // la source SANS son alpha — `ONE`, `SRCCOLOR` — peindrait la couleur
        // que garde un texel transparent : le magenta de la clé, en carré plein
        // autour de l'image.
        if (source[3] < kAlphaReference) {
          continue;
        }

        const std::size_t at =
            (static_cast<std::size_t>(row) * static_cast<std::size_t>(image_.width) +
             static_cast<std::size_t>(column)) *
            kChannels;
        Color destination{};
        for (std::size_t channel = 0; channel < kChannels; ++channel) {
          destination[channel] = static_cast<float>(image_.pixels[at + channel]) / kMaxChannel;
        }
        for (std::size_t channel = 0; channel < kChannels; ++channel) {
          const float mixed =
              source[channel] * Factor(quad.blend_source, channel, source, destination) +
              destination[channel] * Factor(quad.blend_destination, channel, source, destination);
          image_.pixels[at + channel] =
              static_cast<std::uint8_t>(std::clamp(mixed, 0.0F, 1.0F) * kMaxChannel);
        }
      }
    }
  }
}

}  // namespace ro::render
