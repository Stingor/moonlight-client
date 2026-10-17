#include "ro/render/point_shadow.h"

#include <algorithm>
#include <array>
#include <cstddef>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace ro::render {
namespace {

// Un quart de l'espace par face : un champ de 90°, carré.
constexpr float kFaceFieldDegrees = 90.0F;
constexpr float kFaceAspectRatio = 1.0F;

struct FaceAxes {
  glm::vec3 forward;
  glm::vec3 up;
};

// LE HAUT DE CHAQUE FACE est celui que la table des textures cubiques d'OpenGL
// impose : les faces latérales ont le haut vers −Y, celles du dessus et du
// dessous vers ±Z.
constexpr std::array<FaceAxes, kCubeFaceCount> kFaceAxes = {{
    {{1.0F, 0.0F, 0.0F}, {0.0F, -1.0F, 0.0F}},
    {{-1.0F, 0.0F, 0.0F}, {0.0F, -1.0F, 0.0F}},
    {{0.0F, 1.0F, 0.0F}, {0.0F, 0.0F, 1.0F}},
    {{0.0F, -1.0F, 0.0F}, {0.0F, 0.0F, -1.0F}},
    {{0.0F, 0.0F, 1.0F}, {0.0F, -1.0F, 0.0F}},
    {{0.0F, 0.0F, -1.0F}, {0.0F, -1.0F, 0.0F}},
}};

}  // namespace

std::size_t ShadowedPointLightCount(const PointLightSet& set) {
  return std::min({set.count, kMaxPointLights, kMaxShadowedPointLights});
}

float PointShadowFar(float range_world) {
  constexpr float kNearestFarShare = 2.0F;
  return std::max(range_world, kPointShadowClearanceWorld * kNearestFarShare);
}

std::array<glm::mat4, kCubeFaceCount> PointShadowFaceViewProjections(glm::vec3 light_position,
                                                                     float far_world) {
  const glm::mat4 projection = glm::perspective(glm::radians(kFaceFieldDegrees), kFaceAspectRatio,
                                                kPointShadowClearanceWorld, far_world);
  std::array<glm::mat4, kCubeFaceCount> faces{};
  for (std::size_t face = 0; face < kCubeFaceCount; ++face) {
    faces[face] = projection * glm::lookAt(light_position, light_position + kFaceAxes[face].forward,
                                           kFaceAxes[face].up);
  }
  return faces;
}

glm::vec3 CubeFaceForward(std::size_t face) { return kFaceAxes.at(face).forward; }

float PointShadowDepth(float axis_distance_world, float far_world) {
  // La profondeur normalisée d'une perspective, de −1 à 1, ramenée de 0 à 1.
  const float near_world = kPointShadowClearanceWorld;
  const float normalized =
      ((far_world + near_world) / (far_world - near_world)) -
      ((2.0F * far_world * near_world) / ((far_world - near_world) * axis_distance_world));
  return (normalized * 0.5F) + 0.5F;
}

std::array<bool, kMaxShadowedPointLights> PointShadowRepaints(
    const std::array<std::optional<PaintedPointLight>, kMaxShadowedPointLights>& painted,
    const PointLightSet& set, std::size_t count, std::uint64_t frame) {
  std::array<bool, kMaxShadowedPointLights> repaint{};
  for (std::size_t rank = 0; rank < std::min(count, kMaxShadowedPointLights); ++rank) {
    const PaintedPointLight wanted{.world_position = set.lights[rank].world_position,
                                   .range_world = set.lights[rank].range_world};
    const bool moved = painted[rank] != wanted;
    const bool its_turn = rank % kPointShadowRepaintPeriod == frame % kPointShadowRepaintPeriod;
    repaint[rank] = moved || its_turn;
  }
  return repaint;
}

}  // namespace ro::render
