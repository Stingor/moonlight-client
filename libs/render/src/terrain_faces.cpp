#include "terrain_faces.h"

#include <algorithm>
#include <limits>

namespace ro::render {
namespace {

using formats::GndCube;
using formats::GndTile;

// Blanc là où il n'y a rien à lire : au-delà du bord de la carte, et sur une
// cellule trouée. C'est ce que fait la référence.
constexpr std::array<float, 4> kNoTint = {1.0F, 1.0F, 1.0F, 1.0F};

std::array<float, 4> TileColor(const GndTile& tile) {
  // Le fichier range la couleur BLEU, VERT, ROUGE, ALPHA — l'ordre d'un pixel
  // Windows, pas celui d'un nom de couleur. La lire comme ARGB échange le rouge
  // et le bleu, et prend l'alpha pour du rouge : sur une carte dont tous les
  // carreaux sont blancs, cela ne se voit pas ; sur les autres, si.
  return {static_cast<float>(tile.color[2]) / 255.0F, static_cast<float>(tile.color[1]) / 255.0F,
          static_cast<float>(tile.color[0]) / 255.0F, static_cast<float>(tile.color[3]) / 255.0F};
}

// La normale des faces du dessus : la verticale, quelle que soit la pente.
//
// POURQUOI LA PENTE NE COMPTE PAS
//
// Le sol de RO ne s'ombre pas selon son inclinaison. La direction du soleil est
// déjà CUITE dans la lightmap de la carte ; la rejouer par un produit scalaire
// contre la normale du carreau ombrerait le relief une seconde fois, et le sol
// se couvrirait de taches carrées d'une cellule, chacune éclairée d'un bloc.
//
// Ce n'est pas une simplification : c'est mesuré. Sur gonryun, en confrontant
// notre rendu à une capture du client sur les fenêtres unies des deux images,
// le facteur appliqué par le client vaut 1,00 sur TOUTE l'étendue de notre
// éclairage calculé, de 0,60 à 1,00 — aucune corrélation avec l'inclinaison.
// Le rendre plat porte le rapport des zones éclairées de 0,90 à 0,999.
//
// Les faces LATÉRALES gardent la leur : elles sont verticales, la lightmap ne
// les couvre pas de la même façon, et rien dans la mesure ne les concerne.
//
// L'éclairage CALCULÉ — lumières ponctuelles, ombres en temps réel — n'est pas
// cuit : il lit la forme, par `TerrainVertex::shape_normal`.
constexpr std::array<float, 3> kGroundNormal{0.0F, 1.0F, 0.0F};

// Vrai si une paroi regarde vers le VOISIN, faux si elle regarde vers le cube
// qui la porte.
//
// UNE PAROI REGARDE LE CUBE LE PLUS BAS : c'est de son côté qu'elle se voit, et
// c'est ce côté-là que le soleil éclaire ou non. Les deux parois suivent la
// même règle, et une normale qui regarde vers le cube haut retourne leur
// éclairage : la paroi au soleil sort sombre, celle à l'ombre claire, et le
// défaut change de côté quand la caméra tourne.
//
// La hauteur comparée est celle du coin 2 du cube, contre le coin 0 du voisin,
// pour les DEUX parois — y compris celle de droite, qui longe pourtant les coins
// 1 et 3 : c'est cette paire que le jeu compare. À égalité, la paroi regarde le
// cube.
bool WallFacesNeighbour(const GndCube& cube, const GndCube& neighbour) {
  return cube.corner_height[2] < neighbour.corner_height[0];
}

TerrainVertex Corner(const std::array<float, 3>& position, float u, float v,
                     const std::array<float, 2>& lightmap_uv, const std::array<float, 4>& color,
                     const std::array<float, 3>& normal) {
  return TerrainVertex{.position = position,
                       .texture_uv = {u, v},
                       .lightmap_uv = lightmap_uv,
                       .color = color,
                       .normal = normal};
}

}  // namespace

TerrainFaces::TerrainFaces(const formats::GndMap& gnd, const LightmapAtlas& atlas,
                           std::span<const std::uint8_t> invisible_tiles)
    : gnd_(gnd), atlas_(atlas), invisible_tiles_(invisible_tiles) {}

// UNE FACE SANS TEXTURE NE S'ÉMET PAS : elle ne pourrait se peindre qu'avec la
// texture de repli, un magenta que le client ne montre jamais. Sa couleur de
// coin, elle, reste lue par ses voisins.
const GndTile* TerrainFaces::FaceTileAt(std::int32_t tile_index) const {
  const auto& tiles = gnd_.tiles();
  if (tile_index < 0 || static_cast<std::size_t>(tile_index) >= tiles.size()) {
    return nullptr;
  }
  const GndTile& tile = tiles[static_cast<std::size_t>(tile_index)];
  return tile.texture_index >= 0 ? &tile : nullptr;
}

// La couleur d'un carreau se lit au COIN, pas sur la face.
//
// Le format la range par carreau, mais le jeu la traite comme une valeur posée
// à l'ANGLE de la cellule : chacun des quatre sommets d'une face prend celle
// d'un carreau voisin — le sien, celui du fond, celui de droite, celui de la
// diagonale — et la carte graphique interpole entre les quatre. Donner la même
// couleur aux quatre coins découpe le sol en rectangles nets, ce qui saute aux
// yeux dès qu'une carte peint ses cellules de teintes différentes : une
// clairière dans un bois s'y voit en damier.
std::array<float, 4> TerrainFaces::CornerColor(std::int32_t cube_x, std::int32_t cube_y) const {
  if (cube_x < 0 || cube_y < 0 || cube_x >= gnd_.width() || cube_y >= gnd_.height()) {
    return kNoTint;
  }
  const std::int32_t tile_index = gnd_.at(cube_x, cube_y).tile_up;
  const auto& tiles = gnd_.tiles();
  if (tile_index < 0 || static_cast<std::size_t>(tile_index) >= tiles.size()) {
    return kNoTint;
  }
  return TileColor(tiles[static_cast<std::size_t>(tile_index)]);
}

std::optional<TerrainFace> TerrainFaces::Top(std::int32_t cube_x, std::int32_t cube_y) const {
  const GndCube& cube = gnd_.at(cube_x, cube_y);
  const GndTile* tile = FaceTileAt(cube.tile_up);
  // Un carreau tout transparent n'est PAS un carreau : c'est ainsi que le
  // mappeur troue la carte, et l'émettre reviendrait à confier au filtrage de
  // texture une décision qui est déjà prise dans les données.
  const auto tile_index = static_cast<std::size_t>(cube.tile_up);
  if (tile == nullptr ||
      (tile_index < invisible_tiles_.size() && invisible_tiles_[tile_index] != 0)) {
    return std::nullopt;
  }
  const float depth = static_cast<float>(gnd_.height()) * kWorldUnitsPerCube;
  const float left = static_cast<float>(cube_x) * kWorldUnitsPerCube;
  const float right = left + kWorldUnitsPerCube;
  // Les coins 2 et 3 sont au fond de la cellule ; les coins 0 et 1 sont UN PAS
  // PLUS PRÈS de l'observateur — pas un pas plus loin.
  //
  // Intervertir les deux ne change ni le nombre de sommets ni l'emprise de la
  // carte : cela retourne chaque carreau sur lui-même. La texture sort en miroir
  // et ne se raccorde plus d'une cellule à l'autre, ce qui ne se voit qu'en
  // comparant à une capture du client.
  const float z_back = depth - static_cast<float>(cube_y) * kWorldUnitsPerCube;
  const float z_front = z_back + kWorldUnitsPerCube;
  const std::array<float, 2> low = atlas_.MinimumOf(tile->lightmap_index);
  const std::array<float, 2> high = atlas_.MaximumOf(tile->lightmap_index);
  const std::array<float, 4>& h = cube.corner_height;

  // Les hauteurs sont NÉGATIVES dans le monde : le format compte vers le bas,
  // la scène vers le haut. Le coin proche porte la couleur de CE carreau ; les
  // trois autres celle de leur voisin.
  TerrainFace face{.texture_index = tile->texture_index};
  face.corners = {Corner({left, -h[0], z_front}, tile->u[0], tile->v[0], {low[0], low[1]},
                         TileColor(*tile), kGroundNormal),
                  Corner({right, -h[1], z_front}, tile->u[1], tile->v[1], {high[0], low[1]},
                         CornerColor(cube_x + 1, cube_y), kGroundNormal),
                  Corner({left, -h[2], z_back}, tile->u[2], tile->v[2], {low[0], high[1]},
                         CornerColor(cube_x, cube_y + 1), kGroundNormal),
                  Corner({right, -h[3], z_back}, tile->u[3], tile->v[3], {high[0], high[1]},
                         CornerColor(cube_x + 1, cube_y + 1), kGroundNormal)};
  return face;
}

// Ferme le dénivelé avec le cube de droite. Elle regarde vers l'est quand le
// cube est le plus haut, vers l'ouest sinon — voir WallFacesNeighbour.
std::optional<TerrainFace> TerrainFaces::EastWall(std::int32_t cube_x, std::int32_t cube_y) const {
  const GndCube& cube = gnd_.at(cube_x, cube_y);
  const GndTile* tile = FaceTileAt(cube.tile_side);
  if (tile == nullptr || cube_x + 1 >= gnd_.width()) {
    return std::nullopt;
  }
  const GndCube& neighbour = gnd_.at(cube_x + 1, cube_y);
  const float depth = static_cast<float>(gnd_.height()) * kWorldUnitsPerCube;
  const float right = static_cast<float>(cube_x + 1) * kWorldUnitsPerCube;
  const float z_back = depth - static_cast<float>(cube_y) * kWorldUnitsPerCube;
  const float z_front = z_back + kWorldUnitsPerCube;
  // Une paroi longe la frontière : elle prend les couleurs des deux carreaux
  // qu'elle sépare de l'avant à l'arrière.
  const std::array<float, 4> back = CornerColor(cube_x + 1, cube_y + 1);
  const std::array<float, 4> front = CornerColor(cube_x + 1, cube_y);
  const std::array<float, 3> side = WallFacesNeighbour(cube, neighbour)
                                        ? std::array<float, 3>{1.0F, 0.0F, 0.0F}
                                        : std::array<float, 3>{-1.0F, 0.0F, 0.0F};
  const std::array<float, 2> low = atlas_.MinimumOf(tile->lightmap_index);
  const std::array<float, 2> high = atlas_.MaximumOf(tile->lightmap_index);
  TerrainFace face{.texture_index = tile->texture_index};
  face.corners = {Corner({right, -cube.corner_height[3], z_back}, tile->u[0], tile->v[0],
                         {low[0], low[1]}, back, side),
                  Corner({right, -cube.corner_height[1], z_front}, tile->u[1], tile->v[1],
                         {high[0], low[1]}, front, side),
                  Corner({right, -neighbour.corner_height[2], z_back}, tile->u[2], tile->v[2],
                         {low[0], high[1]}, back, side),
                  Corner({right, -neighbour.corner_height[0], z_front}, tile->u[3], tile->v[3],
                         {high[0], high[1]}, front, side)};
  return face;
}

// Ferme le dénivelé avec le cube de la rangée suivante. Elle occupe la
// frontière commune aux deux cellules, donc z_back — et joint les coins 2 et 3
// de celle-ci aux coins 0 et 1 de l'autre. La rangée suivante est plus au
// FOND : regarder vers elle, c'est regarder vers les z décroissants.
std::optional<TerrainFace> TerrainFaces::NorthWall(std::int32_t cube_x, std::int32_t cube_y) const {
  const GndCube& cube = gnd_.at(cube_x, cube_y);
  const GndTile* tile = FaceTileAt(cube.tile_front);
  if (tile == nullptr || cube_y + 1 >= gnd_.height()) {
    return std::nullopt;
  }
  const GndCube& neighbour = gnd_.at(cube_x, cube_y + 1);
  const float depth = static_cast<float>(gnd_.height()) * kWorldUnitsPerCube;
  const float left = static_cast<float>(cube_x) * kWorldUnitsPerCube;
  const float right = left + kWorldUnitsPerCube;
  const float z_back = depth - static_cast<float>(cube_y) * kWorldUnitsPerCube;
  // De même, de gauche à droite.
  const std::array<float, 4> west = CornerColor(cube_x, cube_y + 1);
  const std::array<float, 4> east = CornerColor(cube_x + 1, cube_y + 1);
  const std::array<float, 3> front = WallFacesNeighbour(cube, neighbour)
                                         ? std::array<float, 3>{0.0F, 0.0F, -1.0F}
                                         : std::array<float, 3>{0.0F, 0.0F, 1.0F};
  const std::array<float, 2> low = atlas_.MinimumOf(tile->lightmap_index);
  const std::array<float, 2> high = atlas_.MaximumOf(tile->lightmap_index);
  TerrainFace face{.texture_index = tile->texture_index};
  face.corners = {Corner({left, -cube.corner_height[2], z_back}, tile->u[0], tile->v[0],
                         {low[0], low[1]}, west, front),
                  Corner({right, -cube.corner_height[3], z_back}, tile->u[1], tile->v[1],
                         {high[0], low[1]}, east, front),
                  Corner({left, -neighbour.corner_height[0], z_back}, tile->u[2], tile->v[2],
                         {low[0], high[1]}, west, front),
                  Corner({right, -neighbour.corner_height[1], z_back}, tile->u[3], tile->v[3],
                         {high[0], high[1]}, east, front)};
  return face;
}

TerrainMesh AssembleTerrainMesh(const std::map<std::int16_t, TerrainBatch>& batches) {
  TerrainMesh mesh;
  std::size_t vertex_total = 0;
  std::size_t index_total = 0;
  for (const auto& [texture_index, batch] : batches) {
    vertex_total += batch.vertices.size();
    index_total += batch.indices.size();
  }
  mesh.vertices.reserve(vertex_total);
  mesh.indices.reserve(index_total);
  mesh.surfaces.reserve(batches.size());

  for (const auto& [texture_index, batch] : batches) {
    const auto base = static_cast<std::uint32_t>(mesh.vertices.size());
    mesh.surfaces.push_back(
        TerrainSurface{.texture_index = texture_index,
                       .first_vertex = base,
                       .vertex_count = static_cast<std::uint32_t>(batch.vertices.size()),
                       .first_index = static_cast<std::uint32_t>(mesh.indices.size()),
                       .index_count = static_cast<std::uint32_t>(batch.indices.size())});
    mesh.vertices.insert(mesh.vertices.end(), batch.vertices.begin(), batch.vertices.end());
    // Les indices d'un lot comptent depuis SON premier sommet : ils se
    // décalent d'autant dans le maillage commun.
    for (const std::uint32_t index : batch.indices) {
      mesh.indices.push_back(base + index);
    }
  }

  constexpr float kFloatMax = std::numeric_limits<float>::max();
  mesh.bounds_min = {kFloatMax, kFloatMax, kFloatMax};
  mesh.bounds_max = {-kFloatMax, -kFloatMax, -kFloatMax};
  for (const TerrainVertex& vertex : mesh.vertices) {
    for (std::size_t axis = 0; axis < 3; ++axis) {
      mesh.bounds_min[axis] = std::min(mesh.bounds_min[axis], vertex.position[axis]);
      mesh.bounds_max[axis] = std::max(mesh.bounds_max[axis], vertex.position[axis]);
    }
  }
  if (mesh.vertices.empty()) {
    mesh.bounds_min = {0.0F, 0.0F, 0.0F};
    mesh.bounds_max = {0.0F, 0.0F, 0.0F};
  }
  return mesh;
}

}  // namespace ro::render
