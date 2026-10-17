#pragma once

#include <array>
#include <cstdint>

#include "ro/ui/wheel_drag.h"

namespace ro::ui {

// LA TAILLE D'UNE CARTE EN CUBES DE SOL, deux cases par cube : ce qui permet de
// passer du repère du RSW à celui des cases.
struct MapCubes {
  std::int32_t width = 0;
  std::int32_t height = 0;
};

// Une case vaut cinq unités du RSW.
inline constexpr float kRswUnitsPerCell = 5.0F;
// Un cube du sol vaut deux cases de côté.
inline constexpr float kCellsPerCube = 2.0F;

// LE POINT (x, y) DE LA GRILLE DES COINS DU SOL, en cases depuis le coin de la
// carte : le coin bas-gauche de la case (2x, 2y).
[[nodiscard]] constexpr std::array<float, 2> CellsOfCornerPoint(std::int32_t corner_x,
                                                                std::int32_t corner_y) {
  return {static_cast<float>(corner_x) * kCellsPerCube,
          static_cast<float>(corner_y) * kCellsPerCube};
}

// UNE POSITION DU RSW LUE DEPUIS LE COIN DE LA CARTE, comme la minicarte : x et
// y en cases, avec leurs décimales — la case entière est leur plancher —, puis
// la hauteur en unités du RSW, positive vers le haut. Le RSW, lui, compte
// depuis le centre, et sa hauteur descend.
[[nodiscard]] std::array<float, 3> CellsOfNative(const std::array<float, 3>& native,
                                                 MapCubes cubes);
[[nodiscard]] std::array<float, 3> NativeOfCells(const std::array<float, 3>& cells, MapCubes cubes);

// LE CHAMP « Position » EN CASES pour une position du RSW : il montre les
// cases, et ne réécrit dans `native` que les composantes qu'on a changées —
// l'aller-retour des autres ne doit pas les déplacer d'un cheveu.
[[nodiscard]] WheelDragResult PositionInCells(const char* label, std::array<float, 3>& native,
                                              MapCubes cubes);

}  // namespace ro::ui
