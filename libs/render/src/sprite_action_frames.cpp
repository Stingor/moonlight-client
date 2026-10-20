#include "ro/render/sprite_action_frames.h"

#include <cmath>
#include <cstdint>

#include <glm/glm.hpp>

#include "ro/render/actor_sprite.h"
#include "ro/render/sprite_canvas.h"
#include "ro/render/sprite_frame.h"

namespace ro::render {

ActionFrames ComposeActionFrames(const formats::SprSprite& sprite,
                                 const formats::ActAnimation& animation, std::size_t action_index) {
  ActionFrames composed;
  if (action_index >= animation.actions().size()) {
    return composed;
  }
  const formats::ActAction& action = animation.actions()[action_index];
  std::vector<std::vector<SpriteQuad>> frame_quads;
  frame_quads.reserve(action.frames.size());
  SpriteBounds bounds;
  bool grown = false;
  for (std::size_t frame = 0; frame < action.frames.size(); ++frame) {
    std::vector<SpriteQuad> quads =
        BuildFrameQuads(sprite, animation, action_index, frame, /*apply_rotation=*/true);
    for (const SpriteQuad& quad : quads) {
      GrowBounds(bounds, grown, quad);
    }
    frame_quads.push_back(std::move(quads));
  }
  if (!grown || bounds.empty()) {
    return composed;
  }
  const auto width = static_cast<std::int32_t>(std::ceil(bounds.max.x - bounds.min.x));
  const auto height = static_cast<std::int32_t>(std::ceil(bounds.max.y - bounds.min.y));
  composed.frame_interval_ms = action.frame_interval_ms();
  composed.frames.reserve(frame_quads.size());
  for (const std::vector<SpriteQuad>& quads : frame_quads) {
    std::vector<formats::Image> images;
    images.reserve(quads.size());
    for (const SpriteQuad& quad : quads) {
      images.push_back(ResolveImage(SpriteImageKey{.sprite = &sprite,
                                                   .palette = nullptr,
                                                   .section = quad.section,
                                                   .image_index = quad.image_index}));
    }
    SpriteCanvas canvas(width, height);
    canvas.Draw(quads, images, -bounds.min);
    composed.frames.push_back(canvas.image());
  }
  return composed;
}

}  // namespace ro::render
