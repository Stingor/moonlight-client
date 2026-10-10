#pragma once

#include <cstdint>

namespace ro::core {

// UNE COULEUR DE HUIT BITS PAR CANAL, opacité comprise.
struct Rgba8 {
  std::uint8_t red = 0;
  std::uint8_t green = 0;
  std::uint8_t blue = 0;
  std::uint8_t alpha = 0;

  friend bool operator==(const Rgba8&, const Rgba8&) = default;
};

// CE QUI A ÉTÉ PEINT SUR DU BLANC, retourné pour un papier sombre.
//
// La couleur se lit comme une ENCRE posée sur du blanc : son opacité est sa
// blancheur manquante — 255 moins son plus bas canal —, et sa teinte nette celle
// qui, à cette opacité sur du blanc, la redonne. L'encre se reflète ensuite en
// clarté autour d'un papier sombre : ce qui s'écartait du blanc s'écarte
// d'autant du papier, vers le clair, et garde sa teinte. Une encre déjà plus
// claire que son reflet reste telle.
//
// Le blanc devient donc transparent, le noir blanc, un filet gris un filet
// clair, un bleu un bleu qui se lit sur le sombre. L'opacité rendue est celle
// de l'encre, multipliée par celle de la couleur ; posée sur le papier, elle
// le laisse voir là où l'original laissait voir le blanc.
[[nodiscard]] Rgba8 InkForDarkPaper(Rgba8 color);

}  // namespace ro::core
