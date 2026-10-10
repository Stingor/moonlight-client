#pragma once

#include <cstdint>

// LA COULEUR TELLE QUE LE SERVEUR LA RANGE.
//
// En-tête PRIVÉ : seule cette bibliothèque lit une couleur sur le fil.
namespace ro::net {

// 🔴 LE SERVEUR RANGE SA COULEUR EN BGR, et ce n'est pas une étourderie :
// `channel.cpp` convertit « RGB to BGR » en lisant `channels.conf`, parce que
// c'est la forme que Direct3D attend, et les messages colorés suivent la même
// forme. La remettre à l'endroit se fait ICI, une fois, et non à chaque
// endroit qui peint — un canal bleu qui paraîtrait rouge est le genre de défaut
// qu'on attribue à la configuration du serveur.
[[nodiscard]] constexpr std::uint32_t RgbFromBgr(std::uint32_t bgr) noexcept {
  const std::uint32_t blue = (bgr >> 16) & 0xFFU;
  const std::uint32_t green = (bgr >> 8) & 0xFFU;
  const std::uint32_t red = bgr & 0xFFU;
  return (red << 16) | (green << 8) | blue;
}

}  // namespace ro::net
