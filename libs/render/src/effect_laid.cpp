#include "ro/render/effect_laid.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <functional>
#include <span>
#include <vector>

#include <glm/glm.hpp>

namespace ro::render {
namespace {

// Le π du client, qui convertit l'orientation : un π plus juste la décalerait
// d'un rien.
constexpr float kClientPi = 3.141592F;
constexpr float kHalfTurnDegrees = 180.0F;

}  // namespace

void LayEffectQuads(std::span<const EffectQuad> quads, const LaidEffectPose& pose,
                    const std::function<float(float world_x, float world_z)>& ground_height,
                    std::vector<EffectWorldQuad>& out) {
  // L'ABSCISSE DE L'IMAGE va vers la droite de l'écran à orientation nulle : le
  // cap de la caméra s'en retranche, puisqu'il tourne la droite de l'écran
  // dans l'autre sens.
  const float radians =
      (pose.turn_degrees - pose.camera_yaw_degrees) / kHalfTurnDegrees * kClientPi;
  const glm::vec3 rightward(std::cos(radians), 0.0F, std::sin(radians));
  const glm::vec3 downward(-std::sin(radians), 0.0F, std::cos(radians));
  const float scale_x = pose.flip_x ? -pose.scale : pose.scale;
  const float scale_y = pose.flip_y ? -pose.scale : pose.scale;
  for (const EffectQuad& quad : quads) {
    EffectWorldQuad layer;
    for (std::size_t corner = 0; corner < layer.corners.size(); ++corner) {
      const glm::vec2 pixel = quad.corners[corner];
      glm::vec3 point =
          pose.center + rightward * (pixel.x * scale_x) + downward * (pixel.y * scale_y);
      point.y =
          (ground_height ? ground_height(point.x, point.z) : pose.center.y) + kLaidEffectLiftUnits;
      layer.corners[corner] = point;
    }
    // L'ordre des coins est celui de l'image : haut-gauche, haut-droit,
    // bas-droit, bas-gauche.
    layer.texture_uv = {
        glm::vec2{quad.uv_min.x, quad.uv_min.y}, glm::vec2{quad.uv_max.x, quad.uv_min.y},
        glm::vec2{quad.uv_max.x, quad.uv_max.y}, glm::vec2{quad.uv_min.x, quad.uv_max.y}};
    layer.tint = quad.tint;
    layer.blend_source = quad.blend_source;
    layer.blend_destination = quad.blend_destination;
    layer.texture = quad.texture;
    out.push_back(layer);
  }
}

void ShapeStandingEffectCorners(std::array<glm::vec2, 4>& corners, float lift_pixels,
                                float spin_degrees, float scale, bool flip_x, bool flip_y) {
  const bool rotated = spin_degrees > 0.0F;
  const float radians = spin_degrees / kHalfTurnDegrees * kClientPi;
  const float cosinus = std::cos(radians);
  const float sinus = std::sin(radians);
  for (glm::vec2& corner : corners) {
    // Autour du POINT DU SOL : l'ancre est à `lift_pixels` au-dessus, donc
    // plus BAS en ordonnée descendante.
    glm::vec2 from_ground(corner.x, corner.y - lift_pixels);
    from_ground *= scale;
    if (flip_x) {
      from_ground.x = -from_ground.x;
    }
    if (flip_y) {
      from_ground.y = -from_ground.y;
    }
    if (rotated) {
      from_ground = glm::vec2(from_ground.x * cosinus - from_ground.y * sinus,
                              from_ground.x * sinus + from_ground.y * cosinus);
    }
    corner = glm::vec2(from_ground.x, from_ground.y + lift_pixels);
  }
}

}  // namespace ro::render
