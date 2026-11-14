#include "ro/render/union_hover.h"

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

using ro::render::Motion;
using ro::render::StepUnionHover;
using ro::render::UnionHover;
using ro::render::UnionHoverClock;
using ro::render::UnionHoverPixels;
using ro::render::UnionHoverPosture;
using ro::render::UnionHoverPostureOf;

namespace {

// Quinze pas de deux degrés : un multiple de trente, un cran de palier.
constexpr int kStepsPerNotch = 15;
constexpr std::uint8_t kAfloatRest = 20;
constexpr std::uint8_t kSittingRest = 100;

UnionHover Steps(UnionHover hover, UnionHoverPosture posture, int count) {
  for (int step = 0; step < count; ++step) {
    hover = StepUnionHover(hover, posture);
  }
  return hover;
}

}  // namespace

TEST_CASE("Lévitation : debout, le palier monte d'un cran par trente degrés jusqu'à vingt",
          "[union_hover]") {
  UnionHover hover = Steps({}, UnionHoverPosture::kAfloat, kStepsPerNotch - 1);
  CHECK(hover.angle_degrees == 28);
  CHECK(hover.rest_pixels == 0);
  hover = StepUnionHover(hover, UnionHoverPosture::kAfloat);
  CHECK(hover.angle_degrees == 30);
  CHECK(hover.rest_pixels == 1);
  hover = Steps({}, UnionHoverPosture::kAfloat, kStepsPerNotch * kAfloatRest);
  CHECK(hover.rest_pixels == kAfloatRest);
  // Au palier, il y reste.
  hover = Steps(hover, UnionHoverPosture::kAfloat, kStepsPerNotch * kAfloatRest);
  CHECK(hover.rest_pixels == kAfloatRest);
}

TEST_CASE("Lévitation : l'angle fait le tour en cent quatre-vingts pas", "[union_hover]") {
  const UnionHover hover =
      Steps({.angle_degrees = 0, .rest_pixels = kAfloatRest}, UnionHoverPosture::kAfloat, 180);
  CHECK(hover.angle_degrees == 0);
}

TEST_CASE("Lévitation : assis, le palier monte jusqu'à cent ; redescendu, il revient à vingt",
          "[union_hover]") {
  UnionHover hover = Steps({.angle_degrees = 0, .rest_pixels = kAfloatRest},
                           UnionHoverPosture::kSitting, kStepsPerNotch * kSittingRest);
  CHECK(hover.rest_pixels == kSittingRest);
  hover = Steps(hover, UnionHoverPosture::kAfloat, kStepsPerNotch);
  CHECK(hover.rest_pixels == kSittingRest - 1);
}

TEST_CASE("Lévitation : tout autre geste pose le corps d'un cran à chaque pas", "[union_hover]") {
  UnionHover hover{.angle_degrees = 0, .rest_pixels = kAfloatRest};
  hover = Steps(hover, UnionHoverPosture::kLanding, 1);
  CHECK(hover.rest_pixels == kAfloatRest - 1);
  hover = Steps(hover, UnionHoverPosture::kLanding, kAfloatRest);
  CHECK(hover.rest_pixels == 0);
}

TEST_CASE("Lévitation : quatre fois le sinus plus le palier, tronqué, jamais négatif",
          "[union_hover]") {
  CHECK(UnionHoverPixels({.angle_degrees = 0, .rest_pixels = kAfloatRest}) == 20);
  CHECK(UnionHoverPixels({.angle_degrees = 90, .rest_pixels = kAfloatRest}) == 24);
  CHECK(UnionHoverPixels({.angle_degrees = 270, .rest_pixels = kAfloatRest}) == 16);
  // Tronqué vers zéro : 4 sin(10°) vaut 0,69.
  CHECK(UnionHoverPixels({.angle_degrees = 10, .rest_pixels = 0}) == 0);
  CHECK(UnionHoverPixels({.angle_degrees = 90, .rest_pixels = 0}) == 4);
  // Sous le sol, le client ne descend pas : zéro.
  CHECK(UnionHoverPixels({.angle_degrees = 270, .rest_pixels = 0}) == 0);
}

TEST_CASE("Lévitation : la posture suit le geste, et le voile rouge tient en l'air",
          "[union_hover]") {
  CHECK(UnionHoverPostureOf(Motion::kIdle, false) == UnionHoverPosture::kAfloat);
  CHECK(UnionHoverPostureOf(Motion::kWalk, false) == UnionHoverPosture::kAfloat);
  CHECK(UnionHoverPostureOf(Motion::kReady, false) == UnionHoverPosture::kAfloat);
  CHECK(UnionHoverPostureOf(Motion::kSit, false) == UnionHoverPosture::kSitting);
  CHECK(UnionHoverPostureOf(Motion::kAttack, false) == UnionHoverPosture::kLanding);
  CHECK(UnionHoverPostureOf(Motion::kCast, false) == UnionHoverPosture::kLanding);
  CHECK(UnionHoverPostureOf(Motion::kDie, false) == UnionHoverPosture::kLanding);
  CHECK(UnionHoverPostureOf(Motion::kSit, true) == UnionHoverPosture::kAfloat);
  CHECK(UnionHoverPostureOf(Motion::kAttack, true) == UnionHoverPosture::kAfloat);
}

TEST_CASE("Lévitation : l'horloge fait un pas par calque et par image du client", "[union_hover]") {
  constexpr std::uint32_t kParts = 3;
  UnionHoverClock clock;
  // La première image en est une.
  static_cast<void>(clock.PixelsAt(0, UnionHoverPosture::kAfloat, kParts));
  CHECK(clock.hover().angle_degrees == 2 * kParts);
  // Cinq millisecondes plus tard, aucune image du client n'est due.
  static_cast<void>(clock.PixelsAt(5, UnionHoverPosture::kAfloat, kParts));
  CHECK(clock.hover().angle_degrees == 2 * kParts);
  // À 510 ms, trente images de trois calques : quatre-vingt-dix pas, la moitié
  // d'un tour.
  static_cast<void>(clock.PixelsAt(510, UnionHoverPosture::kAfloat, kParts));
  CHECK(clock.hover().angle_degrees == 2 * kParts + 180);
}

TEST_CASE("Lévitation : un acteur sans partie compte pour une", "[union_hover]") {
  UnionHoverClock clock;
  static_cast<void>(clock.PixelsAt(0, UnionHoverPosture::kAfloat, 0));
  CHECK(clock.hover().angle_degrees == 2);
}

TEST_CASE("Lévitation : une longue absence ne rejoue pas ses pas un à un", "[union_hover]") {
  UnionHoverClock clock;
  static_cast<void>(clock.PixelsAt(0, UnionHoverPosture::kAfloat, 1));
  // Une heure plus tard : le palier a eu le temps d'atteindre vingt, pas plus.
  constexpr std::uint32_t kOneHourMs = 3'600'000;
  CHECK(clock.PixelsAt(kOneHourMs, UnionHoverPosture::kAfloat, 1) >= 16);
  CHECK(clock.hover().rest_pixels == kAfloatRest);
}
