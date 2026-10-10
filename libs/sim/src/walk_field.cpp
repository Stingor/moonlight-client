#include "ro/sim/walk_field.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <limits>
#include <queue>
#include <utility>
#include <vector>

#include "ro/sim/walk_path.h"

namespace ro::sim {

namespace {

constexpr std::uint32_t kUnreached = std::numeric_limits<std::uint32_t>::max();
constexpr std::uint8_t kNoStep = 0xFF;

// Les huit voisins : décalage, et si le pas est diagonal.
struct Neighbour {
  std::int8_t dx;
  std::int8_t dy;
};
constexpr std::array<Neighbour, 8> kNeighbours{{
    {0, 1},
    {-1, 1},
    {-1, 0},
    {-1, -1},
    {0, -1},
    {1, -1},
    {1, 0},
    {1, 1},
}};

// Le pas qui revient d'un voisin : la direction opposée.
constexpr std::uint8_t kOpposite = 4;
constexpr std::uint8_t kDirections = 8;

[[nodiscard]] bool Diagonal(const Neighbour& step) noexcept { return step.dx != 0 && step.dy != 0; }

// Une diagonale ne coupe pas de coin : ses deux voisins droits doivent se
// franchir, sans quoi le serveur la refuse.
[[nodiscard]] bool StepAllowed(const WalkMap& map, std::int32_t x, std::int32_t y,
                               const Neighbour& step) {
  if (!map.Passable(x + step.dx, y + step.dy)) return false;
  if (!Diagonal(step)) return true;
  return map.Passable(x + step.dx, y) && map.Passable(x, y + step.dy);
}

}  // namespace

WalkField WalkField::Flood(const WalkMap& map, std::span<const CellPosition> sources) {
  WalkField field;
  field.width_ = map.width();
  field.height_ = map.height();
  if (field.width_ <= 0 || field.height_ <= 0) return field;
  const std::size_t cells =
      static_cast<std::size_t>(field.width_) * static_cast<std::size_t>(field.height_);
  field.cost_.assign(cells, kUnreached);
  field.toward_source_.assign(cells, kNoStep);

  using Entry = std::pair<std::uint32_t, std::uint32_t>;  // coût, rang de case
  std::priority_queue<Entry, std::vector<Entry>, std::greater<>> open;
  for (const CellPosition& source : sources) {
    if (!map.Passable(source.x, source.y)) continue;
    const std::uint32_t slot =
        static_cast<std::uint32_t>(source.y) * static_cast<std::uint32_t>(field.width_) + source.x;
    field.cost_[slot] = 0;
    open.emplace(0, slot);
  }

  // Dijkstra sur la grille. Le champ se lit ensuite DEPUIS n'importe quelle
  // case : on range donc, pour chaque voisin atteint, le pas qui le RAMÈNE vers
  // la case d'où il a été atteint.
  while (!open.empty()) {
    const auto [cost, slot] = open.top();
    open.pop();
    if (cost != field.cost_[slot]) continue;
    const std::int32_t x =
        static_cast<std::int32_t>(slot % static_cast<std::uint32_t>(field.width_));
    const std::int32_t y =
        static_cast<std::int32_t>(slot / static_cast<std::uint32_t>(field.width_));
    for (std::uint8_t direction = 0; direction < kDirections; ++direction) {
      const Neighbour& step = kNeighbours[direction];
      if (!StepAllowed(map, x, y, step)) continue;
      const std::uint32_t next = cost + (Diagonal(step) ? kMoveDiagonalCost : kMoveCost);
      const std::uint32_t neighbour =
          static_cast<std::uint32_t>(y + step.dy) * static_cast<std::uint32_t>(field.width_) +
          static_cast<std::uint32_t>(x + step.dx);
      if (next >= field.cost_[neighbour]) continue;
      field.cost_[neighbour] = next;
      field.toward_source_[neighbour] =
          static_cast<std::uint8_t>((direction + kOpposite) % kDirections);
      open.emplace(next, neighbour);
    }
  }
  return field;
}

std::optional<std::uint32_t> WalkField::CostFrom(std::int32_t x, std::int32_t y) const {
  if (x < 0 || y < 0 || x >= width_ || y >= height_ || cost_.empty()) return std::nullopt;
  const std::uint32_t cost = cost_[static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) +
                                   static_cast<std::size_t>(x)];
  if (cost == kUnreached) return std::nullopt;
  return cost;
}

