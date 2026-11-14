// LE MODE RÉGLAGE DES JAUGES : des valeurs factices qui ondulent.
//
// 🔴 UN RÉGLAGE SE JUGE AUX DEUX BOUTS : l'arrondi d'une part presque vide, le
// texte d'une jauge pleine. Des valeurs qui n'iraient que de 20 à 80 % ne les
// montreraient jamais, et le joueur réglerait à l'aveugle ce qu'il cherche à
// voir.

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/world.h"
#include "ro/ui/cast_bars.h"

#include "character/stat_bar_values.h"

using moonclient::TuningStatBarValues;
using moonclient::TuningStatBarWarnings;
using ro::ui::StatBar;

namespace {

constexpr std::int64_t kZenyReference = 1'000'000;
constexpr std::uint32_t kSampleStepMs = 20;
constexpr std::uint32_t kSampledMs = 60'000;

}  // namespace

TEST_CASE("Mode réglage : chaque jauge touche le vide et le plein", "[app][jauges]") {
  for (std::size_t rank = 0; rank < ro::ui::kStatBarCount; ++rank) {
    if (rank == static_cast<std::size_t>(StatBar::kCast)) {
      continue;
    }
    double lowest = 1.0;
    double highest = 0.0;
    for (std::uint32_t now_ms = 0; now_ms < kSampledMs; now_ms += kSampleStepMs) {
      const ro::ui::StatBarValue value = TuningStatBarValues(now_ms, kZenyReference)[rank];
      REQUIRE(value.known);
      REQUIRE(value.most > 0);
      REQUIRE(value.current >= 0);
      REQUIRE(value.current <= value.most);
      const double share = static_cast<double>(value.current) / static_cast<double>(value.most);
      lowest = std::min(lowest, share);
      highest = std::max(highest, share);
    }
    CHECK(lowest < 0.01);
    CHECK(highest > 0.99);
  }
}

TEST_CASE("Mode réglage : la bourse se remplit contre le repère du joueur", "[app][jauges]") {
  CHECK(TuningStatBarValues(0, kZenyReference)[static_cast<std::size_t>(StatBar::kZeny)].most ==
        kZenyReference);
}

TEST_CASE("Mode réglage : l'incantation rejoue un sort nommé", "[app][jauges]") {
  const ro::ui::StatBarValue cast =
      TuningStatBarValues(1'600, kZenyReference)[static_cast<std::size_t>(StatBar::kCast)];
  REQUIRE(cast.known);
  CHECK(cast.current == 1'600);
  CHECK(cast.most == 3'000);
  CHECK(cast.composed == "Storm Gust  " + ro::ui::CastSecondsText(1'400));
  // Elle recommence au terme.
  CHECK(TuningStatBarValues(3'100, kZenyReference)[static_cast<std::size_t>(StatBar::kCast)]
            .current == 100);
}

TEST_CASE("Mode réglage : il faut un seuil pour voir la surcharge", "[app][jauges]") {
  ro::sim::World world;
  CHECK(TuningStatBarWarnings(world).overweight_percent == 50);
}
