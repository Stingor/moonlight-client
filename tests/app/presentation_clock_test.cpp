// L'HORLOGE DE PRÉSENTATION DU MODE SURVIVORS : dérivée des pas, elle gèle
// avec la simulation, ne recule pas, et court pendant l'épilogue.

#include "survivors/presentation_clock.h"

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/survivors_geometry.h"

using moonclient::PresentationClock;
using moonclient::PresentationMsOf;
using ro::sim::kSurvivorsStepMs;

namespace {

constexpr std::uint32_t kStep = 150;
constexpr std::uint32_t kHalfStepPerMille = 500;
constexpr std::uint32_t kFullPerMille = 1000;
constexpr std::uint32_t kEpilogueMs = 750;

}  // namespace

TEST_CASE("PresentationMsOf : pas × 20 plus la part écoulée du pas suivant", "[app][survivors]") {
  CHECK(PresentationMsOf(0, 0) == 0);
  CHECK(PresentationMsOf(kStep, 0) == std::uint64_t{kStep} * kSurvivorsStepMs);
  CHECK(PresentationMsOf(kStep, kHalfStepPerMille) ==
        std::uint64_t{kStep} * kSurvivorsStepMs + kSurvivorsStepMs / 2);
  // 999 ‰ n'atteint pas le pas suivant.
  CHECK(PresentationMsOf(kStep, kFullPerMille - 1) < PresentationMsOf(kStep + 1, 0));
}

TEST_CASE("PresentationClock : gèle, ne recule pas, court pendant l'épilogue", "[app][survivors]") {
  PresentationClock clock;
  clock.Follow(kStep, kHalfStepPerMille);
  const std::uint64_t followed_ms = clock.presentation_ms();
  // Gelée : le même pas et le même reste redonnent la même heure.
  clock.Follow(kStep, kHalfStepPerMille);
  CHECK(clock.presentation_ms() == followed_ms);
  // Un choix ouvert remet le reste à zéro sans changer de pas : l'heure tient.
  clock.Follow(kStep, 0);
  CHECK(clock.presentation_ms() == followed_ms);
  // La reprise repart d'où elle était, sans saut.
  clock.Follow(kStep + 1, 0);
  CHECK(clock.presentation_ms() == std::uint64_t{kStep + 1} * kSurvivorsStepMs);
  clock.RunOn(kEpilogueMs);
  CHECK(clock.presentation_ms() == std::uint64_t{kStep + 1} * kSurvivorsStepMs + kEpilogueMs);
  clock.Reset();
  CHECK(clock.presentation_ms() == 0);
}
