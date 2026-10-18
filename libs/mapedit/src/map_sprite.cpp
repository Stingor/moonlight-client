#include "ro/mapedit/map_sprite.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ro::mapedit {
namespace {

constexpr double kMillisecondsPerSecond = 1000.0;

// Un angle ramené dans [0, 360).
[[nodiscard]] float WrappedDegrees(double degrees) {
  double wrapped = std::fmod(degrees, static_cast<double>(kFullTurnDegrees));
  if (wrapped < 0.0) {
    wrapped += static_cast<double>(kFullTurnDegrees);
  }
  // Un angle négatif infime remonté au tour retombe sur le tour entier.
  const auto narrowed = static_cast<float>(wrapped);
  return narrowed >= kFullTurnDegrees ? 0.0F : narrowed;
}

}  // namespace

std::optional<std::uint32_t> SpriteClassNumber(std::string_view sprite) {
  if (sprite.empty() || sprite.size() > std::numeric_limits<std::uint32_t>::digits10) {
    return std::nullopt;
  }
  constexpr std::uint32_t kDecimalBase = 10;
  std::uint32_t number = 0;
  for (const char digit : sprite) {
    if (digit < '0' || digit > '9') {
      return std::nullopt;
    }
    number = number * kDecimalBase + static_cast<std::uint32_t>(digit - '0');
  }
  return number;
}

float SpriteFacingDegreesAt(const MapSprite& sprite, double seconds) {
  const double start = static_cast<double>(sprite.direction % kSpriteDirectionCount) *
                       static_cast<double>(kSpriteDirectionDegrees);
  return WrappedDegrees(start + static_cast<double>(sprite.turn_degrees_per_second) * seconds);
}

std::uint32_t SpriteDirectionOfFacing(float facing_degrees) {
  const float wrapped = WrappedDegrees(facing_degrees);
  const auto nearest = static_cast<std::uint32_t>(std::lround(wrapped / kSpriteDirectionDegrees));
  return nearest % kSpriteDirectionCount;
}

std::size_t SpriteFrameAt(const MapSprite& sprite, double seconds, std::size_t frame_count,
                          float frame_interval_ms) {
  if (frame_count == 0) {
    return 0;
  }
  if (sprite.held_frame.has_value()) {
    return std::min<std::size_t>(*sprite.held_frame, frame_count - 1);
  }
  if (!(frame_interval_ms > 0.0F) || !(seconds > 0.0)) {
    return 0;
  }
  const double elapsed_ms =
      seconds * kMillisecondsPerSecond * static_cast<double>(sprite.frame_speed);
  const double frames = std::floor(elapsed_ms / static_cast<double>(frame_interval_ms));
  return static_cast<std::size_t>(std::fmod(frames, static_cast<double>(frame_count)));
}

float SpriteRollDegreesAt(const MapSprite& sprite, double seconds) {
  return WrappedDegrees(static_cast<double>(sprite.roll_degrees) +
                        static_cast<double>(sprite.roll_degrees_per_second) * seconds);
}

}  // namespace ro::mapedit
