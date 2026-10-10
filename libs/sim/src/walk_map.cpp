#include "ro/sim/walk_map.h"

#include <utility>

namespace ro::sim {
namespace {

bool Blocks(std::uint8_t server_cell) {
  return server_cell == kServerCellWall || server_cell == kServerCellGap;
}

}  // namespace

WalkMap::WalkMap(std::int32_t width, std::int32_t height, std::vector<std::uint8_t> passable)
    : width_(width), height_(height), passable_(std::move(passable)) {
  passable_.resize(static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_), false);
}

WalkMap WalkMap::FromGat(const formats::GatMap& gat) {
  std::vector<std::uint8_t> passable;
  passable.reserve(gat.cells().size());
  for (const formats::GatCell& cell : gat.cells()) {
    const std::uint32_t type = static_cast<std::uint32_t>(cell.type) & ~formats::kGatWaterFlag;
    passable.push_back(Blocks(static_cast<std::uint8_t>(type)) ? 0U : 1U);
  }
  return WalkMap(gat.width(), gat.height(), std::move(passable));
}

WalkMap WalkMap::FromServerCells(std::int32_t width, std::int32_t height,
                                 std::span<const std::uint8_t> cells) {
  std::vector<std::uint8_t> passable;
  passable.reserve(cells.size());
  for (const std::uint8_t cell : cells) {
    passable.push_back(Blocks(cell) ? 0U : 1U);
  }
  return WalkMap(width, height, std::move(passable));
}

WalkMap WalkMap::FromPicture(std::span<const std::string_view> rows) {
  const auto height = static_cast<std::int32_t>(rows.size());
  const auto width = rows.empty() ? 0 : static_cast<std::int32_t>(rows[0].size());
  std::vector<std::uint8_t> passable(
      static_cast<std::size_t>(width) * static_cast<std::size_t>(height), 0U);
  for (std::int32_t y = 0; y < height; ++y) {
    // La première ligne du dessin est le nord : le y le plus grand.
    const std::string_view row = rows[static_cast<std::size_t>(height - 1 - y)];
    for (std::int32_t x = 0; x < width && x < static_cast<std::int32_t>(row.size()); ++x) {
      passable[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
               static_cast<std::size_t>(x)] =
          static_cast<std::uint8_t>(row[static_cast<std::size_t>(x)] == '.' ? 1 : 0);
    }
  }
  return WalkMap(width, height, std::move(passable));
}

bool WalkMap::Passable(std::int32_t x, std::int32_t y) const noexcept {
  if (x < 0 || x >= width_ || y < 0 || y >= height_) {
    return false;
  }
  return passable_[static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) +
                   static_cast<std::size_t>(x)] != 0;
}

}  // namespace ro::sim
