#include "ro/render/sprite_canvas.h"

#include <algorithm>
#include <cmath>

namespace ro::render {
namespace {

// Recouvrement alpha classique, en entiers : destination = source × a +
// destination × (1 − a). Les sprites RO sont presque entièrement binaires en
// alpha, mais les images BGRA en portent un vrai.
void BlendPixel(std::uint8_t* destination, const std::uint8_t* source,
                const std::array<std::uint8_t, 4>& tint) {
  const auto alpha =
      static_cast<std::uint32_t>(source[3]) * static_cast<std::uint32_t>(tint[3]) / 255U;
  if (alpha == 0) {
    return;
  }
  for (std::size_t channel = 0; channel < 3; ++channel) {
    const std::uint32_t tinted = static_cast<std::uint32_t>(source[channel]) *
                                 static_cast<std::uint32_t>(tint[channel]) / 255U;
    const std::uint32_t mixed =
        (tinted * alpha + static_cast<std::uint32_t>(destination[channel]) * (255U - alpha)) / 255U;
    destination[channel] = static_cast<std::uint8_t>(mixed);
  }
  destination[3] = static_cast<std::uint8_t>(
      std::min(255U, alpha + static_cast<std::uint32_t>(destination[3]) * (255U - alpha) / 255U));
}

}  // namespace

void GrowBounds(SpriteBounds& bounds, bool& grown, const SpriteQuad& quad) {
  for (const glm::vec2& corner : quad.corners) {
    if (!grown) {
      bounds.min = corner;
      bounds.max = corner;
      grown = true;
      continue;
    }
    bounds.min = glm::min(bounds.min, corner);
    bounds.max = glm::max(bounds.max, corner);
  }
}

SpriteBounds ComputeBounds(std::span<const SpriteQuad> quads) {
  SpriteBounds bounds;
  bool grown = false;
  for (const SpriteQuad& quad : quads) {
    GrowBounds(bounds, grown, quad);
  }
  return bounds;
}

SpriteCanvas::SpriteCanvas(std::int32_t width, std::int32_t height) {
  image_.width = std::max(width, 0);
  image_.height = std::max(height, 0);
  image_.pixels.assign(
      static_cast<std::size_t>(image_.width) * static_cast<std::size_t>(image_.height) * 4, 0);
}

void SpriteCanvas::Draw(std::span<const SpriteQuad> quads, std::span<const formats::Image> images,
                        glm::vec2 origin) {
  for (std::size_t index = 0; index < quads.size() && index < images.size(); ++index) {
    const SpriteQuad& quad = quads[index];
    const formats::Image& source = images[index];
    if (source.empty() || source.width <= 0 || source.height <= 0) {
      continue;
    }

    // Le quad d'un acteur est aligné aux axes : ses coins 0 et 2 suffisent.
    const glm::vec2 top_left = origin + quad.corners[0];
    const glm::vec2 bottom_right = origin + quad.corners[2];
    const auto left = static_cast<std::int32_t>(std::floor(top_left.x));
    const auto top = static_cast<std::int32_t>(std::floor(top_left.y));
    const auto right = static_cast<std::int32_t>(std::ceil(bottom_right.x));
    const auto bottom = static_cast<std::int32_t>(std::ceil(bottom_right.y));
    const float span_x = bottom_right.x - top_left.x;
    const float span_y = bottom_right.y - top_left.y;
    if (span_x <= 0.0F || span_y <= 0.0F) {
      continue;
    }

    for (std::int32_t y = std::max(top, 0); y < std::min(bottom, image_.height); ++y) {
      for (std::int32_t x = std::max(left, 0); x < std::min(right, image_.width); ++x) {
        // Au plus proche voisin, depuis le CENTRE du pixel de destination :
        // échantillonner depuis son coin décale l'image d'un demi-texel.
        const float u = (static_cast<float>(x) + 0.5F - top_left.x) / span_x;
        const float v = (static_cast<float>(y) + 0.5F - top_left.y) / span_y;
        if (u < 0.0F || u >= 1.0F || v < 0.0F || v >= 1.0F) {
          continue;
        }
        // Le miroir échange les abscisses de texture, il ne déplace pas le quad.
        const float sampled_u = quad.mirrored ? 1.0F - u : u;
        const auto source_x =
            std::min(static_cast<std::int32_t>(sampled_u * static_cast<float>(source.width)),
                     source.width - 1);
        const auto source_y = std::min(
            static_cast<std::int32_t>(v * static_cast<float>(source.height)), source.height - 1);

        const std::size_t source_offset =
            (static_cast<std::size_t>(source_y) * static_cast<std::size_t>(source.width) +
             static_cast<std::size_t>(source_x)) *
            4;
        const std::size_t destination_offset =
            (static_cast<std::size_t>(y) * static_cast<std::size_t>(image_.width) +
             static_cast<std::size_t>(x)) *
            4;
        BlendPixel(&image_.pixels[destination_offset], &source.pixels[source_offset], quad.tint);
      }
    }
  }
}

}  // namespace ro::render
