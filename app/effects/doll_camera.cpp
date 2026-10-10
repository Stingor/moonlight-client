#include "effects/doll_camera.h"

#include <cmath>

#include "ro/render/sprite_billboard.h"

namespace moonclient {

ro::render::Camera DollCamera(glm::vec3 feet, const ro::render::VignetteFraming& framing,
                              float box_width, float box_height) {
  ro::render::Camera camera;
  camera.pitch_degrees = ro::render::kGameElevationDegrees;
  camera.fov_degrees = ro::render::kGameFovDegrees;
  const float box_pixels_per_unit = ro::render::kSpritePixelsPerWorldUnit * framing.scale;
  const float half_fov = glm::radians(camera.fov_degrees) * 0.5F;
  camera.distance = box_height / (2.0F * std::tan(half_fov) * box_pixels_per_unit);
  const ro::render::BillboardBasis basis = ro::render::MakeBillboardBasis(camera);
  const glm::vec2 centre(box_width * 0.5F, box_height * 0.5F);
  camera.look_at = feet + basis.right * ((centre.x - framing.origin.x) / box_pixels_per_unit) +
                   basis.up * ((framing.origin.y - centre.y) / box_pixels_per_unit);
  return camera;
}

}  // namespace moonclient
