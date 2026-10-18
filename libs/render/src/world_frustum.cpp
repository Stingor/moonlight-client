#include "ro/render/world_frustum.h"

#include <algorithm>
#include <cstddef>
#include <limits>

namespace ro::render {

WorldFrustum WorldFrustum::Of(const glm::mat4& view_projection) {
  // Les plans d'un tronc se lisent dans les lignes de la matrice : la ligne de
  // profondeur homogène, plus ou moins chacune des trois autres.
  const auto row = [&view_projection](glm::length_t rank) {
    return glm::vec4(view_projection[0][rank], view_projection[1][rank], view_projection[2][rank],
                     view_projection[3][rank]);
  };
  constexpr glm::length_t kHomogeneousRow = 3;
  const glm::vec4 homogeneous = row(kHomogeneousRow);
  WorldFrustum frustum;
  std::size_t next = 0;
  for (glm::length_t axis = 0; axis < kHomogeneousRow; ++axis) {
    for (const float side : {1.0F, -1.0F}) {
      glm::vec4 plane = homogeneous + (side * row(axis));
      // Normé, pour qu'une hauteur au-dessus du plan se compte en unités du monde.
      const float length = glm::length(glm::vec3(plane));
      frustum.planes_[next++] = length > 0.0F ? plane / length : plane;
    }
  }
  return frustum;
}

bool WorldFrustum::SeesSphere(glm::vec3 center, float radius) const {
  return std::ranges::all_of(planes_, [&](const glm::vec4& plane) {
    return glm::dot(glm::vec3(plane), center) + plane.w >= -radius;
  });
}

bool WorldFrustum::SeesBox(const WorldBox& box) const {
  return std::ranges::all_of(planes_, [&box](const glm::vec4& plane) {
    // Le coin le plus avancé du côté intérieur du plan.
    const glm::vec3 farthest(plane.x >= 0.0F ? box.highest.x : box.lowest.x,
                             plane.y >= 0.0F ? box.highest.y : box.lowest.y,
                             plane.z >= 0.0F ? box.highest.z : box.lowest.z);
    return glm::dot(glm::vec3(plane), farthest) + plane.w >= 0.0F;
  });
}

WorldBox TransformedBox(const WorldBox& box, const glm::mat4& transform) {
  WorldBox moved{.lowest = glm::vec3(std::numeric_limits<float>::infinity()),
                 .highest = glm::vec3(-std::numeric_limits<float>::infinity())};
  constexpr int kCorners = 8;
  for (int corner = 0; corner < kCorners; ++corner) {
    const glm::vec3 local((corner & 1) != 0 ? box.highest.x : box.lowest.x,
                          (corner & 2) != 0 ? box.highest.y : box.lowest.y,
                          (corner & 4) != 0 ? box.highest.z : box.lowest.z);
    const glm::vec3 placed(transform * glm::vec4(local, 1.0F));
    moved.lowest = glm::min(moved.lowest, placed);
    moved.highest = glm::max(moved.highest, placed);
  }
  return moved;
}

bool SphereMeetsBox(glm::vec3 center, float radius, const WorldBox& box) {
  const glm::vec3 nearest = glm::clamp(center, box.lowest, box.highest);
  const glm::vec3 offset = center - nearest;
  return glm::dot(offset, offset) <= radius * radius;
}

}  // namespace ro::render
