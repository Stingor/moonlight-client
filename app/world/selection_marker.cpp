#include "world/selection_marker.h"

#include "ro/render/sprite_frame.h"

namespace moonclient {
namespace {

// L'action du fichier des curseurs qui porte la flèche de sélection.
constexpr std::size_t kSelectionMarkerAction = 3;

}  // namespace

std::span<const ro::render::DrawnActor> SelectionMarker::Drawn(
    const CursorSprite& art, const glm::vec3& ground, std::uint32_t since_map_entry_ms,
    const std::array<std::uint8_t, 4>& tint) {
  frame_ = ro::render::ComposedActor();
  if (art.sprite == nullptr || art.animation == nullptr ||
      art.animation->actions().size() <= kSelectionMarkerAction) {
    return {};
  }
  const std::size_t frame = ro::render::FrameAtTime(
      art.animation->actions()[kSelectionMarkerAction], static_cast<float>(since_map_entry_ms));
  for (const ro::render::SpriteQuad& quad :
       ro::render::BuildFrameQuads(*art.sprite, *art.animation, kSelectionMarkerAction, frame)) {
    frame_.sources.push_back(
        ro::render::SpriteImageKey{art.sprite, nullptr, quad.section, quad.image_index});
    frame_.quads.push_back(quad);
  }
  if (frame_.quads.empty()) {
    return {};
  }
  drawn_ = ro::render::DrawnActor{&frame_, nullptr, ground, tint};
  return std::span(&drawn_, 1);
}

}  // namespace moonclient
