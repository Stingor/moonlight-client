#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include <glm/glm.hpp>

#include "ro/formats/image.h"
#include "ro/render/sprite_frame.h"

namespace ro::render {

// Le rectangle englobant d'une suite de quads, en pixels.
struct SpriteBounds {
  glm::vec2 min{0.0F, 0.0F};
  glm::vec2 max{0.0F, 0.0F};

  [[nodiscard]] bool empty() const noexcept { return max.x <= min.x || max.y <= min.y; }
};

[[nodiscard]] SpriteBounds ComputeBounds(std::span<const SpriteQuad> quads);

// Dessine des quads de sprite dans une image, par recouvrement alpha.
//
// SANS GPU, et c'est délibéré : composer un personnage met en jeu des
// conventions — centrage, miroir, ancrage — dont les erreurs ne se voient qu'à
// l'œil. Pouvoir produire l'image hors de tout contexte graphique permet de la
// regarder, et un jour de la comparer automatiquement.
//
// `origin` est le point de l'image où tombe l'origine du repère des quads.
// L'échantillonnage est au plus proche voisin, comme celui d'un sprite RO à
// l'écran : un filtrage bilinéaire mélangerait les silhouettes.
class SpriteCanvas {
 public:
  SpriteCanvas(std::int32_t width, std::int32_t height);

  // `images` fournit une image par quad, dans le même ordre : c'est l'appelant
  // qui les résout, parce que lui seul sait quelle palette appliquer.
  void Draw(std::span<const SpriteQuad> quads, std::span<const formats::Image> images,
            glm::vec2 origin);

  [[nodiscard]] const formats::Image& image() const noexcept { return image_; }

 private:
  formats::Image image_;
};

}  // namespace ro::render
