#include "ro/sim/survivors_grid.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace ro::sim {

namespace {

// Le seau d'une coordonnée en sous-unités, ramené dans [0, count − 1].
[[nodiscard]] std::int32_t ClampedBucket(std::int64_t subunits, std::int32_t count) noexcept {
  const std::int64_t cell = subunits >= 0 ? subunits / kSubunitsPerCell
                                          : (subunits - (kSubunitsPerCell - 1)) / kSubunitsPerCell;
  const std::int64_t bucket = cell >= 0 ? cell / kGridBucketCells : -1;
  return static_cast<std::int32_t>(std::clamp<std::int64_t>(bucket, 0, count - 1));
}

[[nodiscard]] std::int32_t BucketsFor(std::int32_t cells) noexcept {
  if (cells <= 0) return 1;
  return (cells + kGridBucketCells - 1) / kGridBucketCells;
}

}  // namespace

void SurvivorsGrid::Reset(std::int32_t width_cells, std::int32_t height_cells) {
  columns_ = BucketsFor(width_cells);
  rows_ = BucketsFor(height_cells);
  starts_.assign(static_cast<std::size_t>(columns_) * static_cast<std::size_t>(rows_) + 1, 0);
  indexes_.clear();
}

void SurvivorsGrid::Recenter(std::int32_t origin_x_cells, std::int32_t origin_y_cells) noexcept {
  origin_x_units_ = std::int64_t{origin_x_cells} * kSubunitsPerCell;
  origin_y_units_ = std::int64_t{origin_y_cells} * kSubunitsPerCell;
}

std::size_t SurvivorsGrid::BucketOf(FieldPoint point) const noexcept {
  const std::int32_t column = ClampedBucket(point.x - origin_x_units_, columns_);
  const std::int32_t row = ClampedBucket(point.y - origin_y_units_, rows_);
  return static_cast<std::size_t>(row) * static_cast<std::size_t>(columns_) +
         static_cast<std::size_t>(column);
}

void SurvivorsGrid::Rebuild(std::span<const FieldPoint> points) {
  if (starts_.empty()) Reset(0, 0);
  const std::size_t buckets = starts_.size() - 1;
  std::fill(starts_.begin(), starts_.end(), 0U);
  for (const FieldPoint& point : points) ++starts_[BucketOf(point)];
  // Préfixe INCLUSIF : starts_[b] marque la fin du seau b. Le remplissage, de
  // la fin vers le début, le ramène à son début, et range chaque seau par index
  // croissants.
  std::uint32_t running = 0;
  for (std::size_t bucket = 0; bucket < buckets; ++bucket) {
    running += starts_[bucket];
    starts_[bucket] = running;
  }
  starts_[buckets] = running;
  indexes_.resize(points.size());
  for (std::size_t index = points.size(); index > 0; --index) {
    const std::size_t bucket = BucketOf(points[index - 1]);
    indexes_[--starts_[bucket]] = static_cast<std::uint32_t>(index - 1);
  }
}

void SurvivorsGrid::Near(FieldPoint center, std::int32_t radius_units,
                         std::span<const FieldPoint> points,
                         std::vector<std::uint32_t>& out) const {
  out.clear();
  if (radius_units < 0 || starts_.empty()) return;
  const std::int64_t radius = radius_units;
  const std::int64_t radius_squared = radius * radius;
  const std::int64_t center_x = center.x - origin_x_units_;
  const std::int64_t center_y = center.y - origin_y_units_;
  const std::int32_t first_column = ClampedBucket(center_x - radius, columns_);
  const std::int32_t last_column = ClampedBucket(center_x + radius, columns_);
  const std::int32_t first_row = ClampedBucket(center_y - radius, rows_);
  const std::int32_t last_row = ClampedBucket(center_y + radius, rows_);
  for (std::int32_t row = first_row; row <= last_row; ++row) {
    for (std::int32_t column = first_column; column <= last_column; ++column) {
      const std::size_t bucket =
          static_cast<std::size_t>(row) * static_cast<std::size_t>(columns_) +
          static_cast<std::size_t>(column);
      for (std::uint32_t at = starts_[bucket]; at < starts_[bucket + 1]; ++at) {
        const std::uint32_t index = indexes_[at];
        if (index >= points.size()) continue;
        if (DistanceSquared(points[index], center) <= radius_squared) out.push_back(index);
      }
    }
  }
  // Tri par insertion, sans tampon : les index sont uniques, l'ordre est total,
  // et chaque seau arrive déjà croissant.
  for (std::size_t sorted = 1; sorted < out.size(); ++sorted) {
    const std::uint32_t moving = out[sorted];
    std::size_t at = sorted;
    for (; at > 0 && out[at - 1] > moving; --at) out[at] = out[at - 1];
    out[at] = moving;
  }
}

}  // namespace ro::sim
