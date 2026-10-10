#include "ro/audio/ambient_sound.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace ro::audio {

namespace {

// Le plancher de la pente et du déplacement en abscisse, avant de diviser.
constexpr float kSlopeFloor = 0.001F;
constexpr float kHalf = 0.5F;
constexpr float kMillisecondsPerSecond = 1000.0F;

[[nodiscard]] float GroundDistance(std::array<float, 2> from, std::array<float, 2> to) {
  return std::hypot(to[0] - from[0], to[1] - from[1]);
}

}  // namespace

std::array<float, 2> AmbientSoundPoint(const AmbientSoundSource& source,
                                       std::array<float, 2> native_listener) {
  // Une source sans largeur ne sonne jamais, et son rapport diviserait par zéro.
  if (source.width == 0) {
    return source.native_ground;
  }
  const float offset_x = native_listener[0] - source.native_ground[0];
  const float offset_z = native_listener[1] - source.native_ground[1];
  const float sign_x = offset_x < 0.0F ? -1.0F : 1.0F;
  const float sign_z = offset_z < 0.0F ? -1.0F : 1.0F;
  // ⚠ LA PENTE SE DIVISE PAR UN ÉCART BORNÉ PAR EN BAS, pas par sa valeur
  // absolue : à l'ouest de la source, l'écart négatif devient un millième, et
  // la pente s'envole — le bruit part alors du haut ou du bas du rectangle.
  const float slope = offset_z / std::max(offset_x, kSlopeFloor);
  // ⚠ ET LE RAPPORT DES CÔTÉS SE CALCULE EN ENTIERS : quarante sur cinquante
  // vaut zéro.
  const auto ratio = static_cast<float>(source.height / source.width);
  float point_x = 0.0F;
  float point_z = 0.0F;
  if (ratio < std::abs(slope)) {
    point_z = static_cast<float>(source.height) * sign_z * kHalf;
    point_x = point_z / std::max(slope, kSlopeFloor);
  } else {
    point_x = static_cast<float>(source.width) * sign_x * kHalf;
    point_z = slope * point_x;
  }
  return {source.native_ground[0] + point_x, source.native_ground[1] + point_z};
}

AmbientSoundEmitter::AmbientSoundEmitter(const AmbientSoundSource& source, std::uint32_t now_ms)
    : source_(source),
      diagonal_(std::sqrt(
          static_cast<float>(source.width * source.width + source.height * source.height))),
      cycle_ms_(static_cast<std::uint32_t>(
          static_cast<std::int64_t>(source.cycle_seconds * kMillisecondsPerSecond))),
      last_played_ms_(now_ms - cycle_ms_) {}

std::optional<AmbientSoundCue> AmbientSoundEmitter::Tick(std::array<float, 2> native_listener,
                                                         std::uint32_t now_ms) {
  if (source_.width == 0) {
    return std::nullopt;
  }
  if (GroundDistance(source_.native_ground, native_listener) - (diagonal_ + source_.range) >
      kAmbientSoundWakeMargin) {
    return std::nullopt;
  }
  const std::array<float, 2> point = AmbientSoundPoint(source_, native_listener);
  if (GroundDistance(point, native_listener) - source_.range > kAmbientSoundPlayMargin) {
    return std::nullopt;
  }
  if (now_ms <= last_played_ms_ + cycle_ms_) {
    return std::nullopt;
  }
  last_played_ms_ = now_ms;
  // LES DEUX DISTANCES SE TRONQUENT EN ENTIERS, comme le client les passe à
  // son moteur sonore.
  const auto range_units = static_cast<std::int32_t>(source_.range);
  const auto near_units = static_cast<std::int32_t>(source_.range / kAmbientSoundNearDivisor);
  return AmbientSoundCue{point, static_cast<float>(range_units), static_cast<float>(near_units),
                         source_.volume};
}

std::vector<std::array<float, 2>> AmbientSoundFootprint(const AmbientSoundSource& source) {
  if (source.width == 0) {
    return {};
  }
  const float half_x = static_cast<float>(source.width) * kHalf;
  const float half_z = static_cast<float>(source.height) * kHalf;
  const float x = source.native_ground[0];
  const float z = source.native_ground[1];
  return {{x - half_x, z - half_z},
          {x + half_x, z - half_z},
          {x + half_x, z + half_z},
          {x - half_x, z + half_z}};
}

std::vector<std::array<float, 2>> AmbientSoundHearingCircle(std::array<float, 2> center,
                                                            float range, std::size_t segments) {
  std::vector<std::array<float, 2>> circle;
  if (segments == 0 || !(range > 0.0F)) {
    return circle;
  }
  circle.reserve(segments);
  const float step_radians = 2.0F * std::numbers::pi_v<float> / static_cast<float>(segments);
  for (std::size_t segment = 0; segment < segments; ++segment) {
    const float angle = step_radians * static_cast<float>(segment);
    circle.push_back({center[0] + range * std::cos(angle), center[1] + range * std::sin(angle)});
  }
  return circle;
}

}  // namespace ro::audio
