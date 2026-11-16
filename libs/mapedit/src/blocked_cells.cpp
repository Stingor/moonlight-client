#include "ro/mapedit/blocked_cells.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>
#include <tuple>

namespace ro::mapedit {
namespace {

// Les types de la GAT que la grille de marche du serveur tient pour bloquants.
constexpr std::uint8_t kGatWall = static_cast<std::uint8_t>(formats::GatCellType::kBlocked);
constexpr std::uint8_t kGatFence =
    static_cast<std::uint8_t>(formats::GatCellType::kBlockedSnipable);

using Point = std::array<float, 2>;

// Une enveloppe a au moins trois sommets : en deçà, elle n'a pas d'aire.
constexpr std::size_t kLeastHullPoints = 3;

// Le produit vectoriel de (b - a) et (c - a) : positif quand c tombe à gauche
// de a → b.
[[nodiscard]] float Cross(const Point& a, const Point& b, const Point& c) {
  return (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]);
}

[[nodiscard]] bool Before(const BlockedCell& left, const BlockedCell& right) {
  return std::tie(left.cell_y, left.cell_x) < std::tie(right.cell_y, right.cell_x);
}

// Le type que le serveur lit : tronqué à un octet, délesté du drapeau d'eau.
[[nodiscard]] std::uint8_t ServerType(std::int32_t raw_type) {
  return static_cast<std::uint8_t>(static_cast<std::uint32_t>(raw_type) & ~formats::kGatWaterFlag);
}

}  // namespace

const BlockedCell* MapEdit::BlockedCellAt(std::int32_t cell_x, std::int32_t cell_y) const {
  return FindBlockedCell(blocked_cells_, cell_x, cell_y);
}

void MapEdit::BlockCell(const BlockedCell& cell) {
  const auto place = std::ranges::lower_bound(blocked_cells_, cell, Before);
  if (place != blocked_cells_.end() && place->cell_x == cell.cell_x &&
      place->cell_y == cell.cell_y) {
    place->block = cell.block;
    return;
  }
  blocked_cells_.insert(place, cell);
}

void MapEdit::FreeCell(std::int32_t cell_x, std::int32_t cell_y) {
  const BlockedCell probe{.cell_x = cell_x, .cell_y = cell_y};
  const auto place = std::ranges::lower_bound(blocked_cells_, probe, Before);
  if (place != blocked_cells_.end() && place->cell_x == cell_x && place->cell_y == cell_y) {
    blocked_cells_.erase(place);
  }
}

MapEditReport BlockGatCells(std::span<const BlockedCell> cells, formats::GatMap& gat) {
  MapEditReport report;
  for (const BlockedCell& cell : cells) {
    if (!gat.InBounds(cell.cell_x, cell.cell_y)) {
      report.refused.push_back(std::format("case bloquée : ({}, {}) sort de la grille, {} × {}",
                                           cell.cell_x, cell.cell_y, gat.width(), gat.height()));
      continue;
    }
    // Une case qui porte déjà son type garde son type brut, drapeau d'eau
    // compris.
    const std::int32_t raw_type = gat.at(cell.cell_x, cell.cell_y).type;
    const std::int32_t blocked_type = BlockedGatType(raw_type, cell.block);
    if (ServerType(raw_type) != blocked_type) {
      gat.SetType(cell.cell_x, cell.cell_y, blocked_type);
    }
    ++report.applied;
  }
  return report;
}

std::int32_t BlockedGatType(std::int32_t raw_type, CellBlock block) {
  return ServerType(raw_type) == kGatWall || block == CellBlock::kWall ? kGatWall : kGatFence;
}

const BlockedCell* FindBlockedCell(std::span<const BlockedCell> cells, std::int32_t cell_x,
                                   std::int32_t cell_y) {
  const BlockedCell probe{.cell_x = cell_x, .cell_y = cell_y};
  const auto place = std::ranges::lower_bound(cells, probe, Before);
  if (place != cells.end() && place->cell_x == cell_x && place->cell_y == cell_y) {
    return &*place;
  }
  return nullptr;
}

std::span<const BlockedCell> BlockedCellsInRows(std::span<const BlockedCell> cells,
                                                std::int32_t first_y, std::int32_t last_y) {
  if (first_y > last_y) {
    return {};
  }
  const auto first = std::ranges::lower_bound(cells, first_y, {},
                                              [](const BlockedCell& cell) { return cell.cell_y; });
  const auto last = std::ranges::upper_bound(first, cells.end(), last_y, {},
                                             [](const BlockedCell& cell) { return cell.cell_y; });
  return {first, last};
}

std::vector<std::array<std::int32_t, 2>> CellsUnderFootprint(
    std::span<const std::array<float, 2>> corners) {
  // L'ENVELOPPE, par la chaîne monotone : les points triés, puis le bord bas
  // et le bord haut, chacun ne gardant que les virages à gauche. Elle tourne
  // dans le sens trigonométrique.
  std::vector<Point> points(corners.begin(), corners.end());
  std::ranges::sort(points);
  points.erase(std::unique(points.begin(), points.end()), points.end());
  if (points.size() < kLeastHullPoints) {
    return {};
  }
  std::vector<Point> hull(2 * points.size());
  std::size_t size = 0;
  for (const Point& point : points) {
    while (size >= 2 && Cross(hull[size - 2], hull[size - 1], point) <= 0.0F) {
      --size;
    }
    hull[size++] = point;
  }
  const std::size_t lower_size = size + 1;
  for (auto point = points.rbegin() + 1; point != points.rend(); ++point) {
    while (size >= lower_size && Cross(hull[size - 2], hull[size - 1], *point) <= 0.0F) {
      --size;
    }
    hull[size++] = *point;
  }
  hull.resize(size - 1);
  if (hull.size() < kLeastHullPoints) {
    return {};
  }
  const auto [lowest_x, highest_x] =
      std::ranges::minmax(hull, {}, [](const Point& point) { return point[0]; });
  const auto [lowest_y, highest_y] =
      std::ranges::minmax(hull, {}, [](const Point& point) { return point[1]; });
  std::vector<std::array<std::int32_t, 2>> cells;
  for (auto cell_y = static_cast<std::int32_t>(std::ceil(lowest_y[1]));
       static_cast<float>(cell_y) <= highest_y[1]; ++cell_y) {
    for (auto cell_x = static_cast<std::int32_t>(std::ceil(lowest_x[0]));
         static_cast<float>(cell_x) <= highest_x[0]; ++cell_x) {
      const Point center{static_cast<float>(cell_x), static_cast<float>(cell_y)};
      bool inside = true;
      for (std::size_t edge = 0; edge < hull.size() && inside; ++edge) {
        inside = Cross(hull[edge], hull[(edge + 1) % hull.size()], center) >= 0.0F;
      }
      if (inside) {
        cells.push_back({cell_x, cell_y});
      }
    }
  }
  return cells;
}

}  // namespace ro::mapedit
