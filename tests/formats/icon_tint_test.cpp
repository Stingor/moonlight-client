// LA TEINTE D'UNE ICÔNE : ses mots, contrat de la table, et la recoloration
// qui garde la lumière de chaque pixel.

#include "ro/formats/icon_tint.h"

#include <cstdint>
#include <set>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

using ro::formats::ApplyIconTint;
using ro::formats::IconTint;
using ro::formats::IconTintFromWord;
using ro::formats::IconTintWord;
using ro::formats::Image;
using ro::formats::kIconTintShades;

namespace {

// Une image d'une rangée : noir, gris moyen, blanc, puis un pixel effacé.
[[nodiscard]] Image Strip() {
  Image image;
  image.width = 4;
  image.height = 1;
  image.pixels = {0, 0, 0, 255, 158, 158, 158, 255, 255, 255, 255, 255, 255, 0, 255, 0};
  return image;
}

}  // namespace

TEST_CASE("Teinte d'icône : chaque mot désigne sa teinte et revient", "[formats][teinte]") {
  std::set<std::string_view> words;
  for (const auto& shade : kIconTintShades) {
    CHECK(words.insert(shade.word).second);
    CHECK(IconTintFromWord(shade.word) == shade.tint);
    CHECK(IconTintWord(shade.tint) == shade.word);
  }
  CHECK(IconTintFromWord("") == IconTint::kNone);
  CHECK(IconTintWord(IconTint::kNone).empty());
  CHECK_FALSE(IconTintFromWord("mauve").has_value());
  CHECK_FALSE(IconTintFromWord("Gold").has_value());
}

TEST_CASE("Teinte d'icône : le noir et le blanc restent, le ton moyen prend la couleur",
          "[formats][teinte]") {
  Image image = Strip();
  ApplyIconTint(image, IconTint::kRed);
  // Le noir.
  CHECK(image.pixels[0] == 0);
  CHECK(image.pixels[1] == 0);
  CHECK(image.pixels[2] == 0);
  // Le gris de luminance 0,62 prend la couleur pleine de la teinte.
  CHECK(image.pixels[4] == 214);
  CHECK(image.pixels[5] == 46);
  CHECK(image.pixels[6] == 46);
  CHECK(image.pixels[7] == 255);
  // Le blanc.
  CHECK(image.pixels[8] == 255);
  CHECK(image.pixels[9] == 255);
  CHECK(image.pixels[10] == 255);
  // Le pixel effacé garde son magenta et son alpha nul.
  CHECK(image.pixels[12] == 255);
  CHECK(image.pixels[13] == 0);
  CHECK(image.pixels[14] == 255);
  CHECK(image.pixels[15] == 0);
}

TEST_CASE("Teinte d'icône : sans teinte, rien ne change", "[formats][teinte]") {
  Image image = Strip();
  const Image before = image;
  ApplyIconTint(image, IconTint::kNone);
  CHECK(image.pixels == before.pixels);
}
