#include "survivors/presentation_clock.h"

#include <algorithm>
#include <cstdint>

#include "ro/sim/survivors_geometry.h"

namespace moonclient {
namespace {

constexpr std::uint64_t kPerMille = 1000;

}  // namespace

std::uint64_t PresentationMsOf(std::uint32_t step, std::uint32_t blend_per_mille) noexcept {
  return std::uint64_t{step} * ro::sim::kSurvivorsStepMs +
         std::uint64_t{blend_per_mille} * ro::sim::kSurvivorsStepMs / kPerMille;
}

void PresentationClock::Follow(std::uint32_t step, std::uint32_t blend_per_mille) noexcept {
  followed_ms_ = std::max(followed_ms_, PresentationMsOf(step, blend_per_mille));
}

void PresentationClock::RunOn(std::uint32_t elapsed_ms) noexcept { epilogue_ms_ += elapsed_ms; }

void PresentationClock::Reset() noexcept { *this = PresentationClock(); }

}  // namespace moonclient
