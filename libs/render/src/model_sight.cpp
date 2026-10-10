#include "ro/render/model_sight.h"

#include <algorithm>
#include <cmath>

#include "ro/render/terrain_mesh.h"

namespace ro::render {
namespace {

// Les bornes de l'intervalle que le client resserre, axe par axe.
constexpr float kUnbounded = 999999.0F;
// En deçà, la droite est tenue pour parallèle à la tranche.
constexpr float kParallel = 0.00001F;

// Le point visé : tant de pixels au-dessus de l'ancre, à une hauteur de 480.
constexpr float kTorsoPixels = 23.0F;
// Ce que le segment laisse avant d'atteindre le recul de la caméra.
constexpr float kSegmentShortfall = 16.0F;

constexpr std::size_t kCorners = 8;

[[nodiscard]] glm::vec3 ToVec3(const std::array<float, 3>& value) {
  return {value[0], value[1], value[2]};
}

// Un plan de notre monde, passé au repère du client.
[[nodiscard]] glm::vec4 ClientPlaneOf(const glm::vec4& plane, const ClientFrame& frame) {
  return {plane.x, -plane.y, -plane.z,
          plane.w + plane.x * frame.x_offset + plane.z * frame.z_offset};
}

}  // namespace

ClientFrame ClientFrameOf(const formats::GndMap& gnd) {
  const float half = kWorldUnitsPerCube / 2.0F;
  return ClientFrame{.x_offset = half * static_cast<float>(gnd.width()),
                     .z_offset = kWorldUnitsPerCube + half * static_cast<float>(gnd.height())};
}

SightBox SightBoxOf(const glm::mat4& instance, const ModelMesh& mesh, const ClientFrame& frame) {
  const glm::vec3 low = ToVec3(mesh.bounds_min);
  const glm::vec3 high = ToVec3(mesh.bounds_max);
  SightBox box;
  box.center = frame.PointOf(glm::vec3(instance * glm::vec4((low + high) * 0.5F, 1.0F)));
  for (glm::length_t axis = 0; axis < 3; ++axis) {
    const glm::vec3 column(instance[axis]);
    const float stretch = glm::length(column);
    const auto rank = static_cast<std::size_t>(axis);
    box.half_extents[rank] = std::abs((high[axis] - low[axis]) * stretch) * 0.5F;
    glm::vec3 unit{0.0F};
    unit[axis] = 1.0F;
    box.axes[rank] = ClientFrame::DirectionOf(stretch > 0.0F ? column / stretch : unit);
  }
  box.lowest = glm::vec3(kUnbounded);
  box.highest = glm::vec3(-kUnbounded);
  for (std::size_t corner = 0; corner < kCorners; ++corner) {
    const glm::vec3 local{(corner & 1U) != 0 ? high.x : low.x, (corner & 2U) != 0 ? high.y : low.y,
                          (corner & 4U) != 0 ? high.z : low.z};
    const glm::vec3 placed = frame.PointOf(glm::vec3(instance * glm::vec4(local, 1.0F)));
    box.lowest = glm::min(box.lowest, placed);
    box.highest = glm::max(box.highest, placed);
  }
  return box;
}

bool SegmentMeetsBox(const SightSegment& segment, const SightBox& box) {
  float entry = -kUnbounded;
  float exit = kUnbounded;
  const glm::vec3 offset = box.center - segment.origin;
  for (std::size_t axis = 0; axis < box.axes.size(); ++axis) {
    const float half = box.half_extents[axis];
    const float along = glm::dot(offset, box.axes[axis]);
    const float slope = glm::dot(segment.direction, box.axes[axis]);
    if (std::abs(slope) <= kParallel) {
      if (-along - half > 0.0F || half - along < 0.0F) {
        return false;
      }
      continue;
    }
    const float inverse = 1.0F / slope;
    const float first = (half + along) * inverse;
    const float second = (along - half) * inverse;
    if (first <= second) {
      entry = std::fmax(first, entry);
      exit = std::fmin(second, exit);
    } else {
      entry = std::fmax(second, entry);
      exit = std::fmin(first, exit);
    }
    if (entry > exit || exit < 0.0F) {
      return false;
    }
  }
  if (entry <= 0.0F) {
    return exit <= segment.length;
  }
  return entry <= segment.length;
}

SightSegment PlayerSightSegment(const Camera& camera, float aspect_ratio,
                                std::int32_t viewport_width, std::int32_t viewport_height,
                                const glm::vec3& player_world, const ClientFrame& frame) {
  const glm::vec3 eye = camera.Position();
  SightSegment segment;
  segment.origin = frame.PointOf(eye);
  segment.length = frame.PointOf(player_world).y + camera.distance - kSegmentShortfall;

  const glm::mat4 view = camera.ViewMatrix();
  const glm::mat4 projection = camera.ProjectionMatrix(aspect_ratio);
  const glm::vec4 clip = projection * view * glm::vec4(player_world, 1.0F);
  // Une fenêtre vide, ou un joueur dans le plan de l'œil, n'ont pas d'ancre : le
  // segment vise alors ses pieds.
  if (clip.w == 0.0F || viewport_width <= 0 || viewport_height <= 0) {
    segment.direction = ClientFrame::DirectionOf(glm::normalize(player_world - eye));
    return segment;
  }
  const auto width = static_cast<float>(viewport_width);
  const auto height = static_cast<float>(viewport_height);
  // L'ANCRE DU SPRITE, tronquée comme le client la range.
  const auto anchor_x = static_cast<std::int32_t>((clip.x / clip.w + 1.0F) * 0.5F * width);
  const auto anchor_y = static_cast<std::int32_t>((1.0F - clip.y / clip.w) * 0.5F * height);
  const auto aim_y = static_cast<std::int32_t>(static_cast<float>(anchor_y) -
                                               height / kClientReferenceHeight * kTorsoPixels);

  // Et le pixel ramené à la vue, par demi-fenêtres ENTIÈRES comme les siennes.
  const auto half_width = static_cast<float>(viewport_width / 2);
  const auto half_height = static_cast<float>(viewport_height / 2);
  const float across = (static_cast<float>(anchor_x) - half_width) / half_width;
  const float up = (half_height - static_cast<float>(aim_y)) / half_height;
  const glm::vec3 in_view{across / projection[0][0], up / projection[1][1], -1.0F};
  segment.direction =
      ClientFrame::DirectionOf(glm::normalize(glm::transpose(glm::mat3(view)) * in_view));
  return segment;
}

SightFrustum SightFrustumOf(const Camera& camera, float aspect_ratio, const ClientFrame& frame) {
  const glm::mat4 clip = camera.ProjectionMatrix(aspect_ratio) * camera.ViewMatrix();
  const auto row = [&clip](glm::length_t rank) {
    return glm::vec4(clip[0][rank], clip[1][rank], clip[2][rank], clip[3][rank]);
  };
  constexpr glm::length_t kDepthRow = 3;
  const glm::vec4 depth = row(kDepthRow);
  SightFrustum frustum;
  std::size_t next = 0;
  for (glm::length_t axis = 0; axis < kDepthRow; ++axis) {
    frustum.planes[next++] = ClientPlaneOf(depth + row(axis), frame);
    frustum.planes[next++] = ClientPlaneOf(depth - row(axis), frame);
  }
  return frustum;
}

FrustumSide BoxAgainstFrustum(const SightFrustum& frustum, const SightBox& box) {
  bool across = false;
  for (const glm::vec4& plane : frustum.planes) {
    const glm::vec3 normal(plane);
    const float reach = std::abs(glm::dot(box.axes[1], normal)) * box.half_extents[1] +
                        std::abs(glm::dot(box.axes[0], normal)) * box.half_extents[0] +
                        std::abs(glm::dot(box.axes[2], normal)) * box.half_extents[2];
    const float height = glm::dot(normal, box.center) + plane.w;
    if (height > reach) {
      continue;
    }
    if (height > -reach) {
      across = true;
      continue;
    }
    return FrustumSide::kOutside;
  }
  return across ? FrustumSide::kAcross : FrustumSide::kInside;
}

}  // namespace ro::render
