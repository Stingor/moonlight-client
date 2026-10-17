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

PointLightSet KeepShadowedOrder(
    const PointLightSet& set,
    const std::array<std::optional<PaintedPointLight>, kMaxShadowedPointLights>& painted,
    glm::vec3 eye) {
  const std::size_t shadowed = ShadowedPointLightCount(set);
  if (shadowed == 0) {
    return set;
  }
  const std::size_t count = std::min(set.count, kMaxPointLights);
  std::array<float, kMaxPointLights> distance{};
  // Le rang de la carte qu'une lumière porte déjà, ou rien.
  std::array<std::optional<std::size_t>, kMaxPointLights> painted_rank{};
  for (std::size_t light = 0; light < count; ++light) {
    distance[light] = glm::length(set.lights[light].world_position - eye);
    const PaintedPointLight here{.world_position = set.lights[light].world_position,
                                 .range_world = set.lights[light].range_world};
    for (std::size_t rank = 0; rank < shadowed; ++rank) {
      if (painted[rank] == here) {
        painted_rank[light] = rank;
      }
    }
  }
  // La lumière de chaque carte, puis celles qui n'en ont pas.
  std::array<std::optional<std::size_t>, kMaxShadowedPointLights> slot{};
  std::array<bool, kMaxPointLights> placed{};
  // LES LUMIÈRES DÉJÀ OMBRÉES, de la plus proche à la plus lointaine : chacune
  // garde sa carte si les plus proches qu'elle de façon nette ne la prennent pas.
  std::size_t kept = 0;
  for (std::size_t light = 0; light < count; ++light) {
    if (!painted_rank[light].has_value()) {
      continue;
    }
    std::size_t decisively_closer = 0;
    for (std::size_t other = 0; other < count; ++other) {
      if (!painted_rank[other].has_value() &&
          distance[other] < kShadowSwapRatio * distance[light]) {
        ++decisively_closer;
      }
    }
    if (kept + decisively_closer < shadowed) {
      slot[*painted_rank[light]] = light;
      placed[light] = true;
      ++kept;
    }
  }
  // LES CARTES LIBRES vont aux plus proches des autres : `set` est rangé de la
  // plus proche à la plus lointaine.
  std::size_t next = 0;
  for (std::size_t rank = 0; rank < shadowed; ++rank) {
    if (slot[rank].has_value()) {
      continue;
    }
    while (next < count && placed[next]) {
      ++next;
    }
    if (next < count) {
      slot[rank] = next;
      placed[next] = true;
    }
  }
  PointLightSet ordered = set;
  std::size_t written = 0;
  for (std::size_t rank = 0; rank < shadowed; ++rank) {
    if (slot[rank].has_value()) {
      ordered.lights[written++] = set.lights[*slot[rank]];
    }
  }
  for (std::size_t light = 0; light < count; ++light) {
    if (!placed[light]) {
      ordered.lights[written++] = set.lights[light];
    }
  }
  return ordered;
}

}  // namespace ro::render
