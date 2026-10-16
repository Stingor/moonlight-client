#include "ro/render/point_lights.h"

#include <algorithm>
#include <vector>

namespace ro::render {
namespace {

[[nodiscard]] bool Shines(const PointLight& light) {
  return light.range_world > 0.0F && light.color != glm::vec3(0.0F);
}

}  // namespace

PointLightSet NearestPointLights(std::span<const PointLight> lights, glm::vec3 eye) {
  std::vector<const PointLight*> shining;
  shining.reserve(lights.size());
  for (const PointLight& light : lights) {
    if (Shines(light)) {
      shining.push_back(&light);
    }
  }
  // Le carré de la distance suffit à ordonner. À distance égale, l'ordre
  // d'arrivée tient : un ex æquo ne clignote pas d'une image à l'autre.
  const auto distance_squared = [&eye](const PointLight* light) {
    const glm::vec3 offset = light->world_position - eye;
    return glm::dot(offset, offset);
  };
  std::ranges::stable_sort(shining, {}, distance_squared);

  PointLightSet set;
  set.count = std::min(shining.size(), kMaxPointLights);
  for (std::size_t rank = 0; rank < set.count; ++rank) {
    set.lights[rank] = *shining[rank];
  }
  return set;
}

}  // namespace ro::render
