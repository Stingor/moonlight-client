#include "ro/render/terrain_mesh.h"

#include <algorithm>
#include <cmath>
#include <map>

#include "ro/render/terrain_relief.h"

#include "terrain_faces.h"

namespace ro::render {
namespace {

using formats::GndMap;
using formats::GndTile;

// Deux triangles à partir des quatre coins, dans l'ordre attendu par un
// remplissage en sens antihoraire.
void PushQuad(std::vector<TerrainVertex>& out, const TerrainVertex& a, const TerrainVertex& b,
              const TerrainVertex& c, const TerrainVertex& d) {
  out.push_back(a);
  out.push_back(b);
  out.push_back(c);
  out.push_back(b);
  out.push_back(d);
  out.push_back(c);
}

// La face du dessus se plie selon la diagonale qui joint le coin du fond à
// gauche à celui de devant à droite, comme le jeu.
//
// Sur un carreau dont les quatre coins ne sont pas dans un même plan, c'est la
// diagonale qui décide du relief. Plier selon l'autre creuse le sol là où le
// jeu le bombe : les bosses s'étirent en biais, et leur texture avec.
void PushTopQuad(std::vector<TerrainVertex>& out, const TerrainVertex& back_left,
                 const TerrainVertex& back_right, const TerrainVertex& front_left,
                 const TerrainVertex& front_right) {
  out.push_back(back_left);
  out.push_back(back_right);
  out.push_back(front_right);
  out.push_back(back_left);
  out.push_back(front_right);
  out.push_back(front_left);
}

// Vrai si la région de texture d'un carreau ne contient AUCUN pixel opaque.
//
// La région est le rectangle englobant des quatre coins : les carreaux du GND
// sont des rectangles alignés, et un englobant ne peut que déborder — ce qui
// rend la réponse prudente, jamais hasardeuse.
bool RegionIsClear(const formats::Image& texture, const GndTile& tile) {
  if (texture.width <= 0 || texture.height <= 0 || texture.pixels.empty()) {
    return false;
  }
  const auto [min_u, max_u] = std::minmax_element(tile.u.begin(), tile.u.end());
  const auto [min_v, max_v] = std::minmax_element(tile.v.begin(), tile.v.end());

  // Les coordonnées débordent parfois de l'unité : l'échantillonnage les borne
  // à l'image, et la recherche fait de même plutôt que de lire hors du tampon.
  const auto to_texel = [](float coordinate, int size) {
    const float clamped = std::clamp(coordinate, 0.0F, 1.0F);
    const int texel = static_cast<int>(clamped * static_cast<float>(size));
    return std::clamp(texel, 0, size - 1);
  };
  const int first_x = to_texel(*min_u, texture.width);
  const int last_x = to_texel(*max_u, texture.width);
  const int first_y = to_texel(*min_v, texture.height);
  const int last_y = to_texel(*max_v, texture.height);

  for (int y = first_y; y <= last_y; ++y) {
    for (int x = first_x; x <= last_x; ++x) {
      const std::size_t alpha =
          (static_cast<std::size_t>(y) * static_cast<std::size_t>(texture.width) +
           static_cast<std::size_t>(x)) *
              4 +
          3;
      if (alpha < texture.pixels.size() && texture.pixels[alpha] != 0) {
        return false;
      }
    }
  }
  return true;
}

}  // namespace

std::array<float, 3> TerrainMesh::center() const noexcept {
  return {(bounds_min[0] + bounds_max[0]) * 0.5F, (bounds_min[1] + bounds_max[1]) * 0.5F,
          (bounds_min[2] + bounds_max[2]) * 0.5F};
}

std::vector<std::uint8_t> FindInvisibleTiles(const GndMap& gnd,
                                             std::span<const formats::Image> textures) {
  const std::vector<GndTile>& tiles = gnd.tiles();
  if (textures.empty()) {
    return {};
  }

  std::vector<std::uint8_t> invisible(tiles.size(), 0);
  for (std::size_t index = 0; index < tiles.size(); ++index) {
    const GndTile& tile = tiles[index];
    const auto texture = static_cast<std::size_t>(tile.texture_index);
    if (tile.texture_index < 0 || texture >= textures.size()) {
      continue;
    }
    invisible[index] = RegionIsClear(textures[texture], tile) ? 1 : 0;
  }
  return invisible;
}

TerrainMesh BuildTerrainMesh(const GndMap& gnd, const LightmapAtlas& atlas,
                             std::span<const std::uint8_t> invisible_tiles,
                             std::int32_t subdivisions) {
  const TerrainFaces faces(gnd, atlas, invisible_tiles);
  if (subdivisions > kFlatGroundSubdivisions) {
    return BuildRoundedTerrainMesh(gnd, faces, subdivisions);
  }

  // Regroupement par texture : une carte porte quelques dizaines de textures
  // pour des dizaines de milliers de carreaux, donc trier les sommets une fois
  // évite autant de changements d'état au dessin.
  std::map<std::int16_t, TerrainBatch> by_texture;
  for (std::int32_t y = 0; y < gnd.height(); ++y) {
    for (std::int32_t x = 0; x < gnd.width(); ++x) {
      if (const std::optional<TerrainFace> top = faces.Top(x, y); top.has_value()) {
        const auto& corner = top->corners;
        PushTopQuad(by_texture[top->texture_index].vertices, corner[2], corner[3], corner[0],
                    corner[1]);
      }
      for (const std::optional<TerrainFace>& wall : {faces.EastWall(x, y), faces.NorthWall(x, y)}) {
        if (wall.has_value()) {
          const auto& corner = wall->corners;
          PushQuad(by_texture[wall->texture_index].vertices, corner[0], corner[1], corner[2],
                   corner[3]);
        }
      }
    }
  }
  return AssembleTerrainMesh(by_texture);
}

float GroundHeightAt(const formats::GndMap& gnd, float world_x, float world_z) {
  if (gnd.width() <= 0 || gnd.height() <= 0) {
    return 0.0F;
  }
  const CubePoint point = CubePointAt(gnd, world_x, world_z);
  const formats::GndCube& cube = gnd.at(point.cube_x, point.cube_y);
  const float near_edge = std::lerp(cube.corner_height[0], cube.corner_height[1], point.across);
  const float far_edge = std::lerp(cube.corner_height[2], cube.corner_height[3], point.across);
  // L'axe vertical du monde monte, celui du fichier descend.
  return -std::lerp(near_edge, far_edge, point.along);
}

float WalkableHeightAt(const formats::GatMap& gat, float cell_x, float cell_y) {
  if (gat.width() <= 0 || gat.height() <= 0) {
    return 0.0F;
  }
  // Un nombre entier désigne le CENTRE de sa cellule ; les coins sont donc à
  // un demi de part et d'autre.
  const float corner_x = cell_x + 0.5F;
  const float corner_y = cell_y + 0.5F;
  const auto cell_column =
      std::clamp(static_cast<std::int32_t>(std::floor(corner_x)), 0, gat.width() - 1);
  const auto cell_row =
      std::clamp(static_cast<std::int32_t>(std::floor(corner_y)), 0, gat.height() - 1);
  const float fraction_x = std::clamp(corner_x - static_cast<float>(cell_column), 0.0F, 1.0F);
  const float fraction_y = std::clamp(corner_y - static_cast<float>(cell_row), 0.0F, 1.0F);

  // Le coin 0 est en bas à gauche, le 3 en haut à droite : voir GatCell.
  const std::array<float, 4>& corners = gat.at(cell_column, cell_row).corner_height;
  const float near_edge = std::lerp(corners[0], corners[1], fraction_x);
  const float far_edge = std::lerp(corners[2], corners[3], fraction_x);
  // L'axe vertical du monde monte, celui du fichier descend.
  return -std::lerp(near_edge, far_edge, fraction_y);
}

std::array<float, 3> CellPositionAt(const formats::GndMap& gnd, float cell_x, float cell_y) {
  // Une cellule vaut la moitié d'un cube ; on vise son CENTRE, donc le quart
  // de cube suivant son coin.
  constexpr float kCellsPerCube = 2.0F;
  constexpr float kCellSize = kWorldUnitsPerCube / kCellsPerCube;

  const float world_x = (cell_x + 0.5F) * kCellSize;
  const float depth = static_cast<float>(gnd.height()) * kWorldUnitsPerCube;
  // Même inversion de l'axe Z que pour les rangées de cubes.
  const float world_z = depth + kWorldUnitsPerCube - (cell_y + 0.5F) * kCellSize;
  return {world_x, GroundHeightAt(gnd, world_x, world_z), world_z};
}

std::array<float, 2> FractionalCellAt(const formats::GndMap& gnd, float world_x, float world_z) {
  constexpr float kCellsPerCube = 2.0F;
  constexpr float kCellSize = kWorldUnitsPerCube / kCellsPerCube;

  const float depth = static_cast<float>(gnd.height()) * kWorldUnitsPerCube;
  return {world_x / kCellSize - 0.5F, (depth + kWorldUnitsPerCube - world_z) / kCellSize - 0.5F};
}

std::array<float, 3> CellCenterPosition(const formats::GndMap& gnd, std::int32_t cell_x,
                                        std::int32_t cell_y) {
  return CellPositionAt(gnd, static_cast<float>(cell_x), static_cast<float>(cell_y));
}

std::array<std::int32_t, 2> CellAtPosition(const formats::GndMap& gnd, float world_x,
                                           float world_z) {
  constexpr float kCellsPerCube = 2.0F;
  constexpr float kCellSize = kWorldUnitsPerCube / kCellsPerCube;

  // La cellule c occupe [c·t, (c+1)·t[ en X, puisque son centre est à
  // (c + 1/2)·t : c'est donc un PLANCHER, pas un arrondi. Arrondir décalerait
  // d'une demi-cellule et le personnage viserait la case voisine une fois sur
  // deux.
  const float depth = static_cast<float>(gnd.height()) * kWorldUnitsPerCube;
  const float column = world_x / kCellSize;
  const float row = (depth + kWorldUnitsPerCube - world_z) / kCellSize;
  return {static_cast<std::int32_t>(std::floor(column)),
          static_cast<std::int32_t>(std::floor(row))};
}

}  // namespace ro::render
