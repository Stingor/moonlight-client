#include "ro/render/terrain_depth_chunks.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

namespace ro::render {
namespace {

constexpr std::size_t kTriangleCorners = 3;

// La place d'un bloc dans la grille, en blocs.
using ChunkPlace = std::pair<std::int64_t, std::int64_t>;

[[nodiscard]] glm::vec3 PositionOf(const TerrainVertex& vertex) {
  return {vertex.position[0], vertex.position[1], vertex.position[2]};
}

}  // namespace

TerrainDepthChunks BuildTerrainDepthChunks(const TerrainMesh& mesh) {
  // Par bloc, puis par surface : les indices de ses triangles.
  std::map<ChunkPlace, std::map<std::size_t, std::vector<std::uint32_t>>> gathered;
  std::map<ChunkPlace, WorldBox> boxes;
  const bool indexed = !mesh.indices.empty();
  for (std::size_t surface = 0; surface < mesh.surfaces.size(); ++surface) {
    const TerrainSurface& lot = mesh.surfaces[surface];
    const std::size_t count = indexed ? lot.index_count : lot.vertex_count;
    for (std::size_t corner = 0; corner + kTriangleCorners <= count; corner += kTriangleCorners) {
      std::array<std::uint32_t, kTriangleCorners> triangle{};
      bool inside = true;
      for (std::size_t rank = 0; rank < kTriangleCorners && inside; ++rank) {
        const std::size_t at = (indexed ? lot.first_index : lot.first_vertex) + corner + rank;
        if (indexed && at >= mesh.indices.size()) {
          inside = false;
          break;
        }
        triangle[rank] = indexed ? mesh.indices[at] : static_cast<std::uint32_t>(at);
        inside = triangle[rank] < mesh.vertices.size();
      }
      if (!inside) {
        continue;
      }
      glm::vec3 center(0.0F);
      WorldBox span{.lowest = glm::vec3(std::numeric_limits<float>::infinity()),
                    .highest = glm::vec3(-std::numeric_limits<float>::infinity())};
      for (const std::uint32_t vertex : triangle) {
        const glm::vec3 position = PositionOf(mesh.vertices[vertex]);
        center += position;
        span.lowest = glm::min(span.lowest, position);
        span.highest = glm::max(span.highest, position);
      }
      center /= static_cast<float>(kTriangleCorners);
      const ChunkPlace place{
          static_cast<std::int64_t>(std::floor(center.x / kTerrainDepthChunkWorld)),
          static_cast<std::int64_t>(std::floor(center.z / kTerrainDepthChunkWorld))};
      std::vector<std::uint32_t>& indices = gathered[place][surface];
      indices.insert(indices.end(), triangle.begin(), triangle.end());
      const auto [found, fresh] = boxes.try_emplace(place, span);
      if (!fresh) {
        found->second.lowest = glm::min(found->second.lowest, span.lowest);
        found->second.highest = glm::max(found->second.highest, span.highest);
      }
    }
  }

  TerrainDepthChunks chunks;
  chunks.chunks.reserve(gathered.size());
  for (const auto& [place, surfaces] : gathered) {
    TerrainDepthChunk chunk{.box = boxes.at(place), .runs = {}};
    for (const auto& [surface, indices] : surfaces) {
      chunk.runs.push_back(
          TerrainDepthRun{.surface = surface,
                          .first_index = static_cast<std::uint32_t>(chunks.indices.size()),
                          .index_count = static_cast<std::uint32_t>(indices.size())});
      chunks.indices.insert(chunks.indices.end(), indices.begin(), indices.end());
    }
    chunks.chunks.push_back(std::move(chunk));
  }
  return chunks;
}

}  // namespace ro::render
