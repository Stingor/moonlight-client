#include "ro/render/terrain_depth_chunks.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

namespace ro::render {
namespace {

constexpr std::size_t kTriangleCorners = 3;

using CornerIndices = std::array<std::uint32_t, kTriangleCorners>;

[[nodiscard]] glm::vec3 PositionOf(const TerrainVertex& vertex) {
  return {vertex.position[0], vertex.position[1], vertex.position[2]};
}

// Le triangle qui commence à `corner` dans `lot` ; rien s'il sort du maillage.
[[nodiscard]] std::optional<CornerIndices> TriangleAt(const TerrainMesh& mesh,
                                                      const TerrainSurface& lot,
                                                      std::size_t corner) {
  const bool indexed = !mesh.indices.empty();
  CornerIndices triangle{};
  for (std::size_t rank = 0; rank < kTriangleCorners; ++rank) {
    const std::size_t at = (indexed ? lot.first_index : lot.first_vertex) + corner + rank;
    if (indexed && at >= mesh.indices.size()) {
      return std::nullopt;
    }
    triangle[rank] = indexed ? mesh.indices[at] : static_cast<std::uint32_t>(at);
    if (triangle[rank] >= mesh.vertices.size()) {
      return std::nullopt;
    }
  }
  return triangle;
}

// Appelle `visit(surface, triangle)` pour chaque triangle du maillage.
template <typename Visit>
void ForEachTriangle(const TerrainMesh& mesh, Visit&& visit) {
  const bool indexed = !mesh.indices.empty();
  for (std::size_t surface = 0; surface < mesh.surfaces.size(); ++surface) {
    const TerrainSurface& lot = mesh.surfaces[surface];
    const std::size_t count = indexed ? lot.index_count : lot.vertex_count;
    for (std::size_t corner = 0; corner + kTriangleCorners <= count; corner += kTriangleCorners) {
      if (const std::optional<CornerIndices> triangle = TriangleAt(mesh, lot, corner)) {
        visit(surface, *triangle);
      }
    }
  }
}

// La colonne et la rangée d'un bloc.
struct ChunkPlace {
  std::int64_t column = 0;
  std::int64_t row = 0;
};

[[nodiscard]] ChunkPlace PlaceAt(const glm::vec3& world) {
  return {static_cast<std::int64_t>(std::floor(world.x / kTerrainDepthChunkWorld)),
          static_cast<std::int64_t>(std::floor(world.z / kTerrainDepthChunkWorld))};
}

// Les triangles d'une surface dans une case.
struct GatheredRun {
  std::size_t surface = 0;
  std::vector<std::uint32_t> indices;
};

// Une case de la grille pendant le rangement.
struct GatheredPlace {
  WorldBox box{.lowest = glm::vec3(std::numeric_limits<float>::infinity()),
               .highest = glm::vec3(-std::numeric_limits<float>::infinity())};
  std::vector<GatheredRun> runs;
};

}  // namespace

TerrainDepthChunks BuildTerrainDepthChunks(const TerrainMesh& mesh) {
  // L'ÉTENDUE DE LA GRILLE, par les sommets : le centre d'un triangle se tient
  // entre ses coins. Un sommet dont la place n'est pas un nombre rend son
  // triangle inrangeable.
  const auto placeable = [](const glm::vec3& position) {
    return std::isfinite(position.x) && std::isfinite(position.z);
  };
  std::optional<ChunkPlace> lowest;
  ChunkPlace highest;
  for (const TerrainVertex& vertex : mesh.vertices) {
    const glm::vec3 position = PositionOf(vertex);
    if (!placeable(position)) {
      continue;
    }
    const ChunkPlace place = PlaceAt(position);
    if (!lowest.has_value()) {
      lowest = place;
      highest = place;
    }
    lowest->column = std::min(lowest->column, place.column);
    lowest->row = std::min(lowest->row, place.row);
    highest.column = std::max(highest.column, place.column);
    highest.row = std::max(highest.row, place.row);
  }

  TerrainDepthChunks chunks;
  if (!lowest.has_value()) {
    return chunks;
  }
  TerrainDepthGrid& grid = chunks.grid;
  grid.first_column = lowest->column;
  grid.first_row = lowest->row;
  grid.columns = highest.column - lowest->column + 1;
  grid.rows = highest.row - lowest->row + 1;
  const auto place_count = static_cast<std::size_t>(grid.columns * grid.rows);
  const auto rank_of = [&grid](const ChunkPlace& place) {
    return static_cast<std::size_t>((place.column - grid.first_column) * grid.rows + place.row -
                                    grid.first_row);
  };

  // LES SURFACES ARRIVENT DANS L'ORDRE : un triangle prolonge la dernière de sa
  // case, ou en ouvre une.
  std::vector<GatheredPlace> gathered(place_count);
  ForEachTriangle(mesh, [&](std::size_t surface, const CornerIndices& triangle) {
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
    if (!placeable(center)) {
      return;
    }
    GatheredPlace& at = gathered[rank_of(PlaceAt(center))];
    if (at.runs.empty() || at.runs.back().surface != surface) {
      at.runs.push_back(GatheredRun{.surface = surface, .indices = {}});
    }
    at.runs.back().indices.insert(at.runs.back().indices.end(), triangle.begin(), triangle.end());
    at.box.lowest = glm::min(at.box.lowest, span.lowest);
    at.box.highest = glm::max(at.box.highest, span.highest);
  });

  grid.chunk_of_place.assign(place_count, kNoDepthChunk);
  for (std::size_t place = 0; place < place_count; ++place) {
    GatheredPlace& at = gathered[place];
    if (at.runs.empty()) {
      continue;
    }
    grid.chunk_of_place[place] = static_cast<std::uint32_t>(chunks.chunks.size());
    const auto column = grid.first_column + static_cast<std::int64_t>(place) / grid.rows;
    const auto row = grid.first_row + static_cast<std::int64_t>(place) % grid.rows;
    const float west = static_cast<float>(column) * kTerrainDepthChunkWorld;
    const float south = static_cast<float>(row) * kTerrainDepthChunkWorld;
    grid.overhang_world =
        std::max({grid.overhang_world, west - at.box.lowest.x,
                  at.box.highest.x - (west + kTerrainDepthChunkWorld), south - at.box.lowest.z,
                  at.box.highest.z - (south + kTerrainDepthChunkWorld)});
    TerrainDepthChunk chunk{.box = at.box, .runs = {}};
    chunk.runs.reserve(at.runs.size());
    for (const GatheredRun& run : at.runs) {
      chunk.runs.push_back(
          TerrainDepthRun{.surface = run.surface,
                          .first_index = static_cast<std::uint32_t>(chunks.indices.size()),
                          .index_count = static_cast<std::uint32_t>(run.indices.size())});
      chunks.indices.insert(chunks.indices.end(), run.indices.begin(), run.indices.end());
    }
    chunks.chunks.push_back(std::move(chunk));
  }
  return chunks;
}

}  // namespace ro::render
