#include "ro/sim/survivors_geometry.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace ro::sim {

namespace {

// sin(k × 2π / 64) × 1024, arrondi, pour k = 0..16.
constexpr std::array<std::int32_t, 17> kQuarterSine = {
    0, 100, 200, 297, 392, 483, 569, 650, 724, 792, 851, 903, 946, 980, 1004, 1019, 1024};
constexpr std::uint8_t kQuarterTurn = kHeadingSteps / 4;

// Le sinus du cap, en 1024es : un quart de la table, renversé ou nié selon
// le quart de tour.
[[nodiscard]] std::int32_t SineOf(std::uint8_t heading) noexcept {
  const std::size_t wrapped = static_cast<std::size_t>(heading) % kHeadingSteps;
  const std::size_t quarter = wrapped / kQuarterTurn;
  const std::size_t inside = wrapped % kQuarterTurn;
  const std::int32_t rising = kQuarterSine[inside];
  const std::int32_t falling = kQuarterSine[std::size_t{kQuarterTurn} - inside];
  switch (quarter) {
    case 0:
      return rising;
    case 1:
      return falling;
    case 2:
      return -rising;
    default:
      return -falling;
  }
}

[[nodiscard]] std::int32_t CosineOf(std::uint8_t heading) noexcept {
  const std::uint32_t turned =
      (static_cast<std::uint32_t>(heading) % kHeadingSteps + kQuarterTurn) % kHeadingSteps;
  return SineOf(static_cast<std::uint8_t>(turned));
}

}  // namespace

FieldPoint TurnVector(std::uint8_t heading, std::int32_t length) noexcept {
  // Le cap tourne dans le sens trigonométrique depuis le nord : x vaut −sin,
  // y vaut cos.
  const std::int64_t west = -static_cast<std::int64_t>(SineOf(heading)) * length;
  const std::int64_t north = static_cast<std::int64_t>(CosineOf(heading)) * length;
  return {static_cast<std::int32_t>(west / kSubunitsPerCell),
          static_cast<std::int32_t>(north / kSubunitsPerCell)};
}

std::uint8_t HeadingOf(FieldPoint delta, std::uint8_t fallback) noexcept {
  if (delta.x == 0 && delta.y == 0) return fallback;
  std::uint8_t best = 0;
  std::int64_t best_dot = 0;
  for (std::uint8_t heading = 0; heading < kHeadingSteps; ++heading) {
    const FieldPoint unit = TurnVector(heading, kSubunitsPerCell);
    const std::int64_t dot =
        static_cast<std::int64_t>(delta.x) * unit.x + static_cast<std::int64_t>(delta.y) * unit.y;
    if (heading == 0 || dot > best_dot) {
      best = heading;
      best_dot = dot;
    }
  }
  return best;
}

FieldPoint Toward(FieldPoint from, FieldPoint to, std::int32_t length) noexcept {
  const std::uint32_t distance = IntegerSqrt(static_cast<std::uint64_t>(DistanceSquared(from, to)));
  if (distance == 0) return {};
  const std::int64_t dx = static_cast<std::int64_t>(to.x) - from.x;
  const std::int64_t dy = static_cast<std::int64_t>(to.y) - from.y;
  return {static_cast<std::int32_t>(dx * length / distance),
          static_cast<std::int32_t>(dy * length / distance)};
}

}  // namespace ro::sim
