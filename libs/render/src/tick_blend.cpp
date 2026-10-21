#include "ro/render/tick_blend.h"

#include <algorithm>
#include <cmath>

namespace ro::render {
namespace {

constexpr float kDegreesPerTurn = 360.0F;

}  // namespace

float TickBlendWeight(std::uint32_t since_tick_ms, std::uint32_t tick_ms, bool blended) noexcept {
  if (!blended || tick_ms == 0) {
    return kWholeTickWeight;
  }
  return std::min(static_cast<float>(since_tick_ms) / static_cast<float>(tick_ms),
                  kWholeTickWeight);
}

float BlendScalar(float previous, float current, float weight) noexcept {
  if (weight >= kWholeTickWeight) {
    return current;
  }
  return previous + (current - previous) * weight;
}

float BlendDegrees(float previous, float current, float weight) noexcept {
  if (weight >= kWholeTickWeight) {
    return current;
  }
  // L'écart ramené dans [-180, 180] : le plus court des deux sens.
  const float shortest = std::remainder(current - previous, kDegreesPerTurn);
  return previous + shortest * weight;
}

}  // namespace ro::render
