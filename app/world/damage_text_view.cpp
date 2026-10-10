#include "world/damage_text_view.h"

#include <algorithm>
#include <cstddef>

#include "ro/render/actor_pick.h"
#include "ro/render/sprite_billboard.h"

namespace moonclient {

std::optional<ro::ui::DamageTextView> DamageTextOnScreen(const NumberScreen& screen,
                                                         const glm::vec3& placed,
                                                         const ro::render::NumberText& text,
                                                         const ro::render::GlyphBox& digit,
                                                         float outline_pixels) {
  if (screen.camera == nullptr) {
    return std::nullopt;
  }
  const ro::render::Camera& camera = *screen.camera;
  // LE MILIEU DES CHIFFRES, là où le sprite le met : le point d'appui, plus le
  // milieu de la boîte d'un chiffre à l'échelle de l'instant.
  const glm::vec2 middle_pixels =
      text.landing_pixels + glm::vec2{0.0F, digit.middle_y_pixels * text.scale};
  const glm::vec3 middle =
      ro::render::BillboardPoint(middle_pixels, placed, ro::render::MakeBillboardBasis(camera));
  const std::optional<glm::vec2> on_screen = ro::render::ProjectToScreen(
      camera, screen.aspect_ratio, middle, screen.viewport_width, screen.viewport_height);
  const float sprite_per_screen =
      ro::render::SpritePixelsPerScreenPixel(camera, middle, screen.viewport_height);
  if (!on_screen.has_value() || sprite_per_screen <= 0.0F) {
    return std::nullopt;
  }
  // Ce qu'un pixel de sprite de ce nombre, à cet instant, couvre à l'écran.
  const float screen_per_pixel = text.scale / sprite_per_screen;
  ro::ui::DamageTextView view;
  view.middle = ImVec2(on_screen->x, on_screen->y);
  view.digit_height = digit.height_pixels * screen_per_pixel;
  view.pitch = text.pitch_pixels * screen_per_pixel;
  view.outline = std::max(outline_pixels, 0.0F) * screen_per_pixel;
  view.color = IM_COL32(text.tint[0], text.tint[1], text.tint[2], text.tint[3]);
  // ⚠ LE ZÉRO FINAL EST GARDÉ : un nombre plus long que la place — ce qu'aucun
  // montant ne fait — est tronqué, jamais écrit au-delà.
  const std::size_t written = std::min(text.glyphs.size(), view.glyphs.size() - 1);
  std::copy_n(text.glyphs.begin(), written, view.glyphs.begin());
  return view;
}

}  // namespace moonclient
