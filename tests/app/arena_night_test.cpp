// LA NUIT DE L'ARÈNE (U19) : le ciel du RSW jusqu'à 7:00, puis un palier par
// minute de midi au soir, et le soir tenu dès 15:00.

#include "survivors/arena_night.h"

#include <cstdint>
#include <optional>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using moonclient::ArenaNightShare;
using moonclient::ArenaSkyHour;
using moonclient::kArenaMidnightMs;
using moonclient::kArenaMsPerMinute;
using moonclient::kArenaNightFallsMs;

namespace {

constexpr std::uint64_t kOneMs = 1;
// Les huit paliers qui mènent de midi au soir, sept huitièmes d'heure chacun.
constexpr std::uint64_t kNightSteps = 8;
constexpr float kHoursPerStep = 0.875F;
constexpr float kNoon = 12.0F;
// LE SOIR OÙ LE CIEL S'ARRÊTE : dix-neuf heures, jamais minuit.
constexpr float kEvening = 19.0F;
constexpr std::uint64_t kLongAfterMs = 20 * kArenaMsPerMinute;

[[nodiscard]] std::uint64_t MinuteMs(std::uint64_t minute) { return minute * kArenaMsPerMinute; }

}  // namespace

TEST_CASE("Nuit de l'arène : le ciel du RSW jusqu'à 6:59", "[app][survivors]") {
  CHECK_FALSE(ArenaSkyHour(0).has_value());
  CHECK_FALSE(ArenaSkyHour(MinuteMs(6)).has_value());
  CHECK_FALSE(ArenaSkyHour(kArenaNightFallsMs - kOneMs).has_value());
  CHECK_FALSE(ArenaNightShare(kArenaNightFallsMs - kOneMs).has_value());
}

TEST_CASE("Nuit de l'arène : midi à 7:00, puis un palier par minute", "[app][survivors]") {
  REQUIRE(ArenaSkyHour(kArenaNightFallsMs).has_value());
  CHECK(*ArenaSkyHour(kArenaNightFallsMs) == Catch::Approx(kNoon));
  for (std::uint64_t step = 0; step < kNightSteps; ++step) {
    const float expected = kNoon + kHoursPerStep * static_cast<float>(step);
    const std::uint64_t from_ms = kArenaNightFallsMs + MinuteMs(step);
    // Le palier tient toute sa minute, et pas une milliseconde de plus.
    REQUIRE(ArenaSkyHour(from_ms).has_value());
    CHECK(*ArenaSkyHour(from_ms) == Catch::Approx(expected));
    CHECK(*ArenaSkyHour(from_ms + kArenaMsPerMinute - kOneMs) == Catch::Approx(expected));
    // La part de nuit suit les mêmes paliers.
    const float share = static_cast<float>(step) / static_cast<float>(kNightSteps);
    REQUIRE(ArenaNightShare(from_ms).has_value());
    CHECK(*ArenaNightShare(from_ms) == Catch::Approx(share));
  }
}

TEST_CASE("Nuit de l'arène : le soir à 15:00, et il tient", "[app][survivors]") {
  REQUIRE(ArenaSkyHour(kArenaMidnightMs).has_value());
  CHECK(*ArenaSkyHour(kArenaMidnightMs) == Catch::Approx(kEvening));
  const float last_step_hour = kNoon + kHoursPerStep * static_cast<float>(kNightSteps - 1);
  CHECK(*ArenaSkyHour(kArenaMidnightMs - kOneMs) == Catch::Approx(last_step_hour));
  CHECK(*ArenaSkyHour(kLongAfterMs) == Catch::Approx(kEvening));
  CHECK(*ArenaNightShare(kArenaMidnightMs) == Catch::Approx(1.0F));
  CHECK(*ArenaNightShare(kLongAfterMs) == Catch::Approx(1.0F));
}
