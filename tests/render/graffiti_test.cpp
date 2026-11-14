// LE GRAFFITI : la toile que compose un message, et la grille qui la couche au
// sol.

#include "ro/render/graffiti.h"

#include <cstddef>
#include <cstdint>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Catch::Approx;

namespace {

constexpr std::int32_t kAlphabetSide = 256;
constexpr std::int32_t kGlyph = 32;
constexpr std::size_t kChannels = 4;

// UNE PLANCHE SYNTHÉTIQUE : chaque glyphe peint son propre rang dans le rouge,
// opaque.
ro::formats::Image NumberedAlphabet() {
  ro::formats::Image alphabet;
  alphabet.width = kAlphabetSide;
  alphabet.height = kAlphabetSide;
  alphabet.pixels.resize(static_cast<std::size_t>(kAlphabetSide * kAlphabetSide) * kChannels);
  for (std::int32_t y = 0; y < kAlphabetSide; ++y) {
    for (std::int32_t x = 0; x < kAlphabetSide; ++x) {
      const std::size_t at = static_cast<std::size_t>(y * kAlphabetSide + x) * kChannels;
      alphabet.pixels[at] = static_cast<std::uint8_t>((y / kGlyph) * 8 + x / kGlyph);
      alphabet.pixels[at + 3] = 255;
    }
  }
  return alphabet;
}

// Le rang du glyphe peint en ce pixel de la toile, ou -1 s'il est vide.
int GlyphAt(const ro::formats::Image& canvas, std::int32_t x, std::int32_t y) {
  const std::size_t at = static_cast<std::size_t>(y * canvas.width + x) * kChannels;
  return canvas.pixels[at + 3] == 0 ? -1 : canvas.pixels[at];
}

// La première ligne d'un message de cette longueur.
std::int32_t TopLine(std::int32_t length) { return 36 * ((144 - length) / 24); }

}  // namespace

TEST_CASE("Graffiti : chaque signe prend son glyphe et sa largeur", "[render][graffiti]") {
  const ro::formats::Image alphabet = NumberedAlphabet();
  const ro::formats::Image canvas = ro::render::ComposeGraffitiCanvas(alphabet, "Ab7?#!");
  REQUIRE(canvas.width == 384);
  REQUIRE(canvas.height == 384);
  const std::int32_t y = TopLine(6) + 1;
  CHECK(GlyphAt(canvas, 0, y) == 0);
  CHECK(GlyphAt(canvas, 32, y) == 27);
  CHECK(GlyphAt(canvas, 64, y) == 61);
  // « ? » est étroit : seize pixels.
  CHECK(GlyphAt(canvas, 96, y) == 53);
  CHECK(GlyphAt(canvas, 111, y) == 53);
  // Un signe que la planche n'a pas laisse seize pixels vides.
  CHECK(GlyphAt(canvas, 112, y) == -1);
  CHECK(GlyphAt(canvas, 127, y) == -1);
  // ⚠ « ! » DESSINE LE ZÉRO DE LA PLANCHE.
  CHECK(GlyphAt(canvas, 128, y) == 54);
  // Au-dessus de la première ligne, rien.
  CHECK(GlyphAt(canvas, 0, y - 2) == -1);
}

TEST_CASE("Graffiti : la ligne se coupe à trois cent cinquante-deux pixels", "[render][graffiti]") {
  const ro::formats::Image alphabet = NumberedAlphabet();
  // Onze lettres : la onzième finirait pile à la limite, et passe à la ligne.
  const ro::formats::Image canvas = ro::render::ComposeGraffitiCanvas(alphabet, "AAAAAAAAAAB");
  const std::int32_t y = TopLine(11) + 1;
  CHECK(GlyphAt(canvas, 320, y) == -1);
  CHECK(GlyphAt(canvas, 0, y + 36) == 1);
}

TEST_CASE("Graffiti : un mot qui ne tient plus passe à la ligne avec son espace",
          "[render][graffiti]") {
  const ro::formats::Image alphabet = NumberedAlphabet();
  // Huit lettres, puis un mot de trois : 256 + 96 atteint la limite.
  const ro::formats::Image canvas = ro::render::ComposeGraffitiCanvas(alphabet, "AAAAAAAA BCD");
  const std::int32_t y = TopLine(12) + 1;
  // L'espace ouvre la ligne suivante de la largeur du signe d'avant.
  CHECK(GlyphAt(canvas, 0, y + 36) == -1);
  CHECK(GlyphAt(canvas, 32, y + 36) == 1);
  CHECK(GlyphAt(canvas, 64, y + 36) == 2);
}

TEST_CASE("Graffiti : la grille couvre dix-huit cases et tourne avec le regard",
          "[render][graffiti]") {
  const auto cells = ro::render::ComposeGraffitiCells(0.0F);
  REQUIRE(cells.size() == 324);
  CHECK(cells.front().cell_offset_x == -9);
  CHECK(cells.front().cell_offset_y == -9);
  CHECK(cells.back().cell_offset_x == 8);
  CHECK(cells.back().cell_offset_y == 8);
  // Le coin bas de la grille prend le bord droit de la toile, en haut.
  CHECK(cells.front().texture_uv[0].x == Approx(1.0F));
  CHECK(cells.front().texture_uv[0].y == Approx(0.0F));
  // L'abscisse de la grille court à rebours de celle de la toile.
  CHECK(cells.front().texture_uv[1].x == Approx(1.0F - 1.0F / 18.0F));
  // Un quart de tour : le même coin passe en bas de la toile.
  const auto turned = ro::render::ComposeGraffitiCells(90.0F);
  CHECK(turned.front().texture_uv[0].x == Approx(1.0F).margin(1.0e-4F));
  CHECK(turned.front().texture_uv[0].y == Approx(1.0F).margin(1.0e-4F));
}

TEST_CASE("Flag Graffiti : l'emblème grandit avec le niveau, le haut au nord",
          "[render][graffiti]") {
  CHECK(ro::render::FlagGraffitiSide(1) == 3);
  CHECK(ro::render::FlagGraffitiSide(5) == 11);
  // Un niveau nul compte pour un.
  CHECK(ro::render::FlagGraffitiSide(0) == 3);
  const auto cells = ro::render::ComposeFlagGraffitiCells(5);
  REQUIRE(cells.size() == 121);
  CHECK(cells.front().cell_offset_x == -5);
  CHECK(cells.front().cell_offset_y == -5);
  CHECK(cells.back().cell_offset_x == 5);
  CHECK(cells.back().cell_offset_y == 5);
  // Le coin sud-ouest prend le BAS de l'image, le nord-est son HAUT.
  CHECK(cells.front().texture_uv[0].x == Approx(0.0F));
  CHECK(cells.front().texture_uv[0].y == Approx(1.0F));
  CHECK(cells.back().texture_uv[3].x == Approx(1.0F));
  CHECK(cells.back().texture_uv[3].y == Approx(0.0F));
}
