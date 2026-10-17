#include "ro/render/sun_shadow_pass.h"

#include <array>

#include <glm/glm.hpp>

#include "ro/render/sun_shadow.h"

namespace ro::render {

SunShadowView DrawSunShadow(SunShadowMap& map, const SunShadowCasters& casters,
                            const TerrainMesh& terrain, const SceneLight& light,
                            const Camera& camera, float aspect_ratio, float strength) {
  if (!map.valid()) {
    return SunShadowView();
  }
  const auto point = [](const std::array<float, 3>& xyz) {
    return glm::vec3(xyz[0], xyz[1], xyz[2]);
  };
  const SunShadowFrame frame =
      FrameSunShadow(light.direction, camera, aspect_ratio, point(terrain.bounds_min),
                     point(terrain.bounds_max), map.side_texels());
  map.BeginDepthPass();
  if (casters.terrain != nullptr) {
    casters.terrain->DrawDepth(frame.view_projection);
  }
  if (casters.models != nullptr) {
    casters.models->DrawDepth(frame.view_projection, casters.clock, casters.model_casting);
  }
  map.BeginActorLayer();
  if (casters.sprites != nullptr) {
    map.SetCasterOffset(false);
    for (const std::span<const DrawnActor> lot : casters.actor_lots) {
      casters.sprites->DrawDepth(frame.view_projection, light.direction, camera, lot);
    }
    map.SetCasterOffset(true);
  }
  if (casters.actor_model_renderer != nullptr) {
    casters.actor_model_renderer->DrawDepth(frame.view_projection, casters.actor_models);
  }
  map.EndDepthPass();
  return SunShadowView{.map = &map,
                       .view_projection = frame.view_projection,
                       .texel_world = frame.texel_world,
                       .strength = strength,
                       .sprite_shade = SunShadeOf(light),
                       .toward_sun = glm::normalize(light.direction)};
}

}  // namespace ro::render
