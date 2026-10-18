#include "ro/render/model_pose_boxes.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include <glm/glm.hpp>

namespace ro::render {
namespace {

// L'emprise de `vertices`, élargie à celle de `box`.
void Enclose(const std::vector<ModelVertex>& vertices, WorldBox& box) {
  for (const ModelVertex& vertex : vertices) {
    const glm::vec3 position(vertex.position[0], vertex.position[1], vertex.position[2]);
    box.lowest = glm::min(box.lowest, position);
    box.highest = glm::max(box.highest, position);
  }
}

}  // namespace

ModelPoseBoxes PoseBoxesOf(const ModelInstances& model) {
  const WorldBox empty{.lowest = glm::vec3(std::numeric_limits<float>::max()),
                       .highest = glm::vec3(std::numeric_limits<float>::lowest())};
  WorldBox local = empty;
  Enclose(model.mesh.vertices, local);
  // UNE POSE ANIMÉE, à l'image zéro de son gréement : chaque nœud porte ses
  // sommets.
  WorldBox moving = empty;
  if (!model.rig.empty() && !model.animated.empty()) {
    const std::vector<glm::mat4> rest = PoseRig(model.rig, 0.0F);
    for (const ModelRigPart& part : model.rig.parts) {
      const glm::mat4 node = part.node < rest.size() ? rest[part.node] : glm::mat4(1.0F);
      for (const ModelSurface& surface : part.surfaces) {
        const std::size_t end = std::min<std::size_t>(surface.first_vertex + surface.vertex_count,
                                                      model.rig.vertices.size());
        for (std::size_t vertex = surface.first_vertex; vertex < end; ++vertex) {
          const std::array<float, 3>& at = model.rig.vertices[vertex].position;
          const glm::vec3 posed(node * glm::vec4(at[0], at[1], at[2], 1.0F));
          moving.lowest = glm::min(moving.lowest, posed);
          moving.highest = glm::max(moving.highest, posed);
        }
      }
    }
  }
  if (local.lowest.x > local.highest.x) {
    local = WorldBox();
  }
  if (moving.lowest.x > moving.highest.x) {
    moving = local;
  }
  const glm::vec3 reach = (moving.highest - moving.lowest) * kAnimatedPoseReachShare;
  moving.lowest -= reach;
  moving.highest += reach;

  ModelPoseBoxes poses;
  poses.still_poses = model.transforms.size();
  for (const glm::mat4& transform : model.transforms) {
    poses.boxes.push_back(TransformedBox(local, transform));
  }
  for (const ModelPlaybackGroup& group : model.animated) {
    for (const glm::mat4& transform : group.transforms) {
      poses.boxes.push_back(TransformedBox(moving, transform));
    }
  }
  return poses;
}

bool SightedCasting(std::span<const ModelPoseBoxes> boxes, const ModelOpacities* casting,
                    const ShadowSight& sight, ModelOpacities& into) {
  into.resize(boxes.size());
  bool moving_seen = false;
  for (std::size_t model = 0; model < boxes.size(); ++model) {
    const ModelPoseBoxes& poses = boxes[model];
    std::vector<std::uint8_t>& kept = into[model];
    kept.assign(poses.boxes.size(), 0);
    const std::vector<std::uint8_t>* casts =
        casting != nullptr && model < casting->size() ? &(*casting)[model] : nullptr;
    for (std::size_t pose = 0; pose < poses.boxes.size(); ++pose) {
      const bool full = casts == nullptr || pose >= casts->size() || (*casts)[pose] == kVeilOpaque;
      if (full && sight.Sees(poses.boxes[pose])) {
        kept[pose] = kVeilOpaque;
        moving_seen = moving_seen || pose >= poses.still_poses;
      }
    }
  }
  return moving_seen;
}

}  // namespace ro::render
