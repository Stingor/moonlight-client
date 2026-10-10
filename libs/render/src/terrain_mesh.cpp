#include "ro/render/terrain_mesh.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>

namespace ro::render {
namespace {

using formats::GndCube;
using formats::GndMap;
using formats::GndTile;

// Blanc là où il n'y a rien à lire : au-delà du bord de la carte, et sur une
// cellule trouée. C'est ce que fait la référence.
//
// ⚠ ELLE EST AU FICHIER, ET NON LOCALE À LA FONCTION QUI S'EN SERT : rendue
// par copie depuis une lambda à capture explicite, une constante locale est
// odr-utilisée, donc à capturer. MSVC l'accepte sans, g++ la refuse.
constexpr std::array<float, 4> kNoTint = {1.0F, 1.0F, 1.0F, 1.0F};

std::array<float, 4> TileColor(const GndTile& tile) {
  // Le fichier range la couleur BLEU, VERT, ROUGE, ALPHA — l'ordre d'un pixel
  // Windows, pas celui d'un nom de couleur. La lire comme ARGB échange le rouge
  // et le bleu, et prend l'alpha pour du rouge : sur une carte dont tous les
  // carreaux sont blancs, cela ne se voit pas ; sur les autres, si.
  return {static_cast<float>(tile.color[2]) / 255.0F, static_cast<float>(tile.color[1]) / 255.0F,
          static_cast<float>(tile.color[0]) / 255.0F, static_cast<float>(tile.color[3]) / 255.0F};
}

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
constexpr std::array<float, 3> kGroundNormal{0.0F, 1.0F, 0.0F};

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
                             std::span<const std::uint8_t> invisible_tiles) {
  // Regroupement par texture : une carte porte quelques dizaines de textures
  // pour des dizaines de milliers de carreaux, donc trier les sommets une fois
  // évite autant de changements d'état au dessin.
  std::map<std::int16_t, std::vector<TerrainVertex>> by_texture;

  const std::int32_t width = gnd.width();
  const std::int32_t height = gnd.height();
  const auto& tiles = gnd.tiles();
  const float depth = static_cast<float>(height) * kWorldUnitsPerCube;

  const auto invisible = [invisible_tiles](std::int32_t index) {
    const auto tile = static_cast<std::size_t>(index);
    return index >= 0 && tile < invisible_tiles.size() && invisible_tiles[tile] != 0;
  };

  const auto tile_at = [&tiles](std::int32_t index) -> const GndTile* {
    if (index < 0 || static_cast<std::size_t>(index) >= tiles.size()) {
      return nullptr;
    }
    return &tiles[static_cast<std::size_t>(index)];
  };
  // UNE FACE SANS TEXTURE NE S'ÉMET PAS : elle ne pourrait se peindre qu'avec
  // la texture de repli, un magenta que le client ne montre jamais. Sa couleur
  // de coin, elle, reste lue par ses voisins.
  const auto face_tile_at = [&tile_at](std::int32_t index) -> const GndTile* {
    const GndTile* tile = tile_at(index);
    return tile != nullptr && tile->texture_index >= 0 ? tile : nullptr;
  };

  // La couleur d'un carreau se lit au COIN, pas sur la face.
  //
  // Le format la range par carreau, mais le jeu la traite comme une valeur
  // posée à l'ANGLE de la cellule : chacun des quatre sommets d'une face prend
  // celle d'un carreau voisin — le sien, celui du fond, celui de droite, celui
  // de la diagonale — et la carte graphique interpole entre les quatre. Donner
  // la même couleur aux quatre coins découpe le sol en rectangles nets, ce qui
  // saute aux yeux dès qu'une carte peint ses cellules de teintes différentes :
  // une clairière dans un bois s'y voit en damier.
  const auto corner_color = [&gnd, &tile_at, width, height](std::int32_t x, std::int32_t y) {
    if (x < 0 || y < 0 || x >= width || y >= height) {
      return kNoTint;
    }
    const GndTile* tile = tile_at(gnd.at(x, y).tile_up);
    return tile != nullptr ? TileColor(*tile) : kNoTint;
  };

  for (std::int32_t y = 0; y < height; ++y) {
    for (std::int32_t x = 0; x < width; ++x) {
      const GndCube& cube = gnd.at(x, y);
      const float left = static_cast<float>(x) * kWorldUnitsPerCube;
      const float right = left + kWorldUnitsPerCube;

      // Les coins 2 et 3 sont au fond de la cellule ; les coins 0 et 1 sont UN
      // PAS PLUS PRÈS de l'observateur — pas un pas plus loin.
      //
      // Intervertir les deux ne change ni le nombre de sommets ni l'emprise de
      // la carte : cela retourne chaque carreau sur lui-même. La texture sort
      // en miroir et ne se raccorde plus d'une cellule à l'autre, ce qui ne se
      // voit qu'en comparant à une capture du client.
      const float z_back = depth - static_cast<float>(y) * kWorldUnitsPerCube;
      const float z_front = z_back + kWorldUnitsPerCube;

      // Face du dessus. Les hauteurs sont NÉGATIVES dans le monde : le format
      // compte vers le bas, la scène vers le haut.
      //
      // Un carreau tout transparent n'est PAS un carreau : c'est ainsi que le
      // mappeur troue la carte, et l'émettre reviendrait à confier au filtrage
      // de texture une décision qui est déjà prise dans les données.
      if (const GndTile* tile = face_tile_at(cube.tile_up);
          tile != nullptr && !invisible(cube.tile_up)) {
        // Le coin proche porte la couleur de CE carreau ; les trois autres
        // celle de leur voisin, dans l'ordre où les sommets sont émis.
        const std::array<float, 4> back_left = corner_color(x, y + 1);
        const std::array<float, 4> back_right = corner_color(x + 1, y + 1);
        const std::array<float, 4> front_left = TileColor(*tile);
        const std::array<float, 4> front_right = corner_color(x + 1, y);
        const std::array<float, 3>& up = kGroundNormal;
        const std::array<float, 2> low = atlas.MinimumOf(tile->lightmap_index);
        const std::array<float, 2> high = atlas.MaximumOf(tile->lightmap_index);
        std::vector<TerrainVertex>& out = by_texture[tile->texture_index];
        PushTopQuad(out,
                    {{left, -cube.corner_height[2], z_back},
                     {tile->u[2], tile->v[2]},
                     {low[0], high[1]},
                     back_left,
                     up},
                    {{right, -cube.corner_height[3], z_back},
                     {tile->u[3], tile->v[3]},
                     {high[0], high[1]},
                     back_right,
                     up},
                    {{left, -cube.corner_height[0], z_front},
                     {tile->u[0], tile->v[0]},
                     {low[0], low[1]},
                     front_left,
                     up},
                    {{right, -cube.corner_height[1], z_front},
                     {tile->u[1], tile->v[1]},
                     {high[0], low[1]},
                     front_right,
                     up});
      }

      // Face latérale : ferme le dénivelé avec le cube de droite. Elle regarde
      // vers l'est quand le cube est le plus haut, vers l'ouest sinon — voir
      // WallFacesNeighbour.
      if (const GndTile* tile = face_tile_at(cube.tile_side); tile != nullptr && x + 1 < width) {
        const GndCube& neighbour = gnd.at(x + 1, y);
        // Une paroi longe la frontière : elle prend les couleurs des deux
        // carreaux qu'elle sépare de l'avant à l'arrière.
        const std::array<float, 4> back = corner_color(x + 1, y + 1);
        const std::array<float, 4> front = corner_color(x + 1, y);
        const std::array<float, 3> side = WallFacesNeighbour(cube, neighbour)
                                              ? std::array<float, 3>{1.0F, 0.0F, 0.0F}
                                              : std::array<float, 3>{-1.0F, 0.0F, 0.0F};
        const std::array<float, 2> low = atlas.MinimumOf(tile->lightmap_index);
        const std::array<float, 2> high = atlas.MaximumOf(tile->lightmap_index);
        std::vector<TerrainVertex>& out = by_texture[tile->texture_index];
        PushQuad(out,
                 {{right, -cube.corner_height[3], z_back},
                  {tile->u[0], tile->v[0]},
                  {low[0], low[1]},
                  back,
                  side},
                 {{right, -cube.corner_height[1], z_front},
                  {tile->u[1], tile->v[1]},
                  {high[0], low[1]},
                  front,
                  side},
                 {{right, -neighbour.corner_height[2], z_back},
                  {tile->u[2], tile->v[2]},
                  {low[0], high[1]},
                  back,
                  side},
                 {{right, -neighbour.corner_height[0], z_front},
                  {tile->u[3], tile->v[3]},
                  {high[0], high[1]},
                  front,
                  side});
      }

      // Face frontale : ferme le dénivelé avec le cube de la rangée suivante.
      // Elle occupe la frontière commune aux deux cellules, donc z_back — et
      // joint les coins 2 et 3 de celle-ci aux coins 0 et 1 de l'autre. La
      // rangée suivante est plus au FOND : regarder vers elle, c'est regarder
      // vers les z décroissants.
      if (const GndTile* tile = face_tile_at(cube.tile_front); tile != nullptr && y + 1 < height) {
        const GndCube& neighbour = gnd.at(x, y + 1);
        // De même, de gauche à droite.
        const std::array<float, 4> west = corner_color(x, y + 1);
        const std::array<float, 4> east = corner_color(x + 1, y + 1);
        const std::array<float, 3> front = WallFacesNeighbour(cube, neighbour)
                                               ? std::array<float, 3>{0.0F, 0.0F, -1.0F}
                                               : std::array<float, 3>{0.0F, 0.0F, 1.0F};
        const std::array<float, 2> low = atlas.MinimumOf(tile->lightmap_index);
        const std::array<float, 2> high = atlas.MaximumOf(tile->lightmap_index);
        std::vector<TerrainVertex>& out = by_texture[tile->texture_index];
        PushQuad(out,
                 {{left, -cube.corner_height[2], z_back},
                  {tile->u[0], tile->v[0]},
                  {low[0], low[1]},
                  west,
                  front},
                 {{right, -cube.corner_height[3], z_back},
                  {tile->u[1], tile->v[1]},
                  {high[0], low[1]},
                  east,
                  front},
                 {{left, -neighbour.corner_height[0], z_back},
                  {tile->u[2], tile->v[2]},
                  {low[0], high[1]},
                  west,
                  front},
                 {{right, -neighbour.corner_height[1], z_back},
                  {tile->u[3], tile->v[3]},
                  {high[0], high[1]},
                  east,
                  front});
      }
    }
  }

  TerrainMesh mesh;
  std::size_t total = 0;
  for (const auto& [texture_index, vertices] : by_texture) {
    total += vertices.size();
  }
  mesh.vertices.reserve(total);
  mesh.surfaces.reserve(by_texture.size());

  for (const auto& [texture_index, vertices] : by_texture) {
    mesh.surfaces.push_back(TerrainSurface{texture_index,
                                           static_cast<std::uint32_t>(mesh.vertices.size()),
                                           static_cast<std::uint32_t>(vertices.size())});
    mesh.vertices.insert(mesh.vertices.end(), vertices.begin(), vertices.end());
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

float GroundHeightAt(const formats::GndMap& gnd, float world_x, float world_z) {
  if (gnd.width() <= 0 || gnd.height() <= 0) {
    return 0.0F;
  }
  constexpr float kCube = kWorldUnitsPerCube;
  const float depth = static_cast<float>(gnd.height()) * kCube;

  // Inverse du placement du maillage. Les rangées descendent en Z : la rangée
  // y occupe l'intervalle [depth − y·c, depth − y·c + c], d'où ce changement de
  // variable, qui rend une coordonnée croissante avec y.
  const float column = world_x / kCube;
  const float row = (depth + kCube - world_z) / kCube;

  const auto cube_x = std::clamp(static_cast<std::int32_t>(std::floor(column)), 0, gnd.width() - 1);
  const auto cube_y = std::clamp(static_cast<std::int32_t>(std::floor(row)), 0, gnd.height() - 1);
  const float fraction_x = std::clamp(column - static_cast<float>(cube_x), 0.0F, 1.0F);
  // 0 au bord PROCHE du cube, 1 au bord lointain — l'ordre des coins 0/1 puis
  // 2/3 du fichier.
  const float fraction_z = std::clamp(row - static_cast<float>(cube_y), 0.0F, 1.0F);

  const formats::GndCube& cube = gnd.at(cube_x, cube_y);
  const float near_edge = std::lerp(cube.corner_height[0], cube.corner_height[1], fraction_x);
  const float far_edge = std::lerp(cube.corner_height[2], cube.corner_height[3], fraction_x);
  // L'axe vertical du monde monte, celui du fichier descend.
  return -std::lerp(near_edge, far_edge, fraction_z);
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
