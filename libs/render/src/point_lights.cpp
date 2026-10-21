#include "ro/render/point_lights.h"

#include <algorithm>
#include <cstddef>
#include <vector>

#include "ro/render/world_frustum.h"

namespace ro::render {
namespace {

[[nodiscard]] bool Shines(const PointLight& light) {
  return light.range_world > 0.0F && light.color != glm::vec3(0.0F);
}

}  // namespace

PointLightSet PointLightsByDistance(std::span<const PointLight> lights, glm::vec3 eye) {
  PointLightSet set;
  set.lights.reserve(lights.size());
  for (const PointLight& light : lights) {
    if (Shines(light)) {
      set.lights.push_back(light);
    }
  }
  // Le carré de la distance suffit à ordonner. À distance égale, l'ordre
  // d'arrivée tient : un ex æquo ne clignote pas d'une image à l'autre.
  const auto distance_squared = [&eye](const PointLight& light) {
    const glm::vec3 offset = light.world_position - eye;
    return glm::dot(offset, offset);
  };
  std::ranges::stable_sort(set.lights, {}, distance_squared);
  return set;
}

PointLightSet VisiblePointLights(const PointLightSet& set, const glm::mat4& view_projection) {
  const WorldFrustum frustum = WorldFrustum::Of(view_projection);
  PointLightSet visible;
  visible.stage = set.stage;
  visible.lights.reserve(set.lights.size());
  for (std::size_t rank = 0; rank < set.lights.size(); ++rank) {
    const PointLight& light = set.lights[rank];
    if (!frustum.SeesSphere(light.world_position, light.range_world)) {
      continue;
    }
    visible.lights.push_back(light);
    if (rank < set.shadows.size()) {
      visible.shadows.push_back(set.shadows[rank]);
    }
  }
  if (visible.shadows.size() != visible.lights.size()) {
    visible.shadows.clear();
  }
  return visible;
}

glm::vec3 PointLightingAt(const PointLightSet& set, glm::vec3 world_position) {
  glm::vec3 sum(0.0F);
  for (const PointLight& light : set.lights) {
    if (!Shines(light)) {
      continue;
    }
    const float remaining = std::clamp(
        1.0F - (glm::distance(world_position, ShiningFrom(light)) / light.range_world), 0.0F, 1.0F);
    sum += light.color * remaining * remaining;
  }
  return sum;
}

}  // namespace ro::render
