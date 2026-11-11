#include "survivors/arena_blackout.h"

#include <cmath>
#include <numbers>

#include "ro/render/terrain_mesh.h"

#include "survivors/arena_music.h"
#include "survivors/arena_night.h"

namespace moonclient {

float ArenaLampsOutShare(std::uint64_t presentation_ms) noexcept {
  return ArenaNightShare(presentation_ms).value_or(0.0F) * kArenaBlackoutMostShare;
}

bool ArenaAlarm(std::uint64_t presentation_ms) noexcept {
  return presentation_ms >= kArenaBossFromMs;
}

bool WallLampLit(const WallLamp& lamp, std::uint64_t presentation_ms) noexcept {
  return lamp.blackout_rank >= ArenaLampsOutShare(presentation_ms);
}

std::optional<ro::render::PointLight> WallLampLight(const WallLamp& lamp, glm::vec3 floor_world,
                                                    float light_height_world,
                                                    std::uint64_t presentation_ms) {
  if (!WallLampLit(lamp, presentation_ms)) return std::nullopt;
  ro::render::PointLight light;
  light.world_position = floor_world + glm::vec3(0.0F, light_height_world, 0.0F);
  light.range_world = kWallLampRangeCells * ro::render::kWorldUnitsPerCell;
  light.casts_shadow = false;
  if (!ArenaAlarm(presentation_ms)) {
    light.color = kWallLampColor * kWallLampStrength;
    return light;
  }
  // LE BATTEMENT : un cosinus sur la période, décalé du rang de la lampe.
  const double turns = static_cast<double>(presentation_ms % kWallLampAlarmPeriodMs) /
                           static_cast<double>(kWallLampAlarmPeriodMs) +
                       static_cast<double>(lamp.blackout_rank);
  const auto pulse = static_cast<float>(0.5 + 0.5 * std::cos(2.0 * std::numbers::pi * turns));
  light.color = kWallLampAlarmColor * kWallLampStrength *
                (kWallLampAlarmLeastShare + (1.0F - kWallLampAlarmLeastShare) * pulse);
  return light;
}

std::optional<float> ArenaSkyHourOf(ArenaDarkness darkness,
                                    std::uint64_t presentation_ms) noexcept {
  if (darkness == ArenaDarkness::kBlackout) return std::nullopt;
  return ArenaSkyHour(presentation_ms);
}

}  // namespace moonclient
