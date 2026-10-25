#include "survivors/arena_night.h"

#include <algorithm>

namespace moonclient {

std::optional<float> ArenaSkyHour(std::uint64_t presentation_ms) noexcept {
  if (presentation_ms < kArenaNightFallsMs) {
    return std::nullopt;
  }
  constexpr std::uint64_t kLastStep = (kArenaMidnightMs - kArenaNightFallsMs) / kArenaNightStepMs;
  const std::uint64_t step =
      std::min((presentation_ms - kArenaNightFallsMs) / kArenaNightStepMs, kLastStep);
  const float hour = kArenaNightFirstHour + (kArenaNightLastHour - kArenaNightFirstHour) *
                                                static_cast<float>(step) /
                                                static_cast<float>(kLastStep);
  // Minuit se dit zéro : l'heure du ciel vit dans [0, 24).
  return hour >= kArenaNightLastHour ? 0.0F : hour;
}

}  // namespace moonclient
