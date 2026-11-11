#include "ro/sim/walk_map.h"

#include <utility>

namespace ro::sim {
namespace {

bool Blocks(std::uint8_t server_cell) {
  return server_cell == kServerCellWall || server_cell == kServerCellGap;
}

// Le numéro de région d'une case bloquée.
constexpr std::uint32_t kNoRegion = 0;

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

void WalkMap::SetServerCell(std::int32_t x, std::int32_t y, std::uint8_t server_cell) {
  if (x < 0 || x >= width_ || y < 0 || y >= height_) {
    return;
  }
  std::uint8_t& passable =
      passable_[static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) +
                static_cast<std::size_t>(x)];
  const std::uint8_t now_passable = Blocks(server_cell) ? 0U : 1U;
  if (passable != now_passable) {
    passable = now_passable;
    region_of_cell_.clear();
  }
}

bool WalkMap::Connected(std::int32_t from_x, std::int32_t from_y, std::int32_t to_x,
                        std::int32_t to_y) const {
  if (!Passable(from_x, from_y) || !Passable(to_x, to_y)) {
    return false;
  }
  if (region_of_cell_.empty()) {
    LabelRegions();
  }
  const auto index_of = [this](std::int32_t x, std::int32_t y) {
    return static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) +
           static_cast<std::size_t>(x);
  };
  return region_of_cell_[index_of(from_x, from_y)] == region_of_cell_[index_of(to_x, to_y)];
}

void WalkMap::LabelRegions() const {
  region_of_cell_.assign(passable_.size(), kNoRegion);
  const auto width = static_cast<std::size_t>(width_);
  std::vector<std::size_t> pending;
  std::uint32_t region = kNoRegion;
  for (std::size_t seed = 0; seed < passable_.size(); ++seed) {
    if (passable_[seed] == 0 || region_of_cell_[seed] != kNoRegion) {
      continue;
    }
    ++region;
    region_of_cell_[seed] = region;
    pending.push_back(seed);
    while (!pending.empty()) {
      const std::size_t cell = pending.back();
      pending.pop_back();
      const auto visit = [&](std::size_t next) {
        if (passable_[next] != 0 && region_of_cell_[next] == kNoRegion) {
          region_of_cell_[next] = region;
          pending.push_back(next);
        }
      };
      if (cell % width != 0) {
        visit(cell - 1);
      }
      if (cell % width + 1 < width) {
        visit(cell + 1);
      }
      if (cell >= width) {
        visit(cell - width);
      }
      if (cell + width < passable_.size()) {
        visit(cell + width);
      }
    }
  }
}

}  // namespace ro::sim
