#include "ro/mapedit/ground_edit.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <format>
#include <numbers>
#include <tuple>

namespace ro::mapedit {
namespace {

// Une cellule de la grille de marche vaut un demi-cube : deux coins de
// cellule par point de la grille des coins.
constexpr std::int32_t kCellsPerCube = 2;
// Un cube range ses coins par rangée, deux par rangée.
constexpr std::int32_t kCornersPerRow = 2;
// Les quatre voisins d'un point, que la brosse Adoucir moyenne.
constexpr std::array<std::array<std::int32_t, 2>, 4> kNeighbours = {{
    {-1, 0},
    {1, 0},
    {0, -1},
    {0, 1},
}};

[[nodiscard]] bool Before(const GroundEdit& left, const GroundEdit& right) {
  return std::tie(left.corner_y, left.corner_x) < std::tie(right.corner_y, right.corner_x);
}

[[nodiscard]] bool OnCornerGrid(const formats::GndMap& gnd, std::int32_t x, std::int32_t y) {
  return x >= 0 && x <= gnd.width() && y >= 0 && y <= gnd.height();
}

// Le rang, dans un cube, du coin qui tombe au point (x, y) : le cube (cube_x,
// cube_y) porte ses coins bas-gauche, bas-droit, haut-gauche, haut-droit aux
// points (cube_x, cube_y), (cube_x + 1, cube_y), (cube_x, cube_y + 1) et
// (cube_x + 1, cube_y + 1).
[[nodiscard]] std::size_t CornerRank(std::int32_t x, std::int32_t y, std::int32_t cube_x,
                                     std::int32_t cube_y) {
  return static_cast<std::size_t>((x - cube_x) + kCornersPerRow * (y - cube_y));
}

// Appelle `visit(cube_x, cube_y, corner)` pour chaque coin de cube qui tombe au
// point (x, y).
template <typename Visit>
void ForEachTouchingCorner(const formats::GndMap& gnd, std::int32_t x, std::int32_t y,
                           Visit&& visit) {
  for (std::int32_t cube_y = y - 1; cube_y <= y; ++cube_y) {
    for (std::int32_t cube_x = x - 1; cube_x <= x; ++cube_x) {
      if (cube_x >= 0 && cube_x < gnd.width() && cube_y >= 0 && cube_y < gnd.height()) {
        visit(cube_x, cube_y, CornerRank(x, y, cube_x, cube_y));
      }
    }
  }
}

}  // namespace

void MapEdit::LiftGround(const GroundEdit& edit) {
  const auto place = std::ranges::lower_bound(ground_, edit, Before);
  if (place != ground_.end() && place->corner_x == edit.corner_x &&
      place->corner_y == edit.corner_y) {
    place->lift += edit.lift;
    if (std::abs(place->lift) < kGroundLiftEpsilon) {
      ground_.erase(place);
    }
    return;
  }
  if (std::abs(edit.lift) >= kGroundLiftEpsilon) {
    ground_.insert(place, edit);
  }
}

MapEditReport ApplyGroundEdits(std::span<const GroundEdit> edits, formats::GndMap& gnd) {
  MapEditReport report;
  for (const GroundEdit& edit : edits) {
    if (!OnCornerGrid(gnd, edit.corner_x, edit.corner_y)) {
      report.refused.push_back(
          std::format("relief : le point ({}, {}) sort de la grille des coins, {} × {}",
                      edit.corner_x, edit.corner_y, gnd.width() + 1, gnd.height() + 1));
      continue;
    }
    ForEachTouchingCorner(gnd, edit.corner_x, edit.corner_y,
                          [&](std::int32_t cube_x, std::int32_t cube_y, std::size_t corner) {
                            std::array<float, 4> heights = gnd.at(cube_x, cube_y).corner_height;
                            heights[corner] -= edit.lift;
                            gnd.SetCornerHeights(cube_x, cube_y, heights);
                          });
    ++report.applied;
  }
  return report;
}

MapEditReport ApplyGroundEdits(const MapEdit& edit, formats::GndMap& gnd) {
  return ApplyGroundEdits(std::span<const GroundEdit>(edit.ground()), gnd);
}

void LiftWalkHeights(std::span<const GroundEdit> edits, std::int32_t gnd_width,
                     std::int32_t gnd_height, formats::GatMap& gat) {
  if (edits.empty() || gnd_width <= 0 || gnd_height <= 0) {
    return;
  }
  // LA LEVÉE DE CHAQUE POINT, en grille pleine : la lire quatre fois par coin de
  // cellule ne demande alors aucune recherche.
  const auto columns = static_cast<std::size_t>(gnd_width) + 1;
  std::vector<float> lift(columns * (static_cast<std::size_t>(gnd_height) + 1), 0.0F);
  std::int32_t lowest_x = gnd_width;
  std::int32_t highest_x = 0;
  std::int32_t lowest_y = gnd_height;
  std::int32_t highest_y = 0;
  for (const GroundEdit& edit : edits) {
    if (edit.corner_x < 0 || edit.corner_x > gnd_width || edit.corner_y < 0 ||
        edit.corner_y > gnd_height) {
      continue;
    }
    lift[static_cast<std::size_t>(edit.corner_y) * columns +
         static_cast<std::size_t>(edit.corner_x)] += edit.lift;
    lowest_x = std::min(lowest_x, edit.corner_x);
    highest_x = std::max(highest_x, edit.corner_x);
    lowest_y = std::min(lowest_y, edit.corner_y);
    highest_y = std::max(highest_y, edit.corner_y);
  }
  if (lowest_x > highest_x || lowest_y > highest_y) {
    return;
  }
  const auto lift_at = [&](std::int32_t x, std::int32_t y) {
    if (x < 0 || x > gnd_width || y < 0 || y > gnd_height) {
      return 0.0F;
    }
    return lift[static_cast<std::size_t>(y) * columns + static_cast<std::size_t>(x)];
  };
  // Le coin de cellule (x, y) tombe au point (x / 2, y / 2) de la grille des
  // coins : entier sur un rang pair, au milieu de deux points sur un impair.
  const auto lift_of_corner = [&](std::int32_t x, std::int32_t y) {
    const std::int32_t point_x = x / kCellsPerCube;
    const std::int32_t point_y = y / kCellsPerCube;
    const float across = (x % kCellsPerCube == 0) ? 0.0F : 1.0F / static_cast<float>(kCellsPerCube);
    const float along = (y % kCellsPerCube == 0) ? 0.0F : 1.0F / static_cast<float>(kCellsPerCube);
    const float near_edge =
        std::lerp(lift_at(point_x, point_y), lift_at(point_x + 1, point_y), across);
    const float far_edge =
        std::lerp(lift_at(point_x, point_y + 1), lift_at(point_x + 1, point_y + 1), across);
    return std::lerp(near_edge, far_edge, along);
  };
  // Seules les cellules dont un coin voit un point levé changent.
  const std::int32_t first_x = std::max(0, kCellsPerCube * lowest_x - kCellsPerCube);
  const std::int32_t last_x = std::min(gat.width() - 1, kCellsPerCube * highest_x + 1);
  const std::int32_t first_y = std::max(0, kCellsPerCube * lowest_y - kCellsPerCube);
  const std::int32_t last_y = std::min(gat.height() - 1, kCellsPerCube * highest_y + 1);
  for (std::int32_t cell_y = first_y; cell_y <= last_y; ++cell_y) {
    for (std::int32_t cell_x = first_x; cell_x <= last_x; ++cell_x) {
      std::array<float, 4> heights = gat.at(cell_x, cell_y).corner_height;
      bool lifted = false;
      for (std::size_t corner = 0; corner < heights.size(); ++corner) {
        const auto step_x = static_cast<std::int32_t>(corner) % kCornersPerRow;
        const auto step_y = static_cast<std::int32_t>(corner) / kCornersPerRow;
        const float raised = lift_of_corner(cell_x + step_x, cell_y + step_y);
        if (raised != 0.0F) {
          heights[corner] -= raised;
          lifted = true;
        }
      }
      if (lifted) {
        gat.SetCornerHeights(cell_x, cell_y, heights);
      }
    }
  }
}

std::optional<float> CornerPointHeight(const formats::GndMap& gnd, std::int32_t x, std::int32_t y) {
  if (!OnCornerGrid(gnd, x, y)) {
    return std::nullopt;
  }
  float sum = 0.0F;
  int count = 0;
  ForEachTouchingCorner(gnd, x, y,
                        [&](std::int32_t cube_x, std::int32_t cube_y, std::size_t corner) {
                          sum += gnd.at(cube_x, cube_y).corner_height[corner];
                          ++count;
                        });
  if (count == 0) {
    return std::nullopt;
  }
  // L'axe vertical du monde monte, celui du fichier descend.
  return -sum / static_cast<float>(count);
}

float BrushFalloff(float distance, float radius) {
  if (!(radius > 0.0F)) {
    return distance <= 0.0F ? 1.0F : 0.0F;
  }
  if (distance >= radius) {
    return 0.0F;
  }
  return (1.0F + std::cos(std::numbers::pi_v<float> * distance / radius)) / 2.0F;
}

std::vector<GroundEdit> BrushStroke(const formats::GndMap& gnd, const GroundBrush& brush,
                                    float center_x, float center_y, float seconds, bool lower,
                                    float flatten_height) {
  std::vector<GroundEdit> edits;
  const float radius = std::max(brush.radius_cubes, 0.0F);
  const auto first_x = std::max(0, static_cast<std::int32_t>(std::ceil(center_x - radius)));
  const auto last_x =
      std::min(gnd.width(), static_cast<std::int32_t>(std::floor(center_x + radius)));
  const auto first_y = std::max(0, static_cast<std::int32_t>(std::ceil(center_y - radius)));
  const auto last_y =
      std::min(gnd.height(), static_cast<std::int32_t>(std::floor(center_y + radius)));
  for (std::int32_t y = first_y; y <= last_y; ++y) {
    for (std::int32_t x = first_x; x <= last_x; ++x) {
      const float distance =
          std::hypot(static_cast<float>(x) - center_x, static_cast<float>(y) - center_y);
      const float step = brush.units_per_second * seconds * BrushFalloff(distance, radius);
      if (!(step > 0.0F)) {
        continue;
      }
      float lift = 0.0F;
      if (brush.mode == GroundBrushMode::kRaise) {
        lift = lower ? -step : step;
      } else {
        const std::optional<float> height = CornerPointHeight(gnd, x, y);
        if (!height.has_value()) {
          continue;
        }
        float target = flatten_height;
        if (brush.mode == GroundBrushMode::kSmooth) {
          float sum = 0.0F;
          int count = 0;
          for (const auto& [step_x, step_y] : kNeighbours) {
            if (const std::optional<float> next = CornerPointHeight(gnd, x + step_x, y + step_y)) {
              sum += *next;
              ++count;
            }
          }
          target = count > 0 ? sum / static_cast<float>(count) : *height;
        }
        lift = std::clamp(target - *height, -step, step);
      }
      if (std::abs(lift) >= kGroundLiftEpsilon) {
        edits.push_back(GroundEdit{.corner_x = x, .corner_y = y, .lift = lift});
      }
    }
  }
  return edits;
}

}  // namespace ro::mapedit
