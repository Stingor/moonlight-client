#pragma once

#include <array>
#include <cstdint>
#include <span>

#include "ro/formats/gnd.h"

namespace ro::render {

// LE SOL ARRONDI : une surface lisse qui passe EXACTEMENT par les hauteurs de
// coin du GND, au lieu des deux triangles plats de chaque cube. Voir
// docs/adr/0122.
//
// Chaque cube porte un carreau bicubique d'Hermite : sa hauteur et ses deux
// pentes à chacun de ses quatre coins. Une pente se lit comme chez Catmull-Rom,
// par la différence centrée entre le coin d'avant et celui d'après sur la ligne
// de la grille, bornée pour que le sol ne déborde pas de ses coins (Fritsch et
// Carlson) — mais seulement le long d'un sol CONTINU. Là où le voisin ne
// partage pas la hauteur du coin, une paroi sépare les deux cubes : la pente ne
// regarde que son propre côté, et la falaise reste vive.
//
// Tout se lit dans l'unité et le sens du FICHIER : une hauteur positive
// descend. Seul `RoundedGroundHeightAt` parle le repère du monde.

// Les sous-carreaux par côté d'un cube quand le sol s'arrondit. Un seul rend le
// sol d'origine, au triangle près.
inline constexpr std::int32_t kFlatGroundSubdivisions = 1;
inline constexpr std::int32_t kRoundedGroundSubdivisions = 4;

// Un point du monde, rapporté au cube qui le contient : `across` va de 0 au bord
// gauche à 1 au bord droit, `along` de 0 au bord PROCHE (coins 0 et 1) à 1 au
// bord du fond (coins 2 et 3). Hors de la carte, le cube du bord, fractions
// bornées à [0, 1].
struct CubePoint {
  std::int32_t cube_x = 0;
  std::int32_t cube_y = 0;
  float across = 0.0F;
  float along = 0.0F;
};
[[nodiscard]] CubePoint CubePointAt(const formats::GndMap& gnd, float world_x, float world_z);

// Le carreau d'un cube : ce qu'il faut pour l'évaluer partout en lui, sans
// relire la carte. Les coins dans l'ordre du GND.
struct CubeRelief {
  std::array<float, 4> corner_height{};
  // La dérivée de la hauteur à chaque coin, en hauteur par cube : selon les
  // colonnes, puis selon les rangées, vers le fond.
  std::array<float, 4> slope_across{};
  std::array<float, 4> slope_along{};
};
//
// `invisible_tiles` vient de `FindInvisibleTiles` : un cube dont le dessus ne se
// dessine pas ne prolonge pas le sol de ses voisins. Vide, seuls les cubes sans
// carreau ni texture en sont.
[[nodiscard]] CubeRelief CubeReliefOf(const formats::GndMap& gnd, std::int32_t cube_x,
                                      std::int32_t cube_y,
                                      std::span<const std::uint8_t> invisible_tiles = {});

// La hauteur du carreau en un point du cube, `across` et `along` dans [0, 1].
[[nodiscard]] float ReliefHeightIn(const CubeRelief& relief, float across, float along);

// La normale du carreau en ce point, dans le REPÈRE DU MONDE, normalisée et
// tournée vers le haut.
[[nodiscard]] std::array<float, 3> ReliefNormalIn(const CubeRelief& relief, float across,
                                                  float along);

// L'altitude du sol arrondi en un point du monde : le pendant de
// `GroundHeightAt`, dans le même repère.
[[nodiscard]] float RoundedGroundHeightAt(const formats::GndMap& gnd, float world_x, float world_z,
                                          std::span<const std::uint8_t> invisible_tiles = {});

// L'écart, en unités du monde, sous lequel la grille de marche est réputée
// suivre le terrain. Sur moc_fild01, 98,7 % des cellules s'en tiennent à un
// dixième ; les autres sont des ponts et des plateformes, à plusieurs unités.
inline constexpr float kWalkOnTerrainTolerance = 0.5F;

// OÙ SE POSE UN ACTEUR QUAND LE SOL S'ARRONDIT, à partir de l'altitude que lui
// donne la grille de marche, `walkable_height`, au point du monde (x, z).
//
// La grille suit le sol d'ORIGINE, bilinéaire dans chaque cube. Là où elle le
// suit, l'acteur prend l'écart que l'arrondi creuse ou soulève à cet endroit :
// ses pieds restent sur le sol dessiné. Là où elle passe au-dessus — un pont,
// une plateforme —, il garde la grille, que l'arrondi du terrain d'en dessous
// ne concerne pas.
[[nodiscard]] float RoundedStandingHeight(const formats::GndMap& gnd, float walkable_height,
                                          float world_x, float world_z,
                                          std::span<const std::uint8_t> invisible_tiles = {});

}  // namespace ro::render
