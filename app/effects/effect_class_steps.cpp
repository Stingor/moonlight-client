#include "effects/effect_class_steps.h"

#include <algorithm>
#include <span>
#include <utility>

#include "ro/render/effect_step_blend.h"

namespace moonclient {

void EffectClassSteps::BeginFrame(std::uint32_t ticks, float alpha) {
  spans_.clear();
  if (!enabled_) {
    if (has_last_ || has_previous_) {
      Forget();
    }
    return;
  }
  alpha_ = alpha;
  if (ticks > 0) {
    has_previous_ = has_last_;
    if (has_last_) {
      previous_ = std::move(last_);
      std::ranges::sort(previous_.spans, {}, &InstanceSpan::serial);
    }
    last_ = Kept{};
    has_last_ = false;
  }
}

std::array<std::size_t, EffectClassSteps::kKinds> EffectClassSteps::Sizes(
    const ro::render::EffectClassOutput& output) {
  return {output.strs.size(), output.quads.size(), output.sprites.size(),
          output.screen_quads.size()};
}

void EffectClassSteps::OpenInstance(std::uint64_t serial,
                                    const ro::render::EffectClassOutput& output) {
  if (!enabled_) {
    return;
  }
  spans_.push_back(InstanceSpan{.serial = serial, .first = Sizes(output), .end = Sizes(output)});
}

void EffectClassSteps::CloseInstance(const ro::render::EffectClassOutput& output) {
  if (!enabled_ || spans_.empty()) {
    return;
  }
  spans_.back().end = Sizes(output);
}

EffectClassSteps::Kept EffectClassSteps::Keep(const ro::render::EffectClassOutput& output,
                                              std::vector<InstanceSpan> spans) {
  Kept kept;
  kept.spans = std::move(spans);
  kept.strs = output.strs;
  for (const ro::render::EffectClassQuadDraw& quad : output.quads) {
    kept.quads.push_back(KeptQuad{std::string(quad.texture), quad});
    kept.quads.back().draw.texture = {};
  }
  for (const ro::render::EffectClassSpriteDraw& sprite : output.sprites) {
    kept.sprites.push_back(KeptSprite{std::string(sprite.sprite), sprite});
    kept.sprites.back().draw.sprite = {};
  }
  for (const ro::render::EffectClassScreenDraw& screen : output.screen_quads) {
    kept.screens.push_back(KeptScreen{std::string(screen.texture), screen});
    kept.screens.back().draw.texture = {};
  }
  return kept;
}

void EffectClassSteps::Blend(ro::render::EffectClassOutput& output) {
  if (!enabled_) {
    return;
  }
  Kept raw = Keep(output, std::move(spans_));
  spans_.clear();
  if (has_previous_) {
    for (const InstanceSpan& now : raw.spans) {
      const auto before =
          std::ranges::lower_bound(previous_.spans, now.serial, {}, &InstanceSpan::serial);
      if (before != previous_.spans.end() && before->serial == now.serial) {
        BlendInstance(now, *before, output);
      }
    }
  }
  last_ = std::move(raw);
  has_last_ = true;
}

void EffectClassSteps::BlendInstance(const InstanceSpan& now, const InstanceSpan& before,
                                     ro::render::EffectClassOutput& output) const {
  const auto count = [](const InstanceSpan& span, Kind kind) {
    return span.end[kind] - span.first[kind];
  };
  if (count(now, kStrs) == count(before, kStrs)) {
    for (std::size_t rank = 0; rank < count(now, kStrs); ++rank) {
      ro::render::EffectClassStrDraw& str = output.strs[now.first[kStrs] + rank];
      const ro::render::EffectClassStrDraw& old = previous_.strs[before.first[kStrs] + rank];
      if (str.path != old.path || str.behind_actors != old.behind_actors) {
        continue;
      }
      str.ground_position =
          ro::render::BlendEffectPoint(old.ground_position, {}, str.ground_position, alpha_);
      str.spin_degrees = ro::render::MixEffectDegrees(old.spin_degrees, str.spin_degrees, alpha_);
      str.scale = ro::render::MixEffectValue(old.scale, str.scale, alpha_);
      str.alpha = ro::render::MixEffectValue(old.alpha, str.alpha, alpha_);
      str.lift_pixels = ro::render::MixEffectValue(old.lift_pixels, str.lift_pixels, alpha_);
      if (old.frame + 1 == str.frame) {
        str.frame_fraction = alpha_ - 1.0F;
      }
    }
  }
  if (count(now, kQuads) == count(before, kQuads)) {
    for (std::size_t rank = 0; rank < count(now, kQuads); ++rank) {
      ro::render::EffectClassQuadDraw& quad = output.quads[now.first[kQuads] + rank];
      const KeptQuad& old = previous_.quads[before.first[kQuads] + rank];
      if (quad.texture != old.texture || quad.render_word != old.draw.render_word ||
          quad.quad.texture != old.draw.quad.texture) {
        continue;
      }
      ro::render::BlendEffectWorldQuad(old.draw.quad, {}, alpha_, quad.quad);
    }
  }
  if (count(now, kSprites) == count(before, kSprites)) {
    for (std::size_t rank = 0; rank < count(now, kSprites); ++rank) {
      ro::render::EffectClassSpriteDraw& sprite = output.sprites[now.first[kSprites] + rank];
      const KeptSprite& old = previous_.sprites[before.first[kSprites] + rank];
      if (sprite.sprite != old.sprite) {
        continue;
      }
      sprite.ground_position = ro::render::BlendEffectPoint(old.draw.ground_position, {},
                                                            sprite.ground_position, alpha_);
      sprite.scale = ro::render::MixEffectValue(old.draw.scale, sprite.scale, alpha_);
      sprite.spin_degrees =
          ro::render::MixEffectDegrees(old.draw.spin_degrees, sprite.spin_degrees, alpha_);
      sprite.tint = ro::render::BlendEffectTint(old.draw.tint, sprite.tint, alpha_);
      sprite.screen_lift_pixels = ro::render::MixEffectValue(old.draw.screen_lift_pixels,
                                                             sprite.screen_lift_pixels, alpha_);
    }
  }
  if (count(now, kScreens) == count(before, kScreens)) {
    for (std::size_t rank = 0; rank < count(now, kScreens); ++rank) {
      ro::render::EffectClassScreenDraw& screen = output.screen_quads[now.first[kScreens] + rank];
      const KeptScreen& old = previous_.screens[before.first[kScreens] + rank];
      if (screen.texture != old.texture) {
        continue;
      }
      static_cast<void>(ro::render::BlendEffectScreenQuads(std::span(&old.draw.quad, 1), {}, alpha_,
                                                           std::span(&screen.quad, 1)));
    }
  }
}

void EffectClassSteps::Forget() {
  spans_.clear();
  last_ = Kept{};
  previous_ = Kept{};
  has_last_ = false;
  has_previous_ = false;
}

}  // namespace moonclient
