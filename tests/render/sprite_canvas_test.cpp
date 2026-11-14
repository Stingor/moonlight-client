#include "ro/render/sprite_canvas.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/image.h"
#include "ro/render/sprite_frame.h"

#include "support/image_builder.h"

using ro::formats::Image;
using ro::render::ComputeBounds;
using ro::render::SpriteBounds;
using ro::render::SpriteCanvas;
using ro::render::SpriteQuad;
using ro::test_support::PixelAt;

namespace {

// Une image 2×1 : rouge opaque à gauche, transparent à droite. Toute inversion
// horizontale se voit, et le pixel vide prouve que l'alpha est respecté.
Image RedThenEmpty() {
  Image image;
  image.width = 2;
  image.height = 1;
  image.pixels = {255, 0, 0, 255, 0, 255, 0, 0};
  return image;
}

// Un quad aligné aux axes, du coin donné, aux dimensions données.
SpriteQuad QuadAt(float left, float top, float width, float height) {
  SpriteQuad quad;
  quad.image_index = 0;
  quad.corners = {glm::vec2{left, top}, glm::vec2{left + width, top},
                  glm::vec2{left + width, top + height}, glm::vec2{left, top + height}};
  return quad;
}

}  // namespace

TEST_CASE("Emprise : elle couvre tous les coins de tous les quads", "[render][canvas]") {
  const std::vector<SpriteQuad> quads = {QuadAt(-5.0F, -3.0F, 2.0F, 2.0F),
                                         QuadAt(10.0F, 4.0F, 1.0F, 1.0F)};
  const SpriteBounds bounds = ComputeBounds(quads);
  CHECK(bounds.min.x == -5.0F);
  CHECK(bounds.min.y == -3.0F);
  CHECK(bounds.max.x == 11.0F);
  CHECK(bounds.max.y == 5.0F);
}

TEST_CASE("Emprise : aucune liste de quads donne une emprise vide", "[render][canvas]") {
  CHECK(ComputeBounds({}).empty());
}

TEST_CASE("Canevas : le quad se pose à l'origine indiquée", "[render][canvas]") {
  SpriteCanvas canvas(4, 2);
  const std::vector<SpriteQuad> quads = {QuadAt(0.0F, 0.0F, 2.0F, 1.0F)};
  const std::vector<Image> images = {RedThenEmpty()};
  canvas.Draw(quads, images, glm::vec2{1.0F, 0.0F});

  // Décalé d'un pixel vers la droite : la colonne 0 reste vide.
  CHECK(PixelAt(canvas.image(), 0, 0)[3] == 0);
  CHECK(PixelAt(canvas.image(), 1, 0) == std::vector<std::uint8_t>{255, 0, 0, 255});
  // Le second texel de la source est transparent : rien n'est écrit.
  CHECK(PixelAt(canvas.image(), 2, 0)[3] == 0);
}

TEST_CASE("Canevas : le miroir retourne la TEXTURE, pas le quad", "[render][canvas]") {
  // Le confondre avec une échelle négative déplacerait le quad : le pixel
  // opaque sortirait du cadre au lieu de changer de côté.
  SpriteCanvas canvas(2, 1);
  std::vector<SpriteQuad> quads = {QuadAt(0.0F, 0.0F, 2.0F, 1.0F)};
  quads[0].mirrored = true;
  canvas.Draw(quads, std::vector<Image>{RedThenEmpty()}, glm::vec2{0.0F, 0.0F});

  // Le rouge est passé à droite, et le quad occupe toujours les deux colonnes.
  CHECK(PixelAt(canvas.image(), 0, 0)[3] == 0);
  CHECK(PixelAt(canvas.image(), 1, 0) == std::vector<std::uint8_t>{255, 0, 0, 255});
}

TEST_CASE("Canevas : la teinte multiplie la couleur du quad", "[render][canvas]") {
  SpriteCanvas canvas(2, 1);
  std::vector<SpriteQuad> quads = {QuadAt(0.0F, 0.0F, 2.0F, 1.0F)};
  quads[0].tint = {128, 255, 255, 255};
  canvas.Draw(quads, std::vector<Image>{RedThenEmpty()}, glm::vec2{0.0F, 0.0F});

  CHECK(PixelAt(canvas.image(), 0, 0)[0] == 128);
}

TEST_CASE("Canevas : un quad hors cadre ne déborde pas", "[render][canvas]") {
  SpriteCanvas canvas(2, 1);
  const std::vector<SpriteQuad> quads = {QuadAt(-100.0F, -100.0F, 2.0F, 1.0F)};
  canvas.Draw(quads, std::vector<Image>{RedThenEmpty()}, glm::vec2{0.0F, 0.0F});
  CHECK(PixelAt(canvas.image(), 0, 0)[3] == 0);
  CHECK(PixelAt(canvas.image(), 1, 0)[3] == 0);
}

TEST_CASE("Canevas : une image agrandie garde ses proportions", "[render][canvas]") {
  // Échantillonnage au plus proche voisin : un sprite RO agrandi doit rester
  // net, chaque texel occupant un bloc entier.
  SpriteCanvas canvas(4, 2);
  const std::vector<SpriteQuad> quads = {QuadAt(0.0F, 0.0F, 4.0F, 2.0F)};
  canvas.Draw(quads, std::vector<Image>{RedThenEmpty()}, glm::vec2{0.0F, 0.0F});

  CHECK(PixelAt(canvas.image(), 0, 0)[0] == 255);
  CHECK(PixelAt(canvas.image(), 1, 1)[0] == 255);
  CHECK(PixelAt(canvas.image(), 2, 0)[3] == 0);
}
