#include "ro/render/portrait_framing.h"

#include <cstddef>

#include "ro/render/doll_animation.h"
#include "ro/render/sprite_canvas.h"

namespace ro::render {
namespace {

constexpr float kHalf = 0.5F;
// Un cadre a deux bords par axe, et l'écart se prend des deux côtés.
constexpr float kEdgesPerAxis = 2.0F;

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

// Fait grandir une boîte pour qu'elle couvre une autre, si celle-ci existe.
void Absorb(SpriteBounds& bounds, bool& grown, const SpriteBounds& other, bool other_grown) {
  if (!other_grown) {
    return;
  }
  bounds = grown ? Union(bounds, other) : other;
  grown = true;
}

// Deux apparences sont les mêmes quand aucune ne précède l'autre : on compare
// les champs, on ne hache pas.
[[nodiscard]] bool SameLook(const ActorLook& left, const ActorLook& right) {
  const ActorLookOrder before;
  return !before(left, right) && !before(right, left);
}

[[nodiscard]] bool SameLoop(const PortraitLoop& left, const PortraitLoop& right) {
  return left.animation == right.animation && left.direction == right.direction &&
         left.body_plays == right.body_plays && left.rest_frame == right.rest_frame;
}

}  // namespace

PortraitBoxes MeasurePortraitBoxes(std::span<const SpriteQuad> quads,
                                   std::span<const ActorPart> parts) {
  PortraitBoxes boxes;
  for (std::size_t index = 0; index < quads.size(); ++index) {
    const ActorPart part = index < parts.size() ? parts[index] : ActorPart::kBody;
    if (IsHeadPart(part)) {
      GrowBounds(boxes.head, boxes.has_head, quads[index]);
    } else if (part == ActorPart::kBody) {
      GrowBounds(boxes.body, boxes.has_body, quads[index]);
    }
  }
  return boxes;
}

PortraitBoxes MeasurePaintedPortraitBoxes(const ComposedActor& composed) {
  PortraitBoxes boxes;
  for (std::size_t index = 0; index < composed.quads.size(); ++index) {
    if (index >= composed.sources.size() || !HasImage(composed.sources[index])) {
      continue;
    }
    const ActorPart part = PartOfQuad(composed, index);
    if (IsHeadPart(part)) {
      GrowBounds(boxes.head, boxes.has_head, composed.quads[index]);
    } else if (part == ActorPart::kBody) {
      GrowBounds(boxes.body, boxes.has_body, composed.quads[index]);
    }
  }
  return boxes;
}

PortraitBoxes MeasureLoopPortraitBoxes(ActorOutfit& outfit, const PortraitLoop& loop) {
  const DollClocks clocks = DollClocksOf(outfit, loop.animation, loop.direction, loop.body_plays);
  PortraitBoxes envelope;
  for (const DollProbe& probe : DollEnvelopeProbes(clocks, loop.rest_frame)) {
    const PortraitBoxes instant = MeasurePaintedPortraitBoxes(
        ComposeDollProbe(outfit, loop.animation, loop.direction, probe));
    Absorb(envelope.head, envelope.has_head, instant.head, instant.has_head);
    Absorb(envelope.body, envelope.has_body, instant.body, instant.has_body);
  }
  return envelope;
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

VignetteFraming PortraitFramer::Frame(ActorOutfit& outfit, const ActorLook& look,
                                      const PortraitLoop& loop, const PortraitAim& aim,
                                      glm::vec2 box_size) {
  const bool same_pose =
      measured_ && outfit_ == &outfit && SameLook(look, look_) && SameLoop(loop, loop_);
  if (!same_pose) {
    boxes_ = MeasureLoopPortraitBoxes(outfit, loop);
    outfit_ = &outfit;
    look_ = look;
    loop_ = loop;
    measured_ = true;
    ++measure_count_;
  }
  return FramePortrait(boxes_, aim, box_size);
}

}  // namespace ro::render
