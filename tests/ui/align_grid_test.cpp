// La grille d'alignement : où tombent ses lignes, et ce que l'aimant en fait.

#include "ro/ui/align_grid.h"

#include <cmath>

#include <catch2/catch_test_macros.hpp>

namespace {

// Une largeur d'écran franche, et un pas qui ne la divise PAS : c'est le cas
// qui distingue une grille ancrée au centre d'une grille ancrée au bord.
constexpr float kScreenWidth = 1000.0F;
constexpr int kCell = 32;

[[nodiscard]] ro::ui::AlignGrid Magnetic() {
  return ro::ui::AlignGrid{.shown = false, .magnetic = true, .cell_pixels = kCell};
}

}  // namespace

TEST_CASE("Grille : l'aimant éteint ne touche à rien", "[ui][grille]") {
  // ⚠ IL REND SA VALEUR TELLE QUELLE, pour que l'appelant l'applique sans
  // condition. Un aimant qu'il faut penser à ne pas appeler est un aimant qu'on
  // appellera quand même, un jour, depuis une fenêtre de plus.
  const ro::ui::AlignGrid off{.magnetic = false, .cell_pixels = kCell};
  CHECK(off.Snap(123.4F, kScreenWidth) == 123.4F);
}

TEST_CASE("Grille : une position se pose sur la ligne la plus proche", "[ui][grille]") {
  const ro::ui::AlignGrid grid = Magnetic();
  const float step = grid.step();
  // 🔴 LA GRILLE EST ANCRÉE SUR LE CENTRE DE L'ÉCRAN, pas sur son bord gauche :
  // c'est ce qui garde le maillage symétrique et fait tomber la croix du milieu
  // sur une ligne. La première ligne n'est donc PAS en zéro dès que la largeur
  // n'est pas un multiple du pas.
  const float first = std::fmod(kScreenWidth * 0.5F, step);
  CHECK(first > 0.0F);

  // Un point posé sur une ligne n'y bouge pas.
  CHECK(grid.Snap(first + step * 3.0F, kScreenWidth) == first + step * 3.0F);
  // Juste après une ligne : il y revient.
  CHECK(grid.Snap(first + step * 3.0F + 1.0F, kScreenWidth) == first + step * 3.0F);
  // Juste avant la suivante : il monte.
  CHECK(grid.Snap(first + step * 4.0F - 1.0F, kScreenWidth) == first + step * 4.0F);
}

TEST_CASE("Grille : le pas reste une grille", "[ui][grille]") {
  // Un pas nul ou négatif ferait une division par zéro, et un pas d'un pixel
  // ne retiendrait plus rien : les deux se bornent au même endroit, celui que
  // l'aimant ET le dessin lisent.
  ro::ui::AlignGrid grid = Magnetic();
  grid.cell_pixels = 0;
  CHECK(grid.step() == static_cast<float>(ro::ui::kSmallestGridCellPixels));
  grid.cell_pixels = 4096;
  CHECK(grid.step() == static_cast<float>(ro::ui::kLargestGridCellPixels));
}
