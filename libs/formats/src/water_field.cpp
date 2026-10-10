#include "ro/formats/water_field.h"

#include <algorithm>
#include <string>
#include <utility>

namespace ro::formats {
namespace {

using core::ErrorCode;
using core::Fail;

// Un cube GND couvre 2 × 2 cellules GAT.
//
// Le rapport n'est pas une convention choisie ici : Cube::AdjustGat de
// GRFEditor écrit les quatre cellules à `2 * y * gat_width + 2 * x`, et les
// dimensions mesurées le confirment (GND 150×150 pour un GAT 300×300).
constexpr std::int32_t kGatCellsPerCubeSide = 2;

}  // namespace

WaterField WaterField::Uniform(float level) {
  WaterField field;
  field.levels_ = {level};
  return field;
}

core::Result<WaterField> WaterField::Zoned(std::vector<float> levels, std::int32_t split_width,
                                           std::int32_t split_height, std::int32_t gnd_width,
                                           std::int32_t gnd_height) {
  if (split_width <= 0 || split_height <= 0) {
    return Fail(ErrorCode::kCorruptData, "découpe d'eau " + std::to_string(split_width) + "x" +
                                             std::to_string(split_height));
  }
  const auto expected =
      static_cast<std::size_t>(split_width) * static_cast<std::size_t>(split_height);
  if (levels.size() != expected) {
    return Fail(ErrorCode::kCorruptData, "découpe d'eau annonçant " + std::to_string(expected) +
                                             " zones pour " + std::to_string(levels.size()) +
                                             " niveaux");
  }
  if (gnd_width <= 0 || gnd_height <= 0) {
    return Fail(ErrorCode::kCorruptData, "dimensions GND nulles pour la découpe d'eau");
  }

  WaterField field;
  field.levels_ = std::move(levels);
  field.split_width_ = split_width;
  field.split_height_ = split_height;
  // Division entière, comme le renderer de GRFEditor. Le reste éventuel est
  // absorbé par la dernière zone via le plafonnement dans LevelAt.
  field.cubes_per_zone_x_ = std::max(1, gnd_width / split_width);
  field.cubes_per_zone_y_ = std::max(1, gnd_height / split_height);
  return field;
}

std::optional<float> WaterField::LevelAt(std::int32_t x, std::int32_t y) const {
  if (levels_.empty()) {
    return std::nullopt;
  }
  if (levels_.size() == 1) {
    return levels_.front();
  }

  const std::int32_t cube_x = x / kGatCellsPerCubeSide;
  const std::int32_t cube_y = y / kGatCellsPerCubeSide;

  // Le renderer parcourt les zones en Y inversé ET indexe les cubes en Y
  // inversé (`cubes[x + (height - y - 1) * width]`). Les deux inversions se
  // compensent : la rangée de zone se déduit directement de la rangée de cube.
  const std::int32_t zone_x = std::clamp(cube_x / cubes_per_zone_x_, 0, split_width_ - 1);
  const std::int32_t zone_y = std::clamp(cube_y / cubes_per_zone_y_, 0, split_height_ - 1);

  return levels_[static_cast<std::size_t>(zone_y) * static_cast<std::size_t>(split_width_) +
                 static_cast<std::size_t>(zone_x)];
}

}  // namespace ro::formats
