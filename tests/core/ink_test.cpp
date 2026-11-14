// L'ENCRE D'UNE COULEUR PEINTE SUR DU BLANC, retournée pour un papier sombre.

#include "ro/core/ink.h"

#include <cstdlib>

#include <catch2/catch_test_macros.hpp>

using ro::core::InkForDarkPaper;
using ro::core::Rgba8;

namespace {

// Deux couleurs égales à un cran près sur chaque canal : l'aller et retour par
// Oklab arrondit.
[[nodiscard]] bool Near(Rgba8 left, Rgba8 right) {
  const auto close = [](int one, int other) { return std::abs(one - other) <= 1; };
  return close(left.red, right.red) && close(left.green, right.green) &&
         close(left.blue, right.blue) && left.alpha == right.alpha;
}

}  // namespace

TEST_CASE("Encre : le blanc du papier devient transparent", "[core][encre]") {
  CHECK(InkForDarkPaper({.red = 255, .green = 255, .blue = 255, .alpha = 255}).alpha == 0);
}

TEST_CASE("Encre : le noir devient du blanc, opaque", "[core][encre]") {
  CHECK(InkForDarkPaper({.red = 0, .green = 0, .blue = 0, .alpha = 255}) ==
        Rgba8{.red = 255, .green = 255, .blue = 255, .alpha = 255});
}

TEST_CASE("Encre : un gris devient du blanc, à l'opacité de sa blancheur manquante",
          "[core][encre]") {
  // Un filet gris sur du blanc est un trait noir en partie transparent : sur le
  // sombre, un trait blanc de même transparence.
  CHECK(InkForDarkPaper({.red = 128, .green = 128, .blue = 128, .alpha = 255}) ==
        Rgba8{.red = 255, .green = 255, .blue = 255, .alpha = 127});
}

TEST_CASE("Encre : l'opacité de la couleur multiplie celle de l'encre", "[core][encre]") {
  CHECK(InkForDarkPaper({.red = 0, .green = 0, .blue = 0, .alpha = 128}) ==
        Rgba8{.red = 255, .green = 255, .blue = 255, .alpha = 128});
}

TEST_CASE("Encre : un bleu nuit devient un bleu clair, de même teinte", "[core][encre]") {
  const Rgba8 turned = InkForDarkPaper({.red = 29, .green = 50, .blue = 85, .alpha = 255});
  CHECK(turned.blue > turned.green);
  CHECK(turned.green > turned.red);
  CHECK(turned.red > 128);
  CHECK(turned.alpha == 226);
}

TEST_CASE("Encre : une encre déjà claire reste telle", "[core][encre]") {
  // Un or vif se lit sur le sombre : son reflet l'éteindrait.
  const Rgba8 gold{.red = 255, .green = 170, .blue = 0, .alpha = 255};
  CHECK(Near(InkForDarkPaper(gold), gold));
}
