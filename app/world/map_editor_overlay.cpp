#include "world/map_editor_overlay.h"

#include <array>
#include <cmath>
#include <optional>
#include <utility>

#include <glm/gtc/constants.hpp>

#include "ro/render/actor_pick.h"
#include "ro/render/map_effect_math.h"
#include "ro/render/model_pick.h"
#include "ro/render/terrain_mesh.h"

namespace moonclient {
namespace {

// LES DOUZE ARÊTES D'UNE BOÎTE, par paires de coins : deux coins dont les rangs
// ne diffèrent que d'un bit sont reliés.
constexpr std::array<std::pair<std::size_t, std::size_t>, 12> kBoxEdges = {{
    {0, 1},
    {2, 3},
    {4, 5},
    {6, 7},  // le long de x
    {0, 2},
    {1, 3},
    {4, 6},
    {5, 7},  // le long de y
    {0, 4},
    {1, 5},
    {2, 6},
    {3, 7},  // le long de z
}};

}  // namespace

std::vector<ro::ui::MapEditorLine> SelectionOutline(const ro::render::MapScene& scene,
                                                    std::size_t rsw_rank,
                                                    const ro::render::Camera& camera,
                                                    float aspect_ratio, float viewport_width,
                                                    float viewport_height) {
  std::vector<ro::ui::MapEditorLine> lines;
  const std::optional<ro::render::ScenePose> pose = ro::render::FindScenePose(scene, rsw_rank);
  if (!pose.has_value()) {
    return lines;
  }
  const std::array<glm::vec3, 8> corners = ro::render::PoseCorners(*pose);
  std::array<std::optional<glm::vec2>, 8> projected{};
  for (std::size_t corner = 0; corner < corners.size(); ++corner) {
    projected[corner] = ro::render::ProjectToScreen(camera, aspect_ratio, corners[corner],
                                                    viewport_width, viewport_height);
  }
  for (const auto& [first, second] : kBoxEdges) {
    if (projected[first].has_value() && projected[second].has_value()) {
      lines.push_back(ro::ui::MapEditorLine{.from = *projected[first], .to = *projected[second]});
    }
  }
  return lines;
}

std::vector<MapEditorSession::LightDot> LightDots(const ro::ui::MapEditorView& view,
                                                  const ro::render::MapScene& scene,
                                                  const ro::render::Camera& camera,
                                                  float aspect_ratio, float viewport_width,
                                                  float viewport_height) {
  const ro::render::MapEffectFrame frame{scene.gnd.width(), scene.gnd.height()};
  std::vector<MapEditorSession::LightDot> dots;
  for (std::size_t slot = 0; slot < view.lights.size(); ++slot) {
    const std::array<float, 3>& native = view.lights[slot].position;
    if (const std::optional<glm::vec2> at = ro::render::ProjectToScreen(
            camera, aspect_ratio, frame.ToWorld(glm::vec3(native[0], native[1], native[2])),
            viewport_width, viewport_height)) {
      dots.push_back(MapEditorSession::LightDot{.at = *at, .slot = slot});
    }
  }
  return dots;
}

std::vector<ro::ui::MapEditorLine> LightRangeRing(const ro::ui::MapEditorLight& light,
                                                  const ro::render::MapScene& scene,
                                                  const ro::render::Camera& camera,
                                                  float aspect_ratio, float viewport_width,
                                                  float viewport_height) {
  // Le cercle en segments : assez pour paraître rond à l'écran.
  constexpr int kRingSegments = 48;
  const ro::render::MapEffectFrame frame{scene.gnd.width(), scene.gnd.height()};
  const glm::vec3 centre =
      frame.ToWorld(glm::vec3(light.position[0], light.position[1], light.position[2]));
  const auto point_at = [&](int segment) {
    const float angle =
        glm::two_pi<float>() * static_cast<float>(segment) / static_cast<float>(kRingSegments);
    return centre + glm::vec3(std::cos(angle), 0.0F, std::sin(angle)) * light.range;
  };
  std::vector<ro::ui::MapEditorLine> ring;
  for (int segment = 0; segment < kRingSegments; ++segment) {
    const std::optional<glm::vec2> from = ro::render::ProjectToScreen(
        camera, aspect_ratio, point_at(segment), viewport_width, viewport_height);
    const std::optional<glm::vec2> to = ro::render::ProjectToScreen(
        camera, aspect_ratio, point_at(segment + 1), viewport_width, viewport_height);
    if (from.has_value() && to.has_value()) {
      ring.push_back(ro::ui::MapEditorLine{
          .from = *from, .to = *to, .kind = ro::ui::MapEditorLineKind::kSelection});
    }
  }
  return ring;
}

std::array<float, 2> CornerPointOf(const ro::formats::GndMap& gnd, float world_x, float world_z) {
  // Les rangées descendent en Z : voir `ro::render::CubePointAt`.
  const float depth = static_cast<float>(gnd.height()) * ro::render::kWorldUnitsPerCube;
  return {world_x / ro::render::kWorldUnitsPerCube,
          (depth + ro::render::kWorldUnitsPerCube - world_z) / ro::render::kWorldUnitsPerCube};
}

std::vector<ro::ui::MapEditorLine> BrushRing(const ro::render::MapScene& scene,
                                             const std::array<float, 2>& corner_point,
                                             float radius_cubes, const ro::render::Camera& camera,
                                             float aspect_ratio, float viewport_width,
                                             float viewport_height) {
  // Le cercle en segments : assez pour paraître rond, et suivre le sol qu'il
  // épouse.
  constexpr int kRingSegments = 64;
  // Un peu au-dessus du sol, en unités du monde : posé dessus, il s'y
  // enfoncerait à moitié.
  constexpr float kRingLift = 0.5F;
  const float depth = static_cast<float>(scene.gnd.height()) * ro::render::kWorldUnitsPerCube;
  const auto point_at = [&](int segment) {
    const float angle =
        glm::two_pi<float>() * static_cast<float>(segment) / static_cast<float>(kRingSegments);
    const float column = corner_point[0] + std::cos(angle) * radius_cubes;
    const float row = corner_point[1] + std::sin(angle) * radius_cubes;
    const float world_x = column * ro::render::kWorldUnitsPerCube;
    const float world_z =
        depth + ro::render::kWorldUnitsPerCube - row * ro::render::kWorldUnitsPerCube;
    return glm::vec3(world_x, ro::render::GroundHeightAt(scene.gnd, world_x, world_z) + kRingLift,
                     world_z);
  };
  std::vector<ro::ui::MapEditorLine> ring;
  std::optional<glm::vec2> from = ro::render::ProjectToScreen(camera, aspect_ratio, point_at(0),
                                                              viewport_width, viewport_height);
  for (int segment = 1; segment <= kRingSegments; ++segment) {
    const std::optional<glm::vec2> to = ro::render::ProjectToScreen(
        camera, aspect_ratio, point_at(segment), viewport_width, viewport_height);
    if (from.has_value() && to.has_value()) {
      ring.push_back(ro::ui::MapEditorLine{
          .from = *from, .to = *to, .kind = ro::ui::MapEditorLineKind::kBrush});
    }
    from = to;
  }
  return ring;
}

std::vector<MapEditorSession::LightDot> EffectDots(const ro::ui::MapEditorView& view,
                                                   const ro::render::MapScene& scene,
                                                   const ro::render::Camera& camera,
                                                   float aspect_ratio, float viewport_width,
                                                   float viewport_height) {
  const ro::render::MapEffectFrame frame{scene.gnd.width(), scene.gnd.height()};
  std::vector<MapEditorSession::LightDot> dots;
  for (std::size_t slot = 0; slot < view.effects.size(); ++slot) {
    const std::array<float, 3>& native = view.effects[slot].position;
    if (const std::optional<glm::vec2> at = ro::render::ProjectToScreen(
            camera, aspect_ratio, frame.ToWorld(glm::vec3(native[0], native[1], native[2])),
            viewport_width, viewport_height)) {
      dots.push_back(MapEditorSession::LightDot{.at = *at, .slot = slot});
    }
  }
  return dots;
}

std::vector<MapEditorSession::LightDot> MapEffectDots(const ro::ui::MapEditorView& view,
                                                      const ro::render::MapScene& scene,
                                                      const ro::render::Camera& camera,
                                                      float aspect_ratio, float viewport_width,
                                                      float viewport_height) {
  const ro::render::MapEffectFrame frame{scene.gnd.width(), scene.gnd.height()};
  std::vector<MapEditorSession::LightDot> dots;
  for (const ro::ui::MapEditorMapEffect& effect : view.map_effects) {
    const std::array<float, 3>& native = effect.position;
    if (const std::optional<glm::vec2> at = ro::render::ProjectToScreen(
            camera, aspect_ratio, frame.ToWorld(glm::vec3(native[0], native[1], native[2])),
            viewport_width, viewport_height)) {
      dots.push_back(MapEditorSession::LightDot{.at = *at, .slot = effect.effect_index});
    }
  }
  return dots;
}

}  // namespace moonclient
