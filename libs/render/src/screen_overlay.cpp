#include "ro/render/screen_overlay.h"

#include <algorithm>
#include <cmath>

#include "ro/render/sprite_frame.h"

namespace ro::render {

std::vector<ScreenQuad> ComposeScreenOutline(glm::vec2 min, glm::vec2 max, float thickness_px,
                                             const std::array<std::uint8_t, 4>& tint) {
  // Un trait plus épais que la moitié du rectangle le remplirait : les deux
  // bords opposés se croiseraient, et le cadre deviendrait un aplat.
  const float thickness = std::min({thickness_px, (max.x - min.x) * 0.5F, (max.y - min.y) * 0.5F});
  if (thickness <= 0.0F) {
    return {};
  }
  // Les montants s'arrêtent AU RAS des traverses : sans quoi les coins
  // porteraient deux quads l'un sur l'autre, et paraîtraient plus opaques.
  const float inner_top = min.y + thickness;
  const float inner_bottom = max.y - thickness;
  return {
      ScreenQuad{.min = {min.x, min.y}, .max = {max.x, inner_top}, .tint = tint},
      ScreenQuad{.min = {min.x, inner_bottom}, .max = {max.x, max.y}, .tint = tint},
      ScreenQuad{.min = {min.x, inner_top}, .max = {min.x + thickness, inner_bottom}, .tint = tint},
      ScreenQuad{.min = {max.x - thickness, inner_top}, .max = {max.x, inner_bottom}, .tint = tint},
  };
}

std::vector<ScreenQuad> ComposeRoundedRect(glm::vec2 top_left, glm::vec2 bottom_right,
                                           float corner_radius_px,
                                           const std::array<std::uint8_t, 4>& tint) {
  std::vector<ScreenQuad> quads;
  const float width = bottom_right.x - top_left.x;
  const float height = bottom_right.y - top_left.y;
  if (width <= 0.0F || height <= 0.0F) {
    return quads;
  }
  // Un coin ne peut pas mordre le coin d'en face.
  const float radius = std::clamp(corner_radius_px, 0.0F, std::min(width, height) * 0.5F);
  const auto band = [&quads, &tint](float left, float right, float top, float bottom) {
    if (right > left && bottom > top) {
      quads.push_back(
          ScreenQuad{.min = glm::vec2{left, top}, .max = glm::vec2{right, bottom}, .tint = tint});
    }
  };
  if (radius <= 0.0F) {
    band(top_left.x, bottom_right.x, top_left.y, bottom_right.y);
    return quads;
  }
  // Le milieu d'un seul tenant, bandes de coin exclues.
  band(top_left.x, bottom_right.x, top_left.y + radius, bottom_right.y - radius);
  // Puis une bande d'un pixel par rangée de coin, retraitée de ce que le cercle
  // demande à cette hauteur-là. Les deux coins du HAUT et ceux du BAS partagent
  // le même retrait : le rectangle est symétrique.
  for (int row = 0; row < static_cast<int>(radius); ++row) {
    // Le centre du pixel, sinon la première rangée sortirait pleine largeur et
    // l'arrondi commencerait un pixel trop bas.
    const float from_edge = radius - (static_cast<float>(row) + 0.5F);
    const float inset = radius - std::sqrt(std::max(0.0F, radius * radius - from_edge * from_edge));
    const float left = top_left.x + inset;
    const float right = bottom_right.x - inset;
    const float top = top_left.y + static_cast<float>(row);
    band(left, right, top, top + 1.0F);
    const float bottom = bottom_right.y - static_cast<float>(row);
    band(left, right, bottom - 1.0F, bottom);
  }
  return quads;
}

std::vector<ScreenQuad> ComposeScreenSprite(const formats::SprSprite& sprite,
                                            const formats::ActAnimation& animation,
                                            std::size_t action, std::size_t frame,
                                            glm::vec2 anchor_px, float scale) {
  std::vector<ScreenQuad> quads;
  if (action >= animation.actions().size() || scale <= 0.0F) {
    return quads;
  }
  for (const SpriteQuad& quad : BuildFrameQuads(sprite, animation, action, frame)) {
    // Le quad d'un sprite est décrit par ses quatre coins ; en deux dimensions
    // il est droit, et les deux coins opposés suffisent. On prend le minimum et
    // le maximum plutôt que les coins 0 et 2, pour qu'une rotation de calque ne
    // rende pas un rectangle à l'envers.
    glm::vec2 low = quad.corners[0];
    glm::vec2 high = quad.corners[0];
    for (const glm::vec2& corner : quad.corners) {
      low = glm::min(low, corner);
      high = glm::max(high, corner);
    }
    quads.push_back(
        ScreenQuad{.min = anchor_px + low * scale,
                   .max = anchor_px + high * scale,
                   .source = SpriteImageKey{&sprite, nullptr, quad.section, quad.image_index},
                   .tint = quad.tint,
                   .mirrored = quad.mirrored});
  }
  return quads;
}

}  // namespace ro::render
