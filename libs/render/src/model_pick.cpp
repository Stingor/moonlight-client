#include "ro/render/model_pick.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <span>
#include <utility>

#include <glm/glm.hpp>

namespace ro::render {
namespace {

// En deçà, la direction du rayon est tenue pour parallèle à une tranche.
constexpr float kParallel = 1e-6F;

// Une boîte orientée dans le monde.
struct WorldBox {
  glm::vec3 center{};
  std::array<glm::vec3, 3> axes{};
  std::array<float, 3> half_extents{};

  [[nodiscard]] float volume() const noexcept {
    return half_extents[0] * half_extents[1] * half_extents[2];
  }
};

WorldBox BoxOf(const glm::mat4& instance, const ModelMesh& mesh) {
  const glm::vec3 low(mesh.bounds_min[0], mesh.bounds_min[1], mesh.bounds_min[2]);
  const glm::vec3 high(mesh.bounds_max[0], mesh.bounds_max[1], mesh.bounds_max[2]);
  WorldBox box;
  box.center = glm::vec3(instance * glm::vec4((low + high) * 0.5F, 1.0F));
  for (glm::length_t axis = 0; axis < 3; ++axis) {
    const glm::vec3 column(instance[axis]);
    const float stretch = glm::length(column);
    const auto rank = static_cast<std::size_t>(axis);
    box.half_extents[rank] = std::abs((high[axis] - low[axis]) * stretch) * 0.5F;
    glm::vec3 unit{0.0F};
    unit[axis] = 1.0F;
    box.axes[rank] = stretch > 0.0F ? column / stretch : unit;
  }
  return box;
}

// Le rayon traverse-t-il la boîte devant son origine ? Le test par tranches :
// axe par axe, l'entrée et la sortie de la droite, dont l'intervalle commun
// doit rester non vide et finir devant l'œil.
bool RayMeetsBox(const ScreenRay& ray, const WorldBox& box) {
  float entry = -std::numeric_limits<float>::max();
  float exit = std::numeric_limits<float>::max();
  const glm::vec3 offset = box.center - ray.origin;
  for (std::size_t axis = 0; axis < box.axes.size(); ++axis) {
    const float half = box.half_extents[axis];
    const float along = glm::dot(offset, box.axes[axis]);
    const float slope = glm::dot(ray.direction, box.axes[axis]);
    if (std::abs(slope) <= kParallel) {
      if (std::abs(along) > half) {
        return false;
      }
      continue;
    }
    float near = (along - half) / slope;
    float far = (along + half) / slope;
    if (near > far) {
      std::swap(near, far);
    }
    entry = std::max(entry, near);
    exit = std::min(exit, far);
    if (entry > exit || exit < 0.0F) {
      return false;
    }
  }
  return true;
}

}  // namespace

std::optional<std::size_t> PickSceneModel(const MapScene& scene, const ScreenRay& ray) {
  std::optional<std::size_t> picked;
  float smallest = std::numeric_limits<float>::max();
  const auto consider = [&](const ModelMesh& mesh, std::span<const glm::mat4> transforms,
                            std::span<const std::size_t> ranks) {
    // Les poses sans rang — celles d'un décor qui ne vient pas d'un RSW — ne se
    // désignent pas.
    for (std::size_t pose = 0; pose < transforms.size() && pose < ranks.size(); ++pose) {
      if (ranks[pose] == kNoRswRank) {
        continue;
      }
      const WorldBox box = BoxOf(transforms[pose], mesh);
      if (box.volume() < smallest && RayMeetsBox(ray, box)) {
        smallest = box.volume();
        picked = ranks[pose];
      }
    }
  };
  for (const ModelInstances& instances : scene.models) {
    consider(instances.mesh, instances.transforms, instances.rsw_ranks);
    for (const ModelPlaybackGroup& group : instances.animated) {
      consider(instances.mesh, group.transforms, group.rsw_ranks);
    }
  }
  return picked;
}

std::optional<ScenePose> FindScenePose(const MapScene& scene, std::size_t rsw_rank) {
  const auto find_in = [rsw_rank](std::span<const glm::mat4> transforms,
                                  std::span<const std::size_t> ranks) -> std::optional<glm::mat4> {
    for (std::size_t pose = 0; pose < transforms.size() && pose < ranks.size(); ++pose) {
      if (ranks[pose] == rsw_rank) {
        return transforms[pose];
      }
    }
    return std::nullopt;
  };
  for (const ModelInstances& instances : scene.models) {
    if (auto found = find_in(instances.transforms, instances.rsw_ranks)) {
      return ScenePose{.mesh = &instances.mesh, .transform = *found};
    }
    for (const ModelPlaybackGroup& group : instances.animated) {
      if (auto found = find_in(group.transforms, group.rsw_ranks)) {
        return ScenePose{.mesh = &instances.mesh, .transform = *found};
      }
    }
  }
  return std::nullopt;
}

std::array<glm::vec3, 8> PoseCorners(const ScenePose& pose) {
  constexpr unsigned kHighX = 1U;
  constexpr unsigned kHighY = 2U;
  constexpr unsigned kHighZ = 4U;
  const std::array<float, 3>& low = pose.mesh->bounds_min;
  const std::array<float, 3>& high = pose.mesh->bounds_max;
  std::array<glm::vec3, 8> corners{};
  for (unsigned corner = 0; corner < corners.size(); ++corner) {
    const glm::vec3 local{(corner & kHighX) != 0 ? high[0] : low[0],
                          (corner & kHighY) != 0 ? high[1] : low[1],
                          (corner & kHighZ) != 0 ? high[2] : low[2]};
    corners[corner] = glm::vec3(pose.transform * glm::vec4(local, 1.0F));
  }
  return corners;
}

}  // namespace ro::render
