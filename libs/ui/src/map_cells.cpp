#include "ro/ui/map_cells.h"

#include <cstddef>

namespace ro::ui {
namespace {

// Glisser d'un pixel avance d'un centième de case ; un cran de molette, d'une
// case — ou d'une unité de hauteur —, dix avec Majuscule.
constexpr WheelDrag kCellsDrag{.drag_step = 0.01F, .wheel_step = 1.0F, .format = "%.2f"};

}  // namespace

std::array<float, 3> CellsOfNative(const std::array<float, 3>& native, MapCubes cubes) {
  return {static_cast<float>(cubes.width) + native[0] / kRswUnitsPerCell,
          static_cast<float>(cubes.height) + native[2] / kRswUnitsPerCell, -native[1]};
}

std::array<float, 3> NativeOfCells(const std::array<float, 3>& cells, MapCubes cubes) {
  return {(cells[0] - static_cast<float>(cubes.width)) * kRswUnitsPerCell, -cells[2],
          (cells[1] - static_cast<float>(cubes.height)) * kRswUnitsPerCell};
}

WheelDragResult PositionInCells(const char* label, std::array<float, 3>& native, MapCubes cubes) {
  const std::array<float, 3> before = CellsOfNative(native, cubes);
  std::array<float, 3> cells = before;
  const WheelDragResult result = WheelDragFloat3(label, cells, kCellsDrag);
  if (!result.changed) {
    return result;
  }
  const std::array<float, 3> moved = NativeOfCells(cells, cubes);
  // Case x → x du RSW, case y → z, hauteur → y.
  constexpr std::array<std::size_t, 3> kNativeAxisOf{0, 2, 1};
  for (std::size_t axis = 0; axis < cells.size(); ++axis) {
    if (cells[axis] != before[axis]) {
      native[kNativeAxisOf[axis]] = moved[kNativeAxisOf[axis]];
    }
  }
  return result;
}

}  // namespace ro::ui
