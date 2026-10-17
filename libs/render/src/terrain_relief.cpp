#include "ro/render/terrain_relief.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <span>

#include "ro/render/terrain_mesh.h"

namespace ro::render {
namespace {

using formats::GndMap;

// Un coin se range dans le cube par sa colonne (bit 0) et sa rangée (bit 1).
constexpr std::int32_t kCornersPerRow = 2;

// Les polynômes d'Hermite cubiques sur [0, 1] : poids de la valeur au début et
// à la fin, puis de la pente au début et à la fin.
float StartValueWeight(float t) { return (2.0F * t - 3.0F) * t * t + 1.0F; }
float EndValueWeight(float t) { return (3.0F - 2.0F * t) * t * t; }
float StartSlopeWeight(float t) { return ((t - 2.0F) * t + 1.0F) * t; }
float EndSlopeWeight(float t) { return (t - 1.0F) * t * t; }
// Leurs dérivées.
float StartValueRate(float t) { return 6.0F * t * (t - 1.0F); }
float EndValueRate(float t) { return 6.0F * t * (1.0F - t); }
float StartSlopeRate(float t) { return (3.0F * t - 4.0F) * t + 1.0F; }
float EndSlopeRate(float t) { return (3.0F * t - 2.0F) * t; }

struct CornerWeights {
  float value = 0.0F;
  float slope = 0.0F;
};

// Le poids, le long d'un axe, de ce que porte un coin situé au début (faux) ou
// à la fin (vrai) de cet axe.
CornerWeights WeightsAt(bool at_end, float t) {
  return at_end ? CornerWeights{EndValueWeight(t), EndSlopeWeight(t)}
                : CornerWeights{StartValueWeight(t), StartSlopeWeight(t)};
}
CornerWeights RatesAt(bool at_end, float t) {
  return at_end ? CornerWeights{EndValueRate(t), EndSlopeRate(t)}
                : CornerWeights{StartValueRate(t), StartSlopeRate(t)};
}

bool IsInside(const GndMap& gnd, std::int32_t cube_x, std::int32_t cube_y) {
  return cube_x >= 0 && cube_y >= 0 && cube_x < gnd.width() && cube_y < gnd.height();
}

// Vrai si le cube a un dessus que l'on DESSINE : un carreau, une texture, et
// pas tout transparent. Un trou de la carte porte des hauteurs, souvent celles
// du bord qu'il prolonge vers le vide ; s'y fier ferait plonger le bord d'une
// île vers le bas, en lame suspendue au-dessus de rien.
bool HasDrawnTop(const GndMap& gnd, std::int32_t cube_x, std::int32_t cube_y,
                 std::span<const std::uint8_t> invisible_tiles) {
  const std::int32_t tile_index = gnd.at(cube_x, cube_y).tile_up;
  const auto& tiles = gnd.tiles();
  if (tile_index < 0 || static_cast<std::size_t>(tile_index) >= tiles.size() ||
      tiles[static_cast<std::size_t>(tile_index)].texture_index < 0) {
    return false;
  }
  const auto tile = static_cast<std::size_t>(tile_index);
  return tile >= invisible_tiles.size() || invisible_tiles[tile] == 0;
}

// La hauteur que le cube donne au point (i, j) de la grille des coins, qui doit
// être l'un de ses quatre.
float CornerOf(const GndMap& gnd, std::int32_t cube_x, std::int32_t cube_y, std::int32_t point_i,
               std::int32_t point_j) {
  const auto corner =
      static_cast<std::size_t>((point_i - cube_x) + kCornersPerRow * (point_j - cube_y));
  return gnd.at(cube_x, cube_y).corner_height[corner];
}

// La hauteur au bout du segment de grille qui part de (i, j) d'un pas
// (step_i, step_j), si le sol s'y PROLONGE sans paroi depuis `height`.
//
// Deux cubes bordent un segment. Seul compte celui qui a un dessus et touche le
// point de départ à la même hauteur : l'autre est de l'autre côté d'une paroi. S'ils y
// touchent tous deux mais divergent au bout, une falaise naît là, et le sol ne
// se prolonge pas.
//
// La réponse ne dépend que du segment et de la hauteur de départ, jamais du
// cube qui la demande : deux cubes qui partagent une arête y lisent donc la
// même pente, et leurs bords arrondis se rejoignent.
std::optional<float> ContinuedHeight(const GndMap& gnd,
                                     std::span<const std::uint8_t> invisible_tiles,
                                     std::int32_t point_i, std::int32_t point_j,
                                     std::int32_t step_i, std::int32_t step_j, float height) {
  const std::int32_t low_i = std::min(point_i, point_i + step_i);
  const std::int32_t low_j = std::min(point_j, point_j + step_j);
  // Un segment horizontal est bordé par le cube de sa rangée et celui d'avant ;
  // un segment vertical, par le cube de sa colonne et celui de gauche.
  const std::array<std::array<std::int32_t, 2>, 2> borders =
      step_i != 0
          ? std::array<std::array<std::int32_t, 2>, 2>{{{low_i, point_j}, {low_i, point_j - 1}}}
          : std::array<std::array<std::int32_t, 2>, 2>{{{point_i, low_j}, {point_i - 1, low_j}}};
  std::optional<float> found;
  for (const auto& [cube_x, cube_y] : borders) {
    if (!IsInside(gnd, cube_x, cube_y) || !HasDrawnTop(gnd, cube_x, cube_y, invisible_tiles) ||
        CornerOf(gnd, cube_x, cube_y, point_i, point_j) != height) {
      continue;
    }
    const float far = CornerOf(gnd, cube_x, cube_y, point_i + step_i, point_j + step_j);
    if (found.has_value() && *found != far) {
      return std::nullopt;
    }
    found = far;
  }
  return found;
}

// La pente en un point, connaissant la montée du segment d'avant et celle du
// segment d'après — en hauteur par cube.
//
// LE SOL NE DÉBORDE PAS DE SES COINS. La différence centrée seule fait bomber
// un plateau au-dessus de lui-même au bord d'une marche, et creuser son pied :
// sept pour cent de la marche, douze unités relevées sur gonryun, de quoi
// recouvrir le socle d'une statue. D'où la règle de Fritsch et Carlson : une
// pente nulle là où le sol change de sens ou cesse de monter, et partout
// ailleurs une pente bornée à trois fois la plus faible des deux montées.
float MonotoneSlope(float rise_before, float rise_after) {
  if (rise_before * rise_after <= 0.0F) {
    return 0.0F;
  }
  constexpr float kMonotoneBound = 3.0F;
  const float central = (rise_before + rise_after) / 2.0F;
  const float bound = kMonotoneBound * std::min(std::abs(rise_before), std::abs(rise_after));
  return std::clamp(central, -bound, bound);
}

// La pente au coin `corner` du cube, le long d'un axe : colonnes (`along_rows`
// faux) ou rangées.
//
// Entre le coin voisin dans le cube et celui que le sol atteint de l'autre côté,
// bornée par `MonotoneSlope`. Sans sol continu de ce côté-là, le point manquant se
// prend en miroir : la pente vaut alors celle de l'arête du cube, et le bord
// d'une falaise garde sa ligne au lieu de se courber vers le pied.
float CornerSlope(const GndMap& gnd, std::span<const std::uint8_t> invisible_tiles,
                  std::int32_t cube_x, std::int32_t cube_y, std::size_t corner, bool along_rows) {
  const std::array<float, 4>& heights = gnd.at(cube_x, cube_y).corner_height;
  const std::size_t stride = along_rows ? kCornersPerRow : 1;
  const bool at_end = ((along_rows ? corner / kCornersPerRow : corner % kCornersPerRow)) != 0;
  const float height = heights[corner];
  const float inside = heights[at_end ? corner - stride : corner + stride];

  const auto column = static_cast<std::int32_t>(corner % kCornersPerRow);
  const auto row = static_cast<std::int32_t>(corner / kCornersPerRow);
  // On sort du cube par le côté du coin.
  const std::int32_t outward = at_end ? 1 : -1;
  const float beyond = ContinuedHeight(gnd, invisible_tiles, cube_x + column, cube_y + row,
                                       along_rows ? 0 : outward, along_rows ? outward : 0, height)
                           .value_or(2.0F * height - inside);
  const float before = at_end ? inside : beyond;
  const float after = at_end ? beyond : inside;
  return MonotoneSlope(height - before, after - height);
}

}  // namespace

CubePoint CubePointAt(const formats::GndMap& gnd, float world_x, float world_z) {
  if (gnd.width() <= 0 || gnd.height() <= 0) {
    return {};
  }
  constexpr float kCube = kWorldUnitsPerCube;
  const float depth = static_cast<float>(gnd.height()) * kCube;

  // Inverse du placement du maillage. Les rangées descendent en Z : la rangée
  // y occupe l'intervalle [depth − y·c, depth − y·c + c], d'où ce changement de
  // variable, qui rend une coordonnée croissante avec y.
  const float column = world_x / kCube;
  const float row = (depth + kCube - world_z) / kCube;

  CubePoint point;
  point.cube_x = std::clamp(static_cast<std::int32_t>(std::floor(column)), 0, gnd.width() - 1);
  point.cube_y = std::clamp(static_cast<std::int32_t>(std::floor(row)), 0, gnd.height() - 1);
  point.across = std::clamp(column - static_cast<float>(point.cube_x), 0.0F, 1.0F);
  point.along = std::clamp(row - static_cast<float>(point.cube_y), 0.0F, 1.0F);
  return point;
}

CubeRelief CubeReliefOf(const formats::GndMap& gnd, std::int32_t cube_x, std::int32_t cube_y,
                        std::span<const std::uint8_t> invisible_tiles) {
  CubeRelief relief;
  relief.corner_height = gnd.at(cube_x, cube_y).corner_height;
  for (std::size_t corner = 0; corner < relief.corner_height.size(); ++corner) {
    relief.slope_across[corner] = CornerSlope(gnd, invisible_tiles, cube_x, cube_y, corner, false);
    relief.slope_along[corner] = CornerSlope(gnd, invisible_tiles, cube_x, cube_y, corner, true);
  }
  return relief;
}

float ReliefHeightIn(const CubeRelief& relief, float across, float along) {
  // Le carreau d'Hermite à torsion nulle : chaque coin pèse par sa hauteur et
  // par ses deux pentes.
  float height = 0.0F;
  for (std::size_t corner = 0; corner < relief.corner_height.size(); ++corner) {
    const CornerWeights column = WeightsAt(corner % kCornersPerRow != 0, across);
    const CornerWeights row = WeightsAt(corner / kCornersPerRow != 0, along);
    height += column.value * row.value * relief.corner_height[corner] +
              column.slope * row.value * relief.slope_across[corner] +
              column.value * row.slope * relief.slope_along[corner];
  }
  return height;
}

std::array<float, 3> ReliefNormalIn(const CubeRelief& relief, float across, float along) {
  float rate_across = 0.0F;
  float rate_along = 0.0F;
  for (std::size_t corner = 0; corner < relief.corner_height.size(); ++corner) {
    const bool column_end = corner % kCornersPerRow != 0;
    const bool row_end = corner / kCornersPerRow != 0;
    const CornerWeights column = WeightsAt(column_end, across);
    const CornerWeights row = WeightsAt(row_end, along);
    const CornerWeights column_rate = RatesAt(column_end, across);
    const CornerWeights row_rate = RatesAt(row_end, along);
    rate_across += column_rate.value * row.value * relief.corner_height[corner] +
                   column_rate.slope * row.value * relief.slope_across[corner] +
                   column_rate.value * row.slope * relief.slope_along[corner];
    rate_along += column.value * row_rate.value * relief.corner_height[corner] +
                  column.slope * row_rate.value * relief.slope_across[corner] +
                  column.value * row_rate.slope * relief.slope_along[corner];
  }
  // Le monde monte quand le fichier descend, et les rangées vont vers les Z
  // décroissants : d'où les deux signes.
  const float rise_x = -rate_across / kWorldUnitsPerCube;
  const float rise_z = rate_along / kWorldUnitsPerCube;
  const float length = std::sqrt(rise_x * rise_x + 1.0F + rise_z * rise_z);
  return {-rise_x / length, 1.0F / length, -rise_z / length};
}

float RoundedGroundHeightAt(const formats::GndMap& gnd, float world_x, float world_z,
                            std::span<const std::uint8_t> invisible_tiles) {
  if (gnd.width() <= 0 || gnd.height() <= 0) {
    return 0.0F;
  }
  const CubePoint point = CubePointAt(gnd, world_x, world_z);
  // L'axe vertical du monde monte, celui du fichier descend.
  return -ReliefHeightIn(CubeReliefOf(gnd, point.cube_x, point.cube_y, invisible_tiles),
                         point.across, point.along);
}

float RoundedStandingHeight(const formats::GndMap& gnd, float walkable_height, float world_x,
                            float world_z, std::span<const std::uint8_t> invisible_tiles) {
  const float flat = GroundHeightAt(gnd, world_x, world_z);
  if (std::abs(walkable_height - flat) > kWalkOnTerrainTolerance) {
    return walkable_height;
  }
  return walkable_height + (RoundedGroundHeightAt(gnd, world_x, world_z, invisible_tiles) - flat);
}

}  // namespace ro::render
