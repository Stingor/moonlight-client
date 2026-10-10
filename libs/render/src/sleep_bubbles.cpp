#include "ro/render/sleep_bubbles.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "ro/render/sprite_frame.h"

namespace ro::render {
namespace {

// Le plancher de la cadence : une cadence déclarée sous un tic vaut un tic.
constexpr float kLeastSpeedUnits = 1.0F;

}  // namespace

ComposedActor ComposeSleepBubbles(const formats::SprSprite& sprite,
                                  const formats::ActAnimation& animation, float asleep_ms) {
  return ComposeHeldChildSprite(sprite, animation, kSleepBubblesAction, asleep_ms);
}

ComposedActor ComposeHeldChildSprite(const formats::SprSprite& sprite,
                                     const formats::ActAnimation& animation,
                                     std::size_t action_index, float held_ms) {
  ComposedActor composed;
  if (animation.actions().size() <= action_index) {
    return composed;
  }
  const formats::ActAction& action = animation.actions()[action_index];
  if (action.frames.empty()) {
    return composed;
  }
  const float ticks = std::max(held_ms, 0.0F) / formats::kMillisecondsPerSpeedUnit;
  const float speed_units = std::max(action.speed_units, kLeastSpeedUnits);
  const auto step = static_cast<std::size_t>(std::floor(ticks / speed_units));
  const std::size_t frame = step % action.frames.size();
  for (const SpriteQuad& quad : BuildFrameQuads(sprite, animation, action_index, frame)) {
    composed.sources.push_back(SpriteImageKey{&sprite, nullptr, quad.section, quad.image_index});
    composed.quads.push_back(quad);
  }
  composed.action_index = action_index;
  composed.frame_index = frame;
  return composed;
}

}  // namespace ro::render
