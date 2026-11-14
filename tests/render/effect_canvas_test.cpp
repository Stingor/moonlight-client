// La toile des effets : ce qu'elle peint, sans GPU.

#include "ro/render/effect_canvas.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <glm/glm.hpp>

#include "ro/formats/image.h"
#include "ro/render/effect_scene.h"

namespace {

// Le facteur `ONE` de Direct3D : la source telle quelle, SANS son alpha.
constexpr std::int32_t kBlendOne = 2;

// La toile de l'essai, carrée.
constexpr std::int32_t kSide = 4;

constexpr std::size_t kChannelsPerPixel = 4;
using Rgba = std::array<std::uint8_t, kChannelsPerPixel>;

// Deux texels côte à côte : un rouge opaque, puis un transparent qui garde le
// magenta de la clé, comme `ApplyMagentaColorKey` le laisse.
ro::formats::Image RedThenKeyed() {
  ro::formats::Image image;
  image.width = 2;
  image.height = 1;
  image.pixels = {255, 0, 0, 255, 255, 0, 255, 0};
  return image;
}

Rgba PixelAt(const ro::formats::Image& image, std::int32_t x, std::int32_t y) {
  const std::size_t at = (static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width) +
                          static_cast<std::size_t>(x)) *
                         kChannelsPerPixel;
  return {image.pixels[at], image.pixels[at + 1], image.pixels[at + 2], image.pixels[at + 3]};
}

}  // namespace

TEST_CASE("Toile d'effet : un texel transparent ne se peint pas, même mélangé sans son alpha",
          "[render][effect]") {
  // 🔴 `ONE` PREND LA SOURCE SANS SON ALPHA : sans le seuil du nuanceur, le
  // texel transparent AJOUTERAIT son magenta au fond noir.
  ro::render::EffectQuad quad;
  // Toute la toile, centrée sur l'origine : la moitié gauche lit le premier
  // texel, la droite le second.
  quad.corners = {glm::vec2{-2.0F, -2.0F}, glm::vec2{2.0F, -2.0F}, glm::vec2{2.0F, 2.0F},
                  glm::vec2{-2.0F, 2.0F}};
  quad.blend_source = kBlendOne;
  quad.blend_destination = kBlendOne;
  const std::vector<ro::formats::Image> textures = {RedThenKeyed()};

  ro::render::EffectCanvas canvas(kSide, kSide);
  canvas.Paint(std::span<const ro::render::EffectQuad>(&quad, 1), textures,
               glm::vec2{kSide / 2.0F, kSide / 2.0F});

  CHECK(PixelAt(canvas.image(), 0, 1) == Rgba{255, 0, 0, 255});
  // Le fond, noir et opaque, reste tel quel sous le texel transparent.
  CHECK(PixelAt(canvas.image(), kSide - 1, 1) == Rgba{0, 0, 0, 255});
}
