#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "ro/render/terrain_mesh.h"
#include "ro/render/world_frustum.h"

namespace ro::render {

// LE SOL EN BLOCS, pour qu'une carte d'ombre ponctuelle ne peigne que ce que
// voit sa face : une lanterne ne redessine pas toute la carte.

// Le côté d'un bloc, en unités du monde : quatre cubes du GND.
inline constexpr float kTerrainDepthChunkWorld = 40.0F;

// Les triangles d'une surface tombés dans un bloc : `index_count` indices à
// partir de `first_index` dans `TerrainDepthChunks::indices`.
struct TerrainDepthRun {
  std::size_t surface = 0;
  std::uint32_t first_index = 0;
  std::uint32_t index_count = 0;
};

struct TerrainDepthChunk {
  // L'emprise de tous ses triangles.
  WorldBox box;
  std::vector<TerrainDepthRun> runs;
};

struct TerrainDepthChunks {
  // Les indices de tous les triangles, rangés bloc par bloc, puis surface par
  // surface : ils désignent les sommets de `TerrainMesh::vertices`.
  std::vector<std::uint32_t> indices;
  std::vector<TerrainDepthChunk> chunks;
};

// Range chaque triangle de `mesh` dans le bloc où tombe son centre. Un
// triangle n'appartient qu'à un bloc, dont l'emprise s'élargit pour le tenir
// tout entier.
[[nodiscard]] TerrainDepthChunks BuildTerrainDepthChunks(const TerrainMesh& mesh);

}  // namespace ro::render
