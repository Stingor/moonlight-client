#include "ro/render/emotion_icons.h"

#include <algorithm>
#include <optional>
#include <utility>

#include "ro/formats/image.h"
#include "ro/render/actor_sprite.h"
#include "ro/render/emotion.h"

namespace ro::render {
namespace {

// L'instant qui tombe au MILIEU d'une image, et non à son début :
// `ComposeEmotion` convertit un temps en numéro d'image, et un instant posé pile
// sur la frontière tomberait d'un côté ou de l'autre selon l'arrondi.
[[nodiscard]] float MiddleOfFrame(std::uint16_t frame, float frame_ms) {
  return (static_cast<float>(frame) + 0.5F) * frame_ms;
}

void Stretch(EmotionBounds& box, const std::array<glm::vec2, 4>& corners, bool& any) {
  for (const glm::vec2& corner : corners) {
    if (!any) {
      box.min = corner;
      box.max = corner;
      any = true;
      continue;
    }
    box.min.x = std::min(box.min.x, corner.x);
    box.min.y = std::min(box.min.y, corner.y);
    box.max.x = std::max(box.max.x, corner.x);
    box.max.y = std::max(box.max.y, corner.y);
  }
}

}  // namespace

const formats::ActAction* EmotionIcons::ActionOf(std::uint16_t emotion) const {
  const std::optional<std::size_t> action = EmotionAction(emotion);
  if (animation_ == nullptr || !action.has_value() || *action >= animation_->actions().size()) {
    return nullptr;
  }
  return &animation_->actions()[*action];
}

std::uint16_t EmotionIcons::FrameCount(std::uint16_t emotion) const {
  const formats::ActAction* action = ActionOf(emotion);
  return action == nullptr ? 0 : static_cast<std::uint16_t>(action->frames.size());
}

float EmotionIcons::FrameMs(std::uint16_t emotion) const {
  const formats::ActAction* action = ActionOf(emotion);
  return action == nullptr ? formats::kDefaultSpeedUnits * formats::kMillisecondsPerSpeedUnit
                           : action->frame_interval_ms();
}

EmotionBounds EmotionIcons::ActionBounds(std::uint16_t emotion) {
  const auto known = boxes_.find(emotion);
  if (known != boxes_.end()) {
    return known->second;
  }
  EmotionBounds& box = boxes_[emotion];
  if (sprite_ == nullptr || animation_ == nullptr) {
    return box;
  }
  // ⚠ SUR TOUTES LES IMAGES, et sans rien téléverser : c'est de la GÉOMÉTRIE.
  // Composer suffit ; les pixels n'entrent pas dans le calcul.
  const float frame_ms = FrameMs(emotion);
  bool any = false;
  for (std::uint16_t frame = 0; frame < FrameCount(emotion); ++frame) {
    const ComposedActor shown =
        ComposeEmotion(*sprite_, *animation_, emotion, MiddleOfFrame(frame, frame_ms));
    for (const SpriteQuad& quad : shown.quads) {
      Stretch(box, quad.corners, any);
    }
  }
  return box;
}

std::span<const EmotionLayer> EmotionIcons::Layers(std::uint16_t emotion, std::uint16_t frame) {
  const std::uint32_t key = KeyOf(emotion, frame);
  const auto known = loaded_.find(key);
  if (known != loaded_.end()) {
    return known->second.layers;
  }
  // ⚠ LA CASE SE POSE D'ABORD, ET VIDE. C'est elle qui retient l'absence : sans
  // cela, une émoticône que le sprite n'a pas se redemanderait à chaque image.
  LoadedFrame& loaded = loaded_[key];
  if (sprite_ == nullptr || animation_ == nullptr) {
    return loaded.layers;
  }

  const ComposedActor shown =
      ComposeEmotion(*sprite_, *animation_, emotion, MiddleOfFrame(frame, FrameMs(emotion)));
  const std::vector<formats::Image> images = ResolveImages(shown);
  if (images.size() != shown.quads.size()) {
    return loaded.layers;  // le sprite ne porte pas cette action-là
  }
  // ⚠ LES DEUX TABLEAUX SE RÉSERVENT D'ABORD : `layers` garde le nom d'une
  // texture qui vit dans `textures`, et une réallocation en cours de route
  // déplacerait celles qu'on a déjà téléversées.
  loaded.textures.reserve(images.size());
  loaded.layers.reserve(images.size());
  for (std::size_t rank = 0; rank < images.size(); ++rank) {
    auto texture = GlTexture::Upload(images[rank], GlTexture::Filtering::kPixelArt);
    if (!texture) {
      continue;
    }
    loaded.textures.push_back(std::move(*texture));
    loaded.layers.push_back(EmotionLayer{.texture = loaded.textures.back().name(),
                                         .corners = shown.quads[rank].corners,
                                         .tint = shown.quads[rank].tint});
  }
  return loaded.layers;
}

}  // namespace ro::render
