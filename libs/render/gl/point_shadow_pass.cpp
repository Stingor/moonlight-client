#include "ro/render/point_shadow_pass.h"

#include <array>
#include <cstddef>
#include <span>

#include <glm/glm.hpp>

#include "ro/render/point_shadow.h"

namespace ro::render {

PointLightSet DrawPointShadows(PointShadowMaps& maps, const SunShadowCasters& casters,
                               const PointLightSet& nearest, const Camera& camera) {
  // Une lumière déjà ombrée garde sa carte : voir `KeepShadowedOrder`.
  const PointLightSet points = KeepShadowedOrder(nearest, maps.painted(), camera.look_at);
  PointLightSet shadowed = points;
  shadowed.shadow_maps = nullptr;
  shadowed.shadowed_count = 0;
  const std::size_t count = ShadowedPointLightCount(points);
  if (!maps.valid() || count == 0) {
    return shadowed;
  }
  const std::array<bool, kMaxShadowedPointLights> repaint =
      PointShadowRepaints(maps.painted(), points, count, maps.TakeFrame());
  maps.BeginDepthPass();
  for (std::size_t rank = 0; rank < count; ++rank) {
    const PointLight& light = points.lights[rank];
    if (!repaint[rank]) {
      continue;
    }
    maps.NotePainted(rank, PaintedPointLight{.world_position = light.world_position,
                                             .range_world = light.range_world});
    const std::array<glm::mat4, kCubeFaceCount> faces =
        PointShadowFaceViewProjections(light.world_position, PointShadowFar(light.range_world));
    for (std::size_t face = 0; face < kCubeFaceCount; ++face) {
      maps.BeginFace(rank, face);
      const glm::mat4& view_projection = faces[face];
      if (casters.terrain != nullptr) {
        casters.terrain->DrawDepth(view_projection);
      }
      if (casters.models != nullptr) {
        casters.models->DrawDepth(view_projection, casters.clock);
      }
      // UN CORPS PLAT se dresse face à la lumière, le long de l'axe de la face :
      // ses panneaux, dressés pour le soleil, ne savent tourner que vers une
      // direction par lot.
      if (casters.sprites != nullptr) {
        maps.SetCasterOffset(false);
        for (const std::span<const DrawnActor> lot : casters.actor_lots) {
          casters.sprites->DrawDepth(view_projection, -CubeFaceForward(face), camera, lot);
        }
        maps.SetCasterOffset(true);
      }
      if (casters.actor_model_renderer != nullptr) {
        casters.actor_model_renderer->DrawDepth(view_projection, casters.actor_models);
      }
    }
  }
  maps.EndDepthPass();
  shadowed.shadow_maps = &maps;
  shadowed.shadowed_count = count;
  return shadowed;
}

}  // namespace ro::render
