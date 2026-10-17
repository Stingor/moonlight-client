#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <vector>

#include "ro/formats/gnd.h"
#include "ro/render/lightmap_atlas.h"
#include "ro/render/terrain_mesh.h"

namespace ro::render {

// LES FACES DU SOL avant toute découpe : ce que les deux constructions du
// maillage — celle d'origine et celle du sol arrondi — lisent de la même façon.
//
// Une face porte ses quatre coins tels que le jeu les pose : position, texture,
// lightmap, couleur et normale. La construction d'origine les émet tels quels ;
// celle du sol arrondi les interpole sur sa grille et ne remplace que la
// hauteur.
struct TerrainFace {
  std::int16_t texture_index = 0;
  std::array<TerrainVertex, 4> corners{};
};

class TerrainFaces {
 public:
  TerrainFaces(const formats::GndMap& gnd, const LightmapAtlas& atlas,
               std::span<const std::uint8_t> invisible_tiles);

  // Le dessus du cube, ses coins dans l'ordre du GND : proche gauche, proche
  // droite, fond gauche, fond droite. Rien quand il ne s'émet pas.
  [[nodiscard]] std::optional<TerrainFace> Top(std::int32_t cube_x, std::int32_t cube_y) const;

  // La paroi entre le cube et son voisin de droite, puis celle entre le cube et
  // son voisin du fond. Les coins 0 et 1 sont au bord du cube, les coins 2 et 3
  // au bord du voisin ; chaque paire va du fond vers l'avant pour la première,
  // de la gauche vers la droite pour la seconde.
  [[nodiscard]] std::optional<TerrainFace> EastWall(std::int32_t cube_x, std::int32_t cube_y) const;
  [[nodiscard]] std::optional<TerrainFace> NorthWall(std::int32_t cube_x,
                                                     std::int32_t cube_y) const;

 private:
  [[nodiscard]] const formats::GndTile* FaceTileAt(std::int32_t tile_index) const;
  [[nodiscard]] std::array<float, 4> CornerColor(std::int32_t cube_x, std::int32_t cube_y) const;

  const formats::GndMap& gnd_;
  const LightmapAtlas& atlas_;
  std::span<const std::uint8_t> invisible_tiles_;
};

// Les sommets d'une texture, et leurs indices quand le maillage en a.
struct TerrainBatch {
  std::vector<TerrainVertex> vertices;
  std::vector<std::uint32_t> indices;
};

// Range les lots par texture dans un seul maillage, puis en prend l'emprise.
[[nodiscard]] TerrainMesh AssembleTerrainMesh(const std::map<std::int16_t, TerrainBatch>& batches);

// La construction du sol arrondi : voir `BuildTerrainMesh`.
[[nodiscard]] TerrainMesh BuildRoundedTerrainMesh(const formats::GndMap& gnd,
                                                  const TerrainFaces& faces,
                                                  std::int32_t subdivisions);

}  // namespace ro::render
