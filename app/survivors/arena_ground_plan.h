#pragma once

#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

#include "ro/sim/survivors_terrain.h"

#include "survivors/arena_ground_look.h"

namespace moonclient {

// LE PLAN D'UNE FENÊTRE DU SOL GÉNÉRÉ : ce que l'arène dessine autour du héros,
// en données pures, sans GL ni fichier. `sim` décide les cases et les pièces ;
// le plan en tire tout ce qui ne change pas la partie (ADR 0134, T1) : la
// texture et la teinte de chaque cube, les ondulations du sol (Q3), une ombre
// cuite sous chaque pièce, la pose des modèles, un semis qui ne bloque pas, et
// la terre des chemins, bordée d'un semis le long de ses lisières.
//
// Tout ce qui se voit est une fonction de la CASE DU MONDE et de la graine de la
// partie : deux fenêtres qui se recouvrent dessinent la même chose dans leur
// partie commune, et une fenêtre recentrée ne fait rien sauter.
//
// Les coordonnées d'une fenêtre comptent ses cases depuis son coin sud-ouest,
// x vers l'est, y vers le nord ; un point (`east`, `north`) en cases désigne un
// coin de case, et le centre de la case (x, y) est (x + ½, y + ½).

// LA FENÊTRE : 160 × 160 cases, soit 80 × 80 cubes du sol, posée sur des
// morceaux entiers.
inline constexpr std::int32_t kGroundWindowCells = 160;
inline constexpr std::int32_t kGroundCellsPerCube = 2;
inline constexpr std::int32_t kGroundWindowCubes = kGroundWindowCells / kGroundCellsPerCube;
// UNE TEXTURE DE SOL COUVRE 4 × 4 CUBES, chaque cube un quart sur chaque axe,
// comme sur les champs de Prontera : le motif se raccorde d'un cube à l'autre.
inline constexpr std::int32_t kGroundTextureCubes = 4;
// Une lightmap fait 8 × 8 texels, dont 6 × 6 couvrent le cube, plus un bord.
inline constexpr std::int32_t kGroundLightmapSide = 8;
inline constexpr std::int32_t kGroundLightmapTexels = kGroundLightmapSide * kGroundLightmapSide;

// Le coin sud-ouest d'une fenêtre, en cases du monde : un multiple de la taille
// d'un morceau.
struct GroundWindowOrigin {
  std::int32_t x_cells = 0;
  std::int32_t y_cells = 0;
  [[nodiscard]] bool operator==(const GroundWindowOrigin&) const = default;
};

// La fenêtre centrée sur le morceau qui contient la case du monde (x, y) : deux
// morceaux de part et d'autre.
[[nodiscard]] GroundWindowOrigin GroundWindowAround(std::int32_t x_cells, std::int32_t y_cells);

// UN CUBE DU SOL.
struct PlannedCube {
  // Rang dans `BiomeLook::textures`.
  std::uint16_t texture = 0;
  // La place du cube dans son motif de 4 × 4 cubes, de 0 à 3 sur chaque axe,
  // lue sur la case du monde ; et les quarts de tour du motif, dans le sens
  // trigonométrique.
  std::uint8_t quarter_east = 0;
  std::uint8_t quarter_north = 0;
  std::uint8_t turn = 0;
  // Les hauteurs des coins dans l'ordre et le sens du fichier GND : sud-ouest,
  // sud-est, nord-ouest, nord-est ; négatives vers le haut.
  std::array<float, 4> corner_height{};
  // La teinte du cube, bleu, vert, rouge, alpha comme dans le fichier.
  std::array<std::uint8_t, 4> color{};
  // Rang dans `GroundPlan::lightmaps` ; 0 : le plein jour partagé.
  std::uint32_t lightmap = 0;
  // Vrai : le cube se lit en retrait de kGroundTileInset du bord de son quart
  // de texture, pour que le filtrage n'y mêle pas un cube voisin qui ne se
  // raccorde pas (un coin de chemin).
  bool inset = false;
  [[nodiscard]] bool operator==(const PlannedCube&) const = default;
};
// Le retrait d'un cube `inset`, en part de la texture entière.
inline constexpr float kGroundTileInset = 1.0F / 64.0F;

// Le plan d'ombre d'une lightmap, 8 × 8 texels du sud au nord et d'ouest en
// est : 255 en plein jour, `kGroundShadeLevel` sous une ombre pleine.
using PlannedShade = std::array<std::uint8_t, kGroundLightmapTexels>;

// UNE POSE DE MODÈLE.
struct PlannedPose {
  std::string_view model;
  // Le point où il se pose, en cases de la fenêtre.
  float east = 0.0F;
  float north = 0.0F;
  float yaw_degrees = 0.0F;
  float scale = 1.0F;
  // Rang de sa pièce dans `GroundPlan::pieces`, ou `kSeedPose` pour un semis.
  std::uint32_t piece = 0;
  [[nodiscard]] bool operator==(const PlannedPose&) const = default;
};
inline constexpr std::uint32_t kSeedPose = 0xFFFF'FFFFU;

// Une pièce de `sim` posée dans la fenêtre, et son dessin.
struct PlannedPiece {
  ro::sim::PlacedPiece placed;
  const ro::sim::TerrainPieceRule* rule = nullptr;
  const PieceLook* look = nullptr;
};

struct GroundPlan {
  GroundWindowOrigin origin;
  // kGroundWindowCubes² cubes, ligne par ligne du sud au nord.
  std::vector<PlannedCube> cubes;
  // kGroundWindowCells² cases : 1 praticable, comme `sim` le dit.
  std::vector<std::uint8_t> passable;
  // kGroundWindowCells² cases : 1 sur un chemin, comme `sim` le dit.
  std::vector<std::uint8_t> path;
  // Les hauteurs des coins de chaque case, dans l'ordre du fichier GAT.
  std::vector<std::array<float, 4>> cell_heights;
  // Les ombres cuites ; la première est le plein jour que partagent les cubes
  // qu'aucune ombre ne touche.
  std::vector<PlannedShade> lightmaps;
  std::vector<PlannedPiece> pieces;
  std::vector<PlannedPose> poses;
};

// L'ombre la plus sombre que pose une pièce, sur 255.
inline constexpr std::uint8_t kGroundShadeLevel = 128;

// D'où vient la lumière du soleil de référence, dans le repère du monde (x vers
// l'est, y vers le haut, z vers le sud) : voir `ro::render::SceneLight`.
struct GroundPlanLight {
  glm::vec3 toward_sun{0.0F, 1.0F, 0.0F};
};

// LE PLAN DE LA FENÊTRE `origin` du terrain `rule` sous la graine `run_seed`,
// dessiné selon `biome`. Une pièce que `biome` ne dessine pas laisse ses cases
// bloquées sous la terre nue.
[[nodiscard]] GroundPlan PlanGroundWindow(const ro::sim::TerrainRule& rule, std::uint64_t run_seed,
                                          const BiomeLook& biome, GroundWindowOrigin origin,
                                          const GroundPlanLight& light);

// L'ANGLE DU RSW (`RswModel::rotation[1]`, en degrés dans [0, 360)) d'un
// modèle qui suit le quart de tour de sa pièce : l'emprise tourne de `quarter`
// quarts dans le sens trigonométrique vu du ciel, l'angle du RSW tourne dans
// l'autre sens, d'où l'angle de la place `slot_yaw_degrees` moins les quarts.
[[nodiscard]] float PieceModelYawDegrees(std::uint8_t quarter, float slot_yaw_degrees);

// La hauteur du sol, dans l'unité et le sens du fichier, au coin de case (x, y)
// du monde : de douces ondulations, les mêmes pour toute fenêtre.
[[nodiscard]] float GroundReliefAt(std::uint64_t run_seed, double world_east, double world_north);

}  // namespace moonclient
