#include "world/number_paths.h"

#include <cstddef>

namespace moonclient {
namespace {

[[nodiscard]] std::size_t RankOf(DamagePath path) noexcept {
  const auto rank = static_cast<std::size_t>(path);
  return rank < kDamagePathCount ? rank : static_cast<std::size_t>(DamagePath::kOnMonster);
}

}  // namespace

const NumberPath& PathOf(const HitPopupLook& look, DamagePath path) noexcept {
  return look.paths[RankOf(path)];
}

NumberPath& PathOf(HitPopupLook& look, DamagePath path) noexcept {
  return look.paths[RankOf(path)];
}

std::optional<ro::render::NumberLife> PathLifeOf(const HitPopupLook& look,
                                                 DamagePath path) noexcept {
  if (path == DamagePath::kWords) {
    return std::nullopt;
  }
  return look.lives[RankOf(path)];
}

ro::render::NumberLife& PathLifeOf(HitPopupLook& look, DamagePath path) noexcept {
  return look.lives[RankOf(path)];
}

ro::render::NumberLook OnPath(const ro::render::NumberLook& motion, const NumberPath& path,
                              const std::optional<ro::render::NumberLife>& life,
                              const std::array<float, 2>& scatter) noexcept {
  ro::render::NumberLook placed = motion;
  placed.start_x_pixels = path.start_x_pixels;
  placed.lift_pixels = path.lift_pixels;
  placed.arrival_x_pixels = path.arrival_x_pixels + scatter[0];
  placed.arrival_y_pixels = path.arrival_y_pixels + scatter[1];
  placed.bend_one_x_pixels = path.bend_one_x_pixels;
  placed.bend_one_y_pixels = path.bend_one_y_pixels;
  placed.bend_two_x_pixels = path.bend_two_x_pixels;
  placed.bend_two_y_pixels = path.bend_two_y_pixels;
  placed.life = life;
  return placed;
}

}  // namespace moonclient
