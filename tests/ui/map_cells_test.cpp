// UNE POSITION DU RSW LUE DEPUIS LE COIN DE LA CARTE, en cases.

#include "ro/ui/map_cells.h"

#include <array>
#include <cmath>

#include <catch2/catch_test_macros.hpp>

using ro::ui::CellsOfNative;
using ro::ui::MapCubes;
using ro::ui::NativeOfCells;

TEST_CASE("Cases : la falaise de gonryun tombe sur la case de roc-cli", "[ui][mapedit]") {
  // gonryun mesure 150 cubes de côté ; roc-cli place le rang 27, en
  // 62.172 -8 -468.02 dans le RSW, sur la case 162 56.
  constexpr MapCubes kGonryun{.width = 150, .height = 150};
  const std::array<float, 3> cells = CellsOfNative({62.172F, -8.0F, -468.02F}, kGonryun);
  CHECK(std::floor(cells[0]) == 162.0F);
  CHECK(std::floor(cells[1]) == 56.0F);
  // La hauteur monte : un y négatif du RSW est au-dessus du sol.
  CHECK(cells[2] == 8.0F);
  // Et l'aller-retour rend la position.
  const std::array<float, 3> back = NativeOfCells(cells, kGonryun);
  CHECK(std::abs(back[0] - 62.172F) < 1e-3F);
  CHECK(std::abs(back[1] + 8.0F) < 1e-3F);
  CHECK(std::abs(back[2] + 468.02F) < 1e-3F);
}
