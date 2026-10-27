#include "ro/render/model_bake.h"

#include <algorithm>
#include <cstddef>

namespace ro::render {
namespace {

[[nodiscard]] ModelVertex Placed(const ModelVertex& vertex, const glm::mat4& transform,
                                 const glm::mat3& linear) {
  const glm::vec4 world =
      transform * glm::vec4(vertex.position[0], vertex.position[1], vertex.position[2], 1.0F);
  const glm::vec3 normal = linear * glm::vec3(vertex.normal[0], vertex.normal[1], vertex.normal[2]);
  return ModelVertex{.position = {world.x, world.y, world.z},
                     .texture_uv = vertex.texture_uv,
                     .normal = {normal.x, normal.y, normal.z}};
}

}  // namespace

BakedModel BakeInstances(const ModelMesh& mesh, std::span<const glm::mat4> transforms,
                         std::int32_t rsm_version) {
  constexpr std::uint32_t kVerticesPerFace = 3;
  BakedModel baked;
  std::vector<FaceSide> single_sides;
  single_sides.reserve(transforms.size());
  for (const glm::mat4& transform : transforms) {
    single_sides.push_back(ShowsCounterClockwise(transform, rsm_version)
                               ? FaceSide::kCounterClockwise
                               : FaceSide::kClockwise);
  }
  std::vector<ModelSurface> surfaces;
  surfaces.reserve(mesh.surfaces.size());
  std::size_t per_instance = 0;
  for (const ModelSurface& surface : mesh.surfaces) {
    const std::size_t end = std::size_t{surface.first_vertex} + surface.vertex_count;
    if (surface.vertex_count == 0 || end > mesh.vertices.size()) {
      continue;
    }
    surfaces.push_back(surface);
    per_instance += surface.vertex_count;
  }
  std::ranges::stable_sort(surfaces, {}, &ModelSurface::texture_index);

  baked.vertices.reserve(per_instance * transforms.size());
  baked.sides.reserve(per_instance * transforms.size());
  for (const ModelSurface& surface : surfaces) {
    const auto first = static_cast<std::uint32_t>(baked.vertices.size());
    for (std::size_t pose = 0; pose < transforms.size(); ++pose) {
      const glm::mat4& transform = transforms[pose];
      const glm::mat3 linear(transform);
      for (std::uint32_t offset = 0; offset < surface.vertex_count; ++offset) {
        const std::uint32_t vertex = surface.first_vertex + offset;
        baked.vertices.push_back(Placed(mesh.vertices[vertex], transform, linear));
        const std::size_t face = vertex / kVerticesPerFace;
        const bool both = face >= mesh.two_sided.size() || mesh.two_sided[face] != 0;
        baked.sides.push_back(both ? FaceSide::kBoth : single_sides[pose]);
      }
    }
    const auto count = static_cast<std::uint32_t>(baked.vertices.size()) - first;
    if (count == 0) {
      continue;
    }
    baked.surfaces.push_back(BakedSurface{.texture_index = surface.texture_index,
                                          .first_vertex = first,
                                          .vertices_per_pose = surface.vertex_count});
    if (!baked.runs.empty() && baked.runs.back().texture_index == surface.texture_index) {
      baked.runs.back().vertex_count += count;
    } else {
      baked.runs.push_back(BakedRun{
          .texture_index = surface.texture_index, .first_vertex = first, .vertex_count = count});
    }
  }
  return baked;
}

const BakedModel& BakedPosesOf(const ModelInstances& model, BakedModel& scratch) {
  if (model.baked != nullptr && model.baked_transforms == model.transforms) {
    return *model.baked;
  }
  scratch = BakeInstances(model.mesh, model.transforms, model.rsm_version);
  return scratch;
}

}  // namespace ro::render
