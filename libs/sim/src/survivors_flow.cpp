#include "ro/sim/survivors_flow.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace ro::sim {

void HordeFlow::Refresh(const SurvivorsGround& ground, CellPosition hero_cell, std::uint32_t step,
                        std::int32_t radius_cells, std::uint32_t refresh_steps) {
  if (flooded_from_) {
    const bool moved = flooded_from_->x != hero_cell.x || flooded_from_->y != hero_cell.y;
    if (!moved || step - flooded_step_ < refresh_steps) return;
  }
  const std::int32_t radius = std::max(radius_cells, 0);
  const std::int32_t first_x = std::max<std::int32_t>(hero_cell.x - radius, 0);
  const std::int32_t first_y = std::max<std::int32_t>(hero_cell.y - radius, 0);
  const std::int32_t last_x = std::min<std::int32_t>(hero_cell.x + radius, ground.width() - 1);
  const std::int32_t last_y = std::min<std::int32_t>(hero_cell.y + radius, ground.height() - 1);
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
  const std::array<CellPosition, 1> source{
      CellPosition{static_cast<std::uint16_t>(hero_cell.x - first_x),
                   static_cast<std::uint16_t>(hero_cell.y - first_y), 0}};
  // Un héros hors de la fenêtre (arène vide, case hors carte) ou sur une case
  // bloquée donne un champ sans source : tout y est injoignable, la horde
  // marche droit sur lui, et le champ ne couvre rien.
  field_ = WalkField::Flood(window, source);
  origin_x_ = first_x;
  origin_y_ = first_y;
  flooded_from_ = hero_cell;
  sourced_ = field_.CostFrom(source[0].x, source[0].y).has_value();
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
