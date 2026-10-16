#include "world/map_editor_gizmo.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

#include "ro/render/actor_pick.h"

namespace moonclient {
namespace {

// Le nombre de points d'un anneau de rotation.
constexpr std::size_t kRingPoints = 32;
// La demi-largeur du carré de l'échelle uniforme, en pixels.
constexpr float kUniformHalfPixels = 6.0F;
// En deçà, un axe à l'écran est tenu pour un point : il se voit par la tranche.
constexpr float kShortestScreenAxis = 1.0F;

ro::ui::MapEditorLineKind KindOf(GizmoAxis axis) {
  switch (axis) {
    case GizmoAxis::kX:
      return ro::ui::MapEditorLineKind::kAxisX;
    case GizmoAxis::kY:
      return ro::ui::MapEditorLineKind::kAxisY;
    case GizmoAxis::kZ:
      return ro::ui::MapEditorLineKind::kAxisZ;
    case GizmoAxis::kAll:
      return ro::ui::MapEditorLineKind::kSelection;
  }
  return ro::ui::MapEditorLineKind::kSelection;
}

// La distance d'un point à un segment, en pixels.
float DistanceToSegment(glm::vec2 point, glm::vec2 from, glm::vec2 to) {
  const glm::vec2 along = to - from;
  const float length_squared = glm::dot(along, along);
  const float t = length_squared > 0.0F
                      ? std::clamp(glm::dot(point - from, along) / length_squared, 0.0F, 1.0F)
                      : 0.0F;
  return glm::length(point - (from + along * t));
}

// Un vecteur perpendiculaire à `axis`, normé.
glm::vec3 PerpendicularTo(const glm::vec3& axis) {
  const glm::vec3 helper =
      std::abs(axis.y) < 0.9F ? glm::vec3(0.0F, 1.0F, 0.0F) : glm::vec3(1.0F, 0.0F, 0.0F);
  return glm::normalize(glm::cross(axis, helper));
}

}  // namespace

GizmoFrame GizmoFrameOf(const ro::render::MapEffectFrame& map_frame,
                        const std::array<float, 3>& rsw_position,
                        const ro::render::Camera& camera) {
  const glm::vec3 native(rsw_position[0], rsw_position[1], rsw_position[2]);
  GizmoFrame frame;
  frame.origin = map_frame.ToWorld(native);
  for (std::size_t axis = 0; axis < frame.axes.size(); ++axis) {
    glm::vec3 step(0.0F);
    step[static_cast<glm::length_t>(axis)] = 1.0F;
    frame.axes[axis] = glm::normalize(map_frame.ToWorld(native + step) - frame.origin);
  }
  frame.length = camera.distance * kGizmoLengthShare;
  return frame;
}

std::vector<GizmoHandle> GizmoHandles(ro::ui::MapEditorTool tool, const GizmoFrame& frame,
                                      const ro::render::Camera& camera, float aspect_ratio,
                                      float viewport_width, float viewport_height) {
  const auto project = [&](const glm::vec3& world) {
    return ro::render::ProjectToScreen(camera, aspect_ratio, world, viewport_width,
                                       viewport_height);
  };
  std::vector<GizmoHandle> handles;
  const std::optional<glm::vec2> origin = project(frame.origin);
  if (!origin.has_value()) {
    return handles;
  }
  for (std::size_t rank = 0; rank < frame.axes.size(); ++rank) {
    const auto axis = static_cast<GizmoAxis>(rank);
    const glm::vec3& direction = frame.axes[rank];
    GizmoHandle handle{.axis = axis};
    if (tool == ro::ui::MapEditorTool::kRotate) {
      // L'ANNEAU : un cercle autour de l'axe, d'un rayon d'une longueur.
      const glm::vec3 first = PerpendicularTo(direction);
      const glm::vec3 second = glm::cross(direction, first);
      handle.closed = true;
      bool whole = true;
      for (std::size_t point = 0; point < kRingPoints && whole; ++point) {
        const float angle = 2.0F * std::numbers::pi_v<float> * static_cast<float>(point) /
                            static_cast<float>(kRingPoints);
        const std::optional<glm::vec2> projected = project(
            frame.origin + (first * std::cos(angle) + second * std::sin(angle)) * frame.length);
        whole = projected.has_value();
        if (whole) {
          handle.points.push_back(*projected);
        }
      }
      if (whole) {
        handles.push_back(std::move(handle));
      }
      continue;
    }
    // LA FLÈCHE : du pivot à une longueur le long de l'axe.
    const std::optional<glm::vec2> tip = project(frame.origin + direction * frame.length);
    if (!tip.has_value()) {
      continue;
    }
    handle.points = {*origin, *tip};
    handle.screen_axis = *tip - *origin;
    handles.push_back(std::move(handle));
  }
  if (tool == ro::ui::MapEditorTool::kScale) {
    // LE CARRÉ DU PIVOT : l'échelle uniforme.
    const glm::vec2 corner(kUniformHalfPixels, kUniformHalfPixels);
    const glm::vec2 other(kUniformHalfPixels, -kUniformHalfPixels);
    handles.push_back(GizmoHandle{
        .axis = GizmoAxis::kAll,
        .points = {*origin - corner, *origin + other, *origin + corner, *origin - other},
        .closed = true});
  }
  return handles;
}

std::optional<std::size_t> GrabbedHandle(const std::vector<GizmoHandle>& handles,
                                         glm::vec2 pointer) {
  std::optional<std::size_t> grabbed;
  float nearest = kGizmoGrabPixels;
  for (std::size_t rank = 0; rank < handles.size(); ++rank) {
    const GizmoHandle& handle = handles[rank];
    const std::size_t count = handle.points.size();
    const std::size_t segments = handle.closed ? count : (count > 0 ? count - 1 : 0);
    for (std::size_t segment = 0; segment < segments; ++segment) {
      const float distance =
          DistanceToSegment(pointer, handle.points[segment], handle.points[(segment + 1) % count]);
      if (distance <= nearest) {
        nearest = distance;
        grabbed = rank;
      }
    }
  }
  return grabbed;
}

ro::ui::map_editor::Place Dragged(ro::ui::MapEditorTool tool, const GizmoHandle& handle,
                                  const GizmoFrame& frame, const ro::ui::map_editor::Place& start,
                                  glm::vec2 drag) {
  ro::ui::map_editor::Place placed = start;
  if (handle.axis == GizmoAxis::kAll) {
    // Vers la droite ou vers le haut grandit : l'ordonnée d'écran descend.
    // ⚠ BORNÉ À ZÉRO : un glissé qui dépasse ne retourne pas le modèle.
    const float factor = std::max(1.0F + (drag.x - drag.y) * kGizmoUniformScalePerPixel, 0.0F);
    for (float& scale : placed.scale) {
      const float scaled = scale * factor;
      scale = std::copysign(std::max(std::abs(scaled), kGizmoSmallestScale), scale);
    }
    return placed;
  }
  const auto axis = static_cast<std::size_t>(handle.axis);
  if (tool == ro::ui::MapEditorTool::kRotate) {
    placed.rotation[axis] = start.rotation[axis] + drag.x * kGizmoDegreesPerPixel;
    return placed;
  }
  // LE GLISSÉ PROJETÉ SUR L'AXE : la part d'une longueur qu'il parcourt.
  const float screen_length_squared = glm::dot(handle.screen_axis, handle.screen_axis);
  if (screen_length_squared < kShortestScreenAxis) {
    return placed;
  }
  const float along = glm::dot(drag, handle.screen_axis) / screen_length_squared;
  if (tool == ro::ui::MapEditorTool::kMove) {
    placed.position[axis] = start.position[axis] + along * frame.length;
  } else {
    const float scaled = start.scale[axis] * std::max(1.0F + along, 0.0F);
    placed.scale[axis] =
        std::copysign(std::max(std::abs(scaled), kGizmoSmallestScale), start.scale[axis]);
  }
  return placed;
}

std::vector<ro::ui::MapEditorLine> GizmoLines(const std::vector<GizmoHandle>& handles,
                                              std::optional<std::size_t> active) {
  std::vector<ro::ui::MapEditorLine> lines;
  for (std::size_t rank = 0; rank < handles.size(); ++rank) {
    const GizmoHandle& handle = handles[rank];
    const ro::ui::MapEditorLineKind kind =
        active == rank ? ro::ui::MapEditorLineKind::kActive : KindOf(handle.axis);
    const std::size_t count = handle.points.size();
    const std::size_t segments = handle.closed ? count : (count > 0 ? count - 1 : 0);
    for (std::size_t segment = 0; segment < segments; ++segment) {
      lines.push_back(ro::ui::MapEditorLine{.from = handle.points[segment],
                                            .to = handle.points[(segment + 1) % count],
                                            .kind = kind});
    }
  }
  return lines;
}

}  // namespace moonclient
