// LA CLASSE QUI PEINT EN PLEIN ÉCRAN : le givre d'une gelure (2272), quatre
// quadrants qui reprennent la même image en miroir, tant que le porteur est là.

#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string_view>

#include "effect_class_internal.h"

namespace ro::render::effect_class {
namespace {

// L'IMAGE EST LE QUART HAUT-DROIT DU CADRE : les trois autres quadrants la
// retournent, horizontalement à gauche, verticalement en bas.
constexpr std::string_view kFrostbiteTexture =
    "effect\\state_effect\\frostbite\\frostbite_screen\\frostbite_screen.tga";
constexpr std::size_t kQuadrants = 4;
constexpr std::size_t kCorners = 4;

// Un coin du quadrant : sa place en pixels, et si la texture s'y lit à
// l'envers sur chaque axe.
struct QuadrantCorner {
  glm::vec2 pixel;
  glm::vec2 uv;
};

class FullScreen final : public EffectClassInstance {
 public:
  explicit FullScreen(const EffectClassBirth& birth) : EffectClassInstance(birth) {
    reused_on_same_owner_ = true;
  }
  [[nodiscard]] bool Alive(const EffectClassTick& tick) const override {
    return tick.owner_present;
  }
  void Draw(const EffectClassView& view, EffectClassOutput& output) const override {
    if (view.viewport_width <= 0.0F || view.viewport_height <= 0.0F) {
      return;
    }
    const float width = view.viewport_width;
    const float height = view.viewport_height;
    // Le centre est celui du client : la moitié entière de chaque côté.
    const float middle_x = std::floor(width / 2.0F);
    const float middle_y = std::floor(height / 2.0F);
    for (std::size_t quadrant = 0; quadrant < kQuadrants; ++quadrant) {
      const bool right = quadrant % 2 == 1;
      const bool bottom = quadrant >= 2;
      const float left_x = right ? middle_x : 0.0F;
      const float right_x = right ? width : middle_x;
      const float top_y = bottom ? middle_y : 0.0F;
      const float bottom_y = bottom ? height : middle_y;
      // Les abscisses de texture se lisent de droite à gauche dans les quadrants
      // de gauche, les ordonnées de bas en haut dans ceux du bas.
      const float u_left = right ? 0.0F : 1.0F;
      const float u_right = right ? 1.0F : 0.0F;
      const float v_top = bottom ? 1.0F : 0.0F;
      const float v_bottom = bottom ? 0.0F : 1.0F;
      // Haut-gauche, bas-gauche, bas-droit, haut-droit : le cycle du quad.
      const std::array<QuadrantCorner, kCorners> corners = {
          QuadrantCorner{{left_x, top_y}, {u_left, v_top}},
          QuadrantCorner{{left_x, bottom_y}, {u_left, v_bottom}},
          QuadrantCorner{{right_x, bottom_y}, {u_right, v_bottom}},
          QuadrantCorner{{right_x, top_y}, {u_right, v_top}}};
      EffectClassScreenDraw draw;
      draw.texture = kFrostbiteTexture;
      for (std::size_t corner = 0; corner < kCorners; ++corner) {
        draw.quad.corners[corner] = corners[corner].pixel;
        draw.quad.texture_uv[corner] = corners[corner].uv;
      }
      output.screen_quads.push_back(draw);
    }
  }

 protected:
  void Update(const EffectClassTick& /*tick*/, EffectClassOutput& /*output*/) override {}
};

}  // namespace

std::unique_ptr<EffectClassInstance> MakeFullScreen(const EffectClassBirth& birth,
                                                    const EffectClassView& /*view*/) {
  return std::make_unique<FullScreen>(birth);
}

}  // namespace ro::render::effect_class
