#include "ro/sim/survivors_openness.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <utility>
#include <vector>

namespace ro::sim {

namespace {

// Les quatre voisins d'une case.
constexpr std::array<std::pair<std::int32_t, std::int32_t>, 4> kNeighbours{
    {{1, 0}, {-1, 0}, {0, 1}, {0, -1}}};

// Une région qui n'a pas encore reçu de numéro, ou une case bloquée.
constexpr std::int32_t kNoRegion = -1;

[[nodiscard]] std::size_t IndexOf(std::int32_t x, std::int32_t y, std::int32_t width) noexcept {
  return static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
         static_cast<std::size_t>(x);
}

// Les bornes d'une fenêtre sur un axe, la fin exclue, ramenées dans la carte.
struct Span {
  std::int32_t from = 0;
  std::int32_t to = 0;
};

[[nodiscard]] Span WindowSpan(std::int32_t centre, std::int32_t side,
                              std::int32_t extent) noexcept {
  const std::int32_t from = centre - side / 2;
  return Span{.from = std::clamp(from, 0, extent), .to = std::clamp(from + side, 0, extent)};
}

// La table des sommes praticables : la case (x, y) de la table compte les cases
// praticables de [0, x) × [0, y).
class PassableSums {
 public:
  explicit PassableSums(const WalkMap& map)
      : stride_(map.width() + 1),
        sums_(
            static_cast<std::size_t>(map.width() + 1) * static_cast<std::size_t>(map.height() + 1),
            0) {
    for (std::int32_t y = 0; y < map.height(); ++y) {
      std::int32_t row = 0;
      for (std::int32_t x = 0; x < map.width(); ++x) {
        row += map.Passable(x, y) ? 1 : 0;
        sums_[IndexOf(x + 1, y + 1, stride_)] = sums_[IndexOf(x + 1, y, stride_)] + row;
      }
    }
  }

  [[nodiscard]] std::int32_t Count(Span xs, Span ys) const noexcept {
    return sums_[IndexOf(xs.to, ys.to, stride_)] - sums_[IndexOf(xs.from, ys.to, stride_)] -
           sums_[IndexOf(xs.to, ys.from, stride_)] + sums_[IndexOf(xs.from, ys.from, stride_)];
  }

