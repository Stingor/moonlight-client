#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

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

// LA GRILLE DES BLOCS : de quoi ne parcourir que ceux qu'une sphère peut toucher.
// Elle couvre l'étendue des blocs existants, et ses cases sans bloc restent à
// `kNoDepthChunk`, car le sol a des trous.
inline constexpr std::uint32_t kNoDepthChunk = 0xFFFFFFFFU;

struct TerrainDepthGrid {
  std::int64_t first_column = 0;
  std::int64_t first_row = 0;
  std::int64_t columns = 0;
  std::int64_t rows = 0;
  // De combien l'emprise d'un bloc déborde au plus de sa case, en unités du
  // monde : un triangle s'y range par son centre, et ses coins en sortent.
  float overhang_world = 0.0F;
  // Le rang du bloc de chaque case, colonne par colonne.
  std::vector<std::uint32_t> chunk_of_place;

  // Appelle `visit` avec le rang de chaque bloc dont l'emprise peut toucher la
  // sphère, dans l'ordre de `TerrainDepthChunks::chunks`. Peut en nommer un que
  // la sphère ne touche pas : c'est à l'appelant de tester son emprise.
  template <typename Visit>
  void ForEachNear(glm::vec3 center, float radius_world, Visit&& visit) const {
    const float reach = radius_world + overhang_world;
    // La dernière case qui commence avant `world`, et la première qui finit
    // après : une emprise qui ne fait que toucher la sphère compte.
    const auto last_starting_by = [](float world) {
      return static_cast<std::int64_t>(std::floor(world / kTerrainDepthChunkWorld));
    };
    const auto first_ending_from = [](float world) {
      return static_cast<std::int64_t>(std::ceil(world / kTerrainDepthChunkWorld)) - 1;
    };
    const std::int64_t from_column = std::max(first_ending_from(center.x - reach), first_column);
    const std::int64_t to_column =
        std::min(last_starting_by(center.x + reach), first_column + columns - 1);
    const std::int64_t from_row = std::max(first_ending_from(center.z - reach), first_row);
    const std::int64_t to_row = std::min(last_starting_by(center.z + reach), first_row + rows - 1);
    for (std::int64_t column = from_column; column <= to_column; ++column) {
      for (std::int64_t row = from_row; row <= to_row; ++row) {
        const auto place =
            static_cast<std::size_t>((column - first_column) * rows + row - first_row);
        if (chunk_of_place[place] != kNoDepthChunk) {
          visit(chunk_of_place[place]);
        }
      }
    }
  }
};

struct TerrainDepthChunks {
  // Les indices de tous les triangles, rangés bloc par bloc, puis surface par
  // surface : ils désignent les sommets de `TerrainMesh::vertices`.
  std::vector<std::uint32_t> indices;
  // Rangés par colonne de la grille, puis par rangée.
  std::vector<TerrainDepthChunk> chunks;
  TerrainDepthGrid grid;
};

// Range chaque triangle de `mesh` dans le bloc où tombe son centre. Un
// triangle n'appartient qu'à un bloc, dont l'emprise s'élargit pour le tenir
// tout entier.
[[nodiscard]] TerrainDepthChunks BuildTerrainDepthChunks(const TerrainMesh& mesh);

}  // namespace ro::render