std::vector<CellPosition> WalkField::PathFrom(std::int32_t x, std::int32_t y) const {
  std::vector<CellPosition> path;
  if (!CostFrom(x, y)) return path;
  // Chaque pas descend strictement le coût : la marche s'arrête donc toujours,
  // et le plafond ne sert qu'à se garder d'un champ abîmé.
  const std::size_t ceiling = cost_.size();
  while (path.size() <= ceiling) {
    path.push_back({static_cast<std::uint16_t>(x), static_cast<std::uint16_t>(y), 0});
    const std::uint8_t direction =
        toward_source_[static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) +
                       static_cast<std::size_t>(x)];
    if (direction == kNoStep) return path;
    x += kNeighbours[direction].dx;
    y += kNeighbours[direction].dy;
  }
  return {};
}

std::optional<CellPosition> NearestPassable(const WalkMap& map, std::int32_t x, std::int32_t y,
                                            std::int32_t max_ring) {
  for (std::int32_t ring = 0; ring <= max_ring; ++ring) {
    // Sur un anneau, la case la plus proche à vol d'oiseau d'abord ; à égalité,
    // l'ordre de balayage tranche, toujours le même.
    std::optional<CellPosition> best;
    std::int64_t best_distance = std::numeric_limits<std::int64_t>::max();
    for (std::int32_t dy = -ring; dy <= ring; ++dy) {
      for (std::int32_t dx = -ring; dx <= ring; ++dx) {
        if (std::max(std::abs(dx), std::abs(dy)) != ring) continue;
        if (!map.Passable(x + dx, y + dy)) continue;
        const std::int64_t distance =
            static_cast<std::int64_t>(dx) * dx + static_cast<std::int64_t>(dy) * dy;
        if (distance >= best_distance) continue;
        best_distance = distance;
        best =
            CellPosition{static_cast<std::uint16_t>(x + dx), static_cast<std::uint16_t>(y + dy), 0};
      }
    }
    if (best) return best;
  }
  return std::nullopt;
}

std::vector<CellPosition> CornersOf(std::span<const CellPosition> path) {
  std::vector<CellPosition> corners;
  if (path.empty()) return corners;
  corners.push_back(path.front());
  for (std::size_t at = 1; at + 1 < path.size(); ++at) {
    const std::int32_t in_dx = path[at].x - path[at - 1].x;
    const std::int32_t in_dy = path[at].y - path[at - 1].y;
    const std::int32_t out_dx = path[at + 1].x - path[at].x;
    const std::int32_t out_dy = path[at + 1].y - path[at].y;
    if (in_dx != out_dx || in_dy != out_dy) corners.push_back(path[at]);
  }
  if (path.size() > 1) corners.push_back(path.back());
  return corners;
}

std::uint32_t OctileCost(std::int32_t from_x, std::int32_t from_y, std::int32_t to_x,
                         std::int32_t to_y) noexcept {
  const std::uint32_t dx = static_cast<std::uint32_t>(std::abs(to_x - from_x));
  const std::uint32_t dy = static_cast<std::uint32_t>(std::abs(to_y - from_y));
  const std::uint32_t diagonal = std::min(dx, dy);
  const std::uint32_t straight = std::max(dx, dy) - diagonal;
  return diagonal * kMoveDiagonalCost + straight * kMoveCost;
}

}  // namespace ro::sim
