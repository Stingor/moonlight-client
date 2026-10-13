#include "ro/render/effect_step_blend.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "ro/render/effect_frame.h"

namespace ro::render {
namespace {

constexpr float kTintCeiling = 255.0F;
constexpr float kFullTurnDegrees = 360.0F;
constexpr float kHalfTurnDegrees = 180.0F;

float Mixed(float previous, float current, float alpha) {
  return previous + (current - previous) * alpha;
}

}  // namespace

float EffectStepAlpha(std::uint32_t owed_ms) noexcept {
  return std::clamp(static_cast<float>(owed_ms) / static_cast<float>(kEffectTickMs), 0.0F, 1.0F);
}

float EffectFrameFraction(float elapsed_ms) noexcept {
  if (!(elapsed_ms > 0.0F)) {
    return 0.0F;
  }
  const float frames = elapsed_ms / static_cast<float>(kEffectTickMs);
  return std::clamp(frames - std::floor(frames), 0.0F, 1.0F);
}

glm::vec3 BlendEffectPoint(glm::vec3 previous, glm::vec3 previous_shift, glm::vec3 current,
                           float alpha) noexcept {
  const glm::vec3 from = previous + previous_shift;
  return from + (current - from) * alpha;
}

std::array<std::uint8_t, 4> BlendEffectTint(const std::array<std::uint8_t, 4>& previous,
                                            const std::array<std::uint8_t, 4>& current,
                                            float alpha) noexcept {
  std::array<std::uint8_t, 4> mixed{};
  for (std::size_t channel = 0; channel < mixed.size(); ++channel) {
    const float value =
        Mixed(static_cast<float>(previous[channel]), static_cast<float>(current[channel]), alpha);
    mixed[channel] = static_cast<std::uint8_t>(std::clamp(std::round(value), 0.0F, kTintCeiling));
  }
  return mixed;
}

void BlendEffectWorldQuad(const EffectWorldQuad& previous, glm::vec3 previous_shift, float alpha,
                          EffectWorldQuad& current) noexcept {
  for (std::size_t corner = 0; corner < current.corners.size(); ++corner) {
    current.corners[corner] =
        BlendEffectPoint(previous.corners[corner], previous_shift, current.corners[corner], alpha);
  }
  current.screen_anchor =
      BlendEffectPoint(previous.screen_anchor, previous_shift, current.screen_anchor, alpha);
  current.tint = BlendEffectTint(previous.tint, current.tint, alpha);
}

bool BlendEffectWorldQuads(std::span<const EffectWorldQuad> previous, glm::vec3 previous_shift,
                           float alpha, std::span<EffectWorldQuad> current) noexcept {
  if (previous.size() != current.size()) {
    return false;
  }
  for (std::size_t index = 0; index < current.size(); ++index) {
    if (previous[index].texture != current[index].texture) {
      return false;
    }
  }
  for (std::size_t index = 0; index < current.size(); ++index) {
    BlendEffectWorldQuad(previous[index], previous_shift, alpha, current[index]);
  }
  return true;
}

std::size_t BlendTaggedEffectWorldQuads(std::span<const EffectWorldQuad> previous,
                                        std::span<const std::uint64_t> previous_tags,
                                        glm::vec3 previous_shift, float alpha,
                                        std::span<EffectWorldQuad> current,
                                        std::span<const std::uint64_t> current_tags) noexcept {
  if (previous.size() != previous_tags.size() || current.size() != current_tags.size()) {
    return 0;
  }
  std::size_t blended = 0;
  std::size_t before = 0;
  for (std::size_t index = 0; index < current.size(); ++index) {
    while (before < previous.size() && previous_tags[before] < current_tags[index]) {
      ++before;
    }
    if (before < previous.size() && previous_tags[before] == current_tags[index] &&
        previous[before].texture == current[index].texture) {
      BlendEffectWorldQuad(previous[before], previous_shift, alpha, current[index]);
      ++blended;
    }
  }
  return blended;
}

float MixEffectValue(float previous, float current, float alpha) noexcept {
  return Mixed(previous, current, alpha);
}

float MixEffectDegrees(float previous, float current, float alpha) noexcept {
  float turn = std::fmod(current - previous, kFullTurnDegrees);
  if (turn > kHalfTurnDegrees) {
    turn -= kFullTurnDegrees;
  } else if (turn < -kHalfTurnDegrees) {
    turn += kFullTurnDegrees;
  }
  // L'ARRIVÉE EST L'ANGLE COURANT TEL QUEL : à part pleine, rien ne change.
  return current - turn * (1.0F - alpha);
}

bool BlendEffectScreenQuads(std::span<const EffectScreenQuad> previous, glm::vec2 previous_shift,
                            float alpha, std::span<EffectScreenQuad> current) noexcept {
  if (previous.size() != current.size()) {
    return false;
  }
  for (std::size_t index = 0; index < current.size(); ++index) {
    if (previous[index].texture != current[index].texture ||
        previous[index].blend_mode != current[index].blend_mode) {
      return false;
    }
  }
  for (std::size_t index = 0; index < current.size(); ++index) {
    for (std::size_t corner = 0; corner < current[index].corners.size(); ++corner) {
      const glm::vec2 from = previous[index].corners[corner] + previous_shift;
      current[index].corners[corner] = from + (current[index].corners[corner] - from) * alpha;
    }
    current[index].tint = BlendEffectTint(previous[index].tint, current[index].tint, alpha);
  }
  return true;
}

}  // namespace ro::render
