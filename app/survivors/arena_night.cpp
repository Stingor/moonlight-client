#include "survivors/arena_night.h"

#include <algorithm>

namespace moonclient {

std::optional<float> ArenaNightShare(std::uint64_t presentation_ms) noexcept {
  if (presentation_ms < kArenaNightFallsMs) {
    return std::nullopt;
  }
  constexpr std::uint64_t kLastStep = (kArenaMidnightMs - kArenaNightFallsMs) / kArenaNightStepMs;
  const std::uint64_t step =
      std::min((presentation_ms - kArenaNightFallsMs) / kArenaNightStepMs, kLastStep);
  return static_cast<float>(step) / static_cast<float>(kLastStep);
}

std::optional<float> ArenaSkyHour(std::uint64_t presentation_ms) noexcept {
  const std::optional<float> share = ArenaNightShare(presentation_ms);
  if (!share.has_value()) {
    return std::nullopt;
  }
  return kArenaNightFirstHour + (kArenaNightLastHour - kArenaNightFirstHour) * *share;
}

}  // namespace moonclient
