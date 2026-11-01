#include "ro/render/effect_panel_depth.h"

#include <algorithm>

namespace ro::render {
namespace {

// Sous cette profondeur, le point est sur l'œil ou derrière lui.
constexpr float kSmallestVisibleDepth = 0.0001F;

}  // namespace

std::array<glm::vec3, 4> EffectPanelDepthCorners(const std::array<glm::vec3, 4>& corners,
                                                 glm::vec3 ground, glm::vec3 eye,
                                                 glm::vec3 forward) {
  const auto depth_of = [&](glm::vec3 point) { return glm::dot(point - eye, forward); };

  float nearest = depth_of(corners[0]);
  for (const glm::vec3& corner : corners) {
    nearest = std::min(nearest, depth_of(corner));
    // LE RAYON QUI VISE CE COIN CROISE LE SOL AVANT LUI quand le coin descend
    // sous le niveau du point d'appui : c'est ce terrain-là qu'il ne faut pas
    // laisser passer devant. Sur un même rayon, la profondeur est
    // proportionnelle à la distance parcourue.
    if (corner.y < ground.y && eye.y > ground.y) {
      const float part = (eye.y - ground.y) / (eye.y - corner.y);
      nearest = std::min(nearest, part * depth_of(corner));
    }
  }
  const float panel_depth = std::max(nearest - kEffectPanelGroundClearance, kSmallestVisibleDepth);

  std::array<glm::vec3, 4> placed = corners;
  for (glm::vec3& corner : placed) {
    const float depth = depth_of(corner);
    if (depth > kSmallestVisibleDepth) {
      corner = eye + (corner - eye) * (panel_depth / depth);
    }
  }
  return placed;
}

}  // namespace ro::render
