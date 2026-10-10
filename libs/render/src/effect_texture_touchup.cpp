#include "ro/render/effect_texture_touchup.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace ro::render {
namespace {

// Les quatre octets d'un pixel, et les trois premiers qui portent la couleur.
constexpr std::size_t kChannelsPerPixel = 4;
constexpr std::size_t kColourChannels = 3;

// Il faut un texel de bord ET un texel d'intérieur pour recopier l'un sur
// l'autre : en deçà de trois de côté, il n'y a pas d'intérieur.
constexpr std::int32_t kSmallestFramedSide = 3;

}  // namespace

formats::Image WithoutBlackPedestal(const formats::Image& image) {
  formats::Image sans = image;
  // Les trois premiers octets de chaque pixel ; le quatrième est l'alpha, que
  // l'on ne touche pas — un effet additif ne le lit que pour doser son apport.
  for (std::size_t pixel = 0; pixel + kChannelsPerPixel <= sans.pixels.size();
       pixel += kChannelsPerPixel) {
    for (std::size_t channel = 0; channel < kColourChannels; ++channel) {
      std::uint8_t& value = sans.pixels[pixel + channel];
      value = value > kEffectBlackPedestal ? static_cast<std::uint8_t>(value - kEffectBlackPedestal)
                                           : 0;
    }
  }
  return sans;
}

formats::Image WithoutLeftoverFrame(const formats::Image& image) {
  formats::Image sans = image;
  if (image.width < kSmallestFramedSide || image.height < kSmallestFramedSide) {
    return sans;
  }
  const std::size_t needed = static_cast<std::size_t>(image.width) *
                             static_cast<std::size_t>(image.height) * kChannelsPerPixel;
  if (sans.pixels.size() < needed) {
    return sans;
  }

  // ⚠ ON LIT LA SOURCE, JAMAIS LA COPIE EN COURS. Un coin prend le texel en
  // diagonale, et le lire dans `sans` le prendrait déjà réécrit.
  const auto at = [&image](std::int32_t x, std::int32_t y) {
    return (static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width) +
            static_cast<std::size_t>(x)) *
           kChannelsPerPixel;
  };
  for (std::int32_t y = 0; y < image.height; ++y) {
    for (std::int32_t x = 0; x < image.width; ++x) {
      if (x != 0 && y != 0 && x != image.width - 1 && y != image.height - 1) {
        continue;
      }
      const std::int32_t inner_x = std::clamp(x, 1, image.width - 2);
      const std::int32_t inner_y = std::clamp(y, 1, image.height - 2);
      const std::size_t destination = at(x, y);
      const std::size_t source = at(inner_x, inner_y);
      // LA COULEUR se recopie telle quelle.
      for (std::size_t channel = 0; channel < kColourChannels; ++channel) {
        sans.pixels[destination + channel] = image.pixels[source + channel];
      }
      // 🔴 L'ALPHA NE PEUT QUE DESCENDRE : le bord prend le plus TRANSPARENT des
      // deux. Un cadre d'outil AJOUTE de l'opacité au bord — celui de
      // `dro_glow_sub_bottom_01.tga`, au pied des piliers de butin, vaut 11 à
      // 18 autour d'un pourtour à zéro, et peint un carré gris ; un bord à
      // alpha nul, lui, est de la DONNÉE — la garde de `alpha_down.tga`, sous le
      // voisin à 235 du pied des colonnes de Safety Wall. Le minimum retire le
      // premier et garde la seconde. Voir l'ADR 0049.
      sans.pixels[destination + kColourChannels] = std::min(
          image.pixels[destination + kColourChannels], image.pixels[source + kColourChannels]);
    }
  }
  return sans;
}

}  // namespace ro::render