 private:
  std::int32_t stride_ = 0;
  std::vector<std::int32_t> sums_;
};

// Les cases praticables atteignables depuis (x, y) sans sortir de [xs) × [ys).
[[nodiscard]] std::int32_t ReachableWithin(const WalkMap& map, std::int32_t start_x,
                                           std::int32_t start_y, Span xs, Span ys) {
  if (start_x < xs.from || start_x >= xs.to || start_y < ys.from || start_y >= ys.to ||
      !map.Passable(start_x, start_y)) {
    return 0;
  }
  const std::int32_t width = xs.to - xs.from;
  std::vector<std::uint8_t> seen(
      static_cast<std::size_t>(width) * static_cast<std::size_t>(ys.to - ys.from), 0U);
  std::vector<std::pair<std::int32_t, std::int32_t>> queue{{start_x, start_y}};
  seen[IndexOf(start_x - xs.from, start_y - ys.from, width)] = 1U;
  for (std::size_t next = 0; next < queue.size(); ++next) {
    const auto [x, y] = queue[next];
    for (const auto& [step_x, step_y] : kNeighbours) {
      const std::int32_t to_x = x + step_x;
      const std::int32_t to_y = y + step_y;
      if (to_x < xs.from || to_x >= xs.to || to_y < ys.from || to_y >= ys.to) continue;
      std::uint8_t& mark = seen[IndexOf(to_x - xs.from, to_y - ys.from, width)];
      if (mark != 0U || !map.Passable(to_x, to_y)) continue;
      mark = 1U;
      queue.emplace_back(to_x, to_y);
    }
  }
  return static_cast<std::int32_t>(queue.size());
}

// Le numéro de région de chaque case praticable, et le numéro de la plus grande.
struct Regions {
  std::vector<std::int32_t> labels;
  std::int32_t main_label = kNoRegion;
  std::int32_t main_cells = 0;
};

[[nodiscard]] Regions LabelRegions(const WalkMap& map) {
  Regions regions;
  regions.labels.assign(
      static_cast<std::size_t>(map.width()) * static_cast<std::size_t>(map.height()), kNoRegion);
  std::vector<std::pair<std::int32_t, std::int32_t>> queue;
  std::int32_t label = 0;
  for (std::int32_t y = 0; y < map.height(); ++y) {
    for (std::int32_t x = 0; x < map.width(); ++x) {
      if (!map.Passable(x, y) || regions.labels[IndexOf(x, y, map.width())] != kNoRegion) continue;
      queue.assign(1, {x, y});
      regions.labels[IndexOf(x, y, map.width())] = label;
      for (std::size_t next = 0; next < queue.size(); ++next) {
        const auto [from_x, from_y] = queue[next];
        for (const auto& [step_x, step_y] : kNeighbours) {
          const std::int32_t to_x = from_x + step_x;
          const std::int32_t to_y = from_y + step_y;
          if (!map.Passable(to_x, to_y)) continue;
          std::int32_t& to_label = regions.labels[IndexOf(to_x, to_y, map.width())];
          if (to_label != kNoRegion) continue;
          to_label = label;
          queue.emplace_back(to_x, to_y);
        }
      }
      const auto cells = static_cast<std::int32_t>(queue.size());
      if (cells > regions.main_cells) {
        regions.main_cells = cells;
        regions.main_label = label;
      }
      ++label;
    }
  }
  return regions;
}

}  // namespace

OpennessWindow WindowAround(const WalkMap& map, CellPosition centre, std::int32_t window_cells) {
  const std::int32_t side = std::max(window_cells, 1);
  const Span xs = WindowSpan(centre.x, side, map.width());
  const Span ys = WindowSpan(centre.y, side, map.height());
  std::int32_t passable = 0;
  for (std::int32_t y = ys.from; y < ys.to; ++y) {
    for (std::int32_t x = xs.from; x < xs.to; ++x) passable += map.Passable(x, y) ? 1 : 0;
  }
  return OpennessWindow{.centre = centre,
                        .passable_cells = passable,
                        .reachable_cells = ReachableWithin(map, centre.x, centre.y, xs, ys)};
}

ArenaOpenness MeasureArenaOpenness(const WalkMap& map, std::int32_t window_cells) {
  ArenaOpenness openness;
  openness.window_cells = std::max(window_cells, 1);
  const std::int32_t side = openness.window_cells;
  openness.map_cells = map.width() * map.height();
  if (openness.map_cells <= 0) return openness;

  const PassableSums sums(map);
  openness.passable_cells = sums.Count({0, map.width()}, {0, map.height()});
  for (std::int32_t y = 0; y + side <= map.height(); y += side) {
    for (std::int32_t x = 0; x + side <= map.width(); x += side) {
      openness.tiled_passable.push_back(sums.Count({x, x + side}, {y, y + side}));
    }
  }
  std::ranges::stable_sort(openness.tiled_passable);

  const Regions regions = LabelRegions(map);
  openness.main_region_cells = regions.main_cells;
  if (regions.main_label == kNoRegion) return openness;

  const std::int32_t middle_x = map.width() / 2;
  const std::int32_t middle_y = map.height() / 2;
  std::int32_t best_count = -1;
  std::int32_t best_distance = 0;
  std::int32_t best_x = 0;
  std::int32_t best_y = 0;
  // Le balayage va du sud au nord puis d'ouest en est : à compte et distance
  // égaux, la première case vue l'emporte, ce qui est la règle annoncée.
  for (std::int32_t y = 0; y < map.height(); ++y) {
    for (std::int32_t x = 0; x < map.width(); ++x) {
      if (regions.labels[IndexOf(x, y, map.width())] != regions.main_label) continue;
      const std::int32_t count =
          sums.Count(WindowSpan(x, side, map.width()), WindowSpan(y, side, map.height()));
      const std::int32_t distance = std::max(std::abs(x - middle_x), std::abs(y - middle_y));
      if (count > best_count || (count == best_count && distance < best_distance)) {
        best_count = count;
        best_distance = distance;
        best_x = x;
        best_y = y;
      }
    }
  }
  openness.best = WindowAround(map,
                               CellPosition{.x = static_cast<std::uint16_t>(best_x),
                                            .y = static_cast<std::uint16_t>(best_y)},
                               side);
  return openness;
}

}  // namespace ro::sim
