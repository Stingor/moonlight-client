#include "ro/render/map_point_lights.h"

#include <algorithm>

namespace ro::render {

PointLightSet MapPointLights(const std::vector<ro::mapedit::AddedLight>& lights,
                             const MapEffectFrame& frame, const SceneLight& sky, bool always_night,
                             glm::vec3 eye) {
  const glm::vec3 day = sky.daylight * sky.tint;
  const float brightest = std::max({day.r, day.g, day.b});
  const float night = always_night ? 1.0F : 1.0F - std::clamp(brightest, 0.0F, 1.0F);
  std::vector<PointLight> placed;
  placed.reserve(lights.size());
  for (const ro::mapedit::AddedLight& light : lights) {
    const float strength = light.night_only ? night : 1.0F;
    placed.push_back(PointLight{
        .world_position =
            frame.ToWorld(glm::vec3(light.position[0], light.position[1], light.position[2])),
        .color = glm::vec3(light.color[0], light.color[1], light.color[2]) * strength,
        .range_world = light.range});
  }
  return NearestPointLights(placed, eye);
}

}  // namespace ro::render
