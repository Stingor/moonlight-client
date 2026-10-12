#include "ro/render/portrait_framing.h"

#include <cstddef>

namespace ro::render {
namespace {

constexpr float kHalf = 0.5F;
// Un cadre a deux bords par axe, et l'écart se prend des deux côtés.
constexpr float kEdgesPerAxis = 2.0F;

// Fait grandir une boîte pour qu'elle couvre les coins d'un quad. `grown` dit
// si elle a déjà été touchée : partir de zéro y ferait entrer l'origine.
void Grow(SpriteBounds& bounds, bool& grown, const SpriteQuad& quad) {
  for (const glm::vec2& corner : quad.corners) {
    if (!grown) {
      bounds.min = corner;
      bounds.max = corner;
      grown = true;
      continue;
    }
    bounds.min = glm::min(bounds.min, corner);
    bounds.max = glm::max(bounds.max, corner);
  }
}

[[nodiscard]] bool IsHeadPart(ActorPart part) noexcept {
  return part == ActorPart::kHead || part == ActorPart::kHeadgearLow ||
         part == ActorPart::kHeadgearMiddle || part == ActorPart::kHeadgearTop;
}

[[nodiscard]] glm::vec2 CenterOf(const SpriteBounds& bounds) {
  return (bounds.min + bounds.max) * kHalf;
}

[[nodiscard]] SpriteBounds Union(const SpriteBounds& one, const SpriteBounds& other) {
  return SpriteBounds{.min = glm::min(one.min, other.min), .max = glm::max(one.max, other.max)};
}

}  // namespace

PortraitBoxes MeasurePortraitBoxes(std::span<const SpriteQuad> quads,
                                   std::span<const ActorPart> parts) {
  PortraitBoxes boxes;
  for (std::size_t index = 0; index < quads.size(); ++index) {
    const ActorPart part = index < parts.size() ? parts[index] : ActorPart::kBody;
    if (IsHeadPart(part)) {
      Grow(boxes.head, boxes.has_head, quads[index]);
    } else if (part == ActorPart::kBody) {
      Grow(boxes.body, boxes.has_body, quads[index]);
    }
  }
  return boxes;
}

VignetteFraming FramePortrait(const PortraitBoxes& boxes, const PortraitAim& aim,
                              glm::vec2 box_size) {
  VignetteFraming framing;
  if (!boxes.has_head && !boxes.has_body) {
    return framing;
  }
  SpriteBounds framed = boxes.has_head ? boxes.head : boxes.body;
  if (!(aim.head_only && boxes.has_head) && boxes.has_head && boxes.has_body) {
    framed = Union(boxes.head, boxes.body);
  }
  const glm::vec2 extent = framed.max - framed.min;
  if (extent.x <= kPortraitLeastBoxPx || extent.y <= kPortraitLeastBoxPx) {
    return framing;
  }

  // Le cadre entier, et non la seule boîte dessinée : c'est sur lui que le
  // joueur règle sa vue.
  const glm::vec2 frame = box_size + glm::vec2(kEdgesPerAxis * aim.inset_px);
  const float scale = ((frame.x - kPortraitFitMarginPx) / extent.x) * aim.zoom;
  if (scale <= 0.0F) {
    return framing;
  }

  glm::vec2 focus = CenterOf(framed);
  if (!aim.head_only && boxes.has_head && boxes.has_body) {
    focus = (CenterOf(boxes.head) + CenterOf(boxes.body)) * kHalf;
  } else if (boxes.has_head) {
    focus = CenterOf(boxes.head);
  }

  framing.scale = scale;
  framing.origin =
      frame * (glm::vec2(kHalf) + aim.offset) - focus * scale - glm::vec2(aim.inset_px);
  return framing;
}

}  // namespace ro::render
