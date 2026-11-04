#include "ro/sim/survivors_flow.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace ro::sim {

void HordeFlow::Refresh(const SurvivorsGround& ground, CellPosition hero_cell, std::uint32_t step,
                        std::int32_t radius_cells, std::uint32_t refresh_steps) {
  const std::array<CellPosition, 1> hero_cells{hero_cell};
  Refresh(ground, hero_cells, step, radius_cells, refresh_steps);
}

void HordeFlow::Refresh(const SurvivorsGround& ground, std::span<const CellPosition> hero_cells,
                        std::uint32_t step, std::int32_t radius_cells,
                        std::uint32_t refresh_steps) {
  if (hero_cells.empty()) return;
  if (!flooded_from_.empty()) {
    const bool moved = !std::ranges::equal(
        flooded_from_, hero_cells,
        [](const CellPosition& a, const CellPosition& b) { return a.x == b.x && a.y == b.y; });
    if (!moved || step - flooded_step_ < refresh_steps) return;
  }
  std::int32_t least_x = hero_cells.front().x;
  std::int32_t least_y = hero_cells.front().y;
  std::int32_t most_x = least_x;
  std::int32_t most_y = least_y;
  for (const CellPosition& cell : hero_cells) {
    least_x = std::min<std::int32_t>(least_x, cell.x);
    least_y = std::min<std::int32_t>(least_y, cell.y);
    most_x = std::max<std::int32_t>(most_x, cell.x);
    most_y = std::max<std::int32_t>(most_y, cell.y);
  }
  const std::int32_t middle_x = least_x + (most_x - least_x) / 2;
  const std::int32_t middle_y = least_y + (most_y - least_y) / 2;
  const std::int32_t radius =
      std::max(radius_cells, 0) + std::max(most_x - middle_x, most_y - middle_y);
  const std::int32_t first_x = std::max<std::int32_t>(middle_x - radius, 0);
  const std::int32_t first_y = std::max<std::int32_t>(middle_y - radius, 0);
  const std::int32_t last_x = std::min<std::int32_t>(middle_x + radius, ground.width() - 1);
  const std::int32_t last_y = std::min<std::int32_t>(middle_y + radius, ground.height() - 1);
  const std::int32_t width = std::max(last_x - first_x + 1, 0);
  const std::int32_t height = std::max(last_y - first_y + 1, 0);

  std::vector<std::uint8_t> passable(static_cast<std::size_t>(width) *
                                     static_cast<std::size_t>(height));
  for (std::int32_t y = 0; y < height; ++y) {
    for (std::int32_t x = 0; x < width; ++x) {
      passable[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
               static_cast<std::size_t>(x)] =
          ground.Passable(first_x + x, first_y + y) ? std::uint8_t{1} : std::uint8_t{0};
    }
  }
  const WalkMap window(width, height, std::move(passable));
  std::vector<CellPosition> sources;
  sources.reserve(hero_cells.size());
  for (const CellPosition& cell : hero_cells) {
    sources.push_back(CellPosition{static_cast<std::uint16_t>(cell.x - first_x),
                                   static_cast<std::uint16_t>(cell.y - first_y), 0});
  }
  // Un héros hors de la fenêtre (arène vide, case hors carte) ou sur une case
  // bloquée donne un champ sans source : tout y est injoignable, la horde
  // marche droit sur lui, et le champ ne couvre rien.
  field_ = WalkField::Flood(window, sources);
  origin_x_ = first_x;
  origin_y_ = first_y;
  flooded_from_.assign(hero_cells.begin(), hero_cells.end());
  sourced_ = std::ranges::any_of(sources, [this](const CellPosition& source) {
    return field_.CostFrom(source.x, source.y).has_value();
  });
  flooded_step_ = step;
  ++floods_;
}

std::optional<CellPosition> HordeFlow::NextCellFrom(std::int32_t x, std::int32_t y) const {
  const std::optional<CellPosition> next = field_.NextCellFrom(x - origin_x_, y - origin_y_);
  if (!next) return std::nullopt;
  return CellPosition{static_cast<std::uint16_t>(next->x + origin_x_),
                      static_cast<std::uint16_t>(next->y + origin_y_), next->direction};
}

std::optional<std::uint32_t> HordeFlow::CostFrom(std::int32_t x, std::int32_t y) const {
  return field_.CostFrom(x - origin_x_, y - origin_y_);
}

bool HordeFlow::Covers(std::int32_t x, std::int32_t y) const noexcept {
  return sourced_ && x >= origin_x_ && y >= origin_y_ && x - origin_x_ < field_.width() &&
         y - origin_y_ < field_.height();
}

}  // namespace ro::sim
