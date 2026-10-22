#include "ro/render/wand_orb.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <utility>

#include "ro/formats/act.h"
#include "ro/formats/spr.h"
#include "ro/render/effect_node.h"

namespace ro::render {
namespace {

constexpr float kChannelTop = 255.0F;
constexpr std::size_t kRgbaChannels = 4;
// Le calque du disque : sa seule image, dans la section des images en RGBA.
constexpr std::int32_t kDiscImageIndex = 0;
constexpr std::int32_t kRgbaSection = 1;

// L'échelle de calque qui donne ce diamètre au disque.
[[nodiscard]] float ScaleFor(float diameter_pixels) {
  return diameter_pixels / static_cast<float>(kWandOrbDiscPixels);
}

[[nodiscard]] formats::ActFrame DiscFrame(float diameter_pixels) {
  formats::ActLayer layer;
  layer.frame_index = kDiscImageIndex;
  layer.section = kRgbaSection;
  layer.scale_x = ScaleFor(diameter_pixels);
  layer.scale_y = layer.scale_x;
  formats::ActFrame frame;
  frame.layers.push_back(layer);
  return frame;
}

// Une action qui passe d'un diamètre à l'autre en `frames` images, précédée
// de l'image vide qu'une pièce tient tant qu'elle attend.
[[nodiscard]] formats::ActAction HiddenThenRamp(float from_pixels, float to_pixels,
                                                std::int32_t frames) {
  formats::ActAction action;
  action.frames.emplace_back();
  const float last = static_cast<float>(std::max(frames - 1, 1));
  for (std::int32_t rank = 0; rank < frames; ++rank) {
    const float share = static_cast<float>(rank) / last;
    action.frames.push_back(DiscFrame(from_pixels + (to_pixels - from_pixels) * share));
  }
  return action;
}

[[nodiscard]] float JitterUnits(std::uint32_t& generator) {
  const float unit =
      static_cast<float>(NextEffectNodeDraw(generator)) / static_cast<float>(kEffectNodeDrawSpan);
  return (unit * 2.0F - 1.0F) * kWandOrbSparkJitterUnits;
}

}  // namespace

std::array<std::uint8_t, 3> WandOrbRgb(std::optional<ElementHue> hue) noexcept {
  return hue.has_value() ? ElementHueRgb(*hue) : kWandOrbArcaneRgb;
}

std::array<std::uint8_t, 3> WandOrbCoreRgb(std::array<std::uint8_t, 3> rgb) noexcept {
  std::array<std::uint8_t, 3> core = {};
  for (std::size_t channel = 0; channel < core.size(); ++channel) {
    const float value = static_cast<float>(rgb[channel]);
    core[channel] = static_cast<std::uint8_t>(
        std::lround(value + (kChannelTop - value) * kWandOrbCoreWhiteShare));
  }
  return core;
}

formats::Image WandOrbDiscImage() {
  formats::Image image;
  image.width = kWandOrbDiscPixels;
  image.height = kWandOrbDiscPixels;
  const auto side = static_cast<std::size_t>(kWandOrbDiscPixels);
  image.pixels.assign(side * side * kRgbaChannels, 0);
  const float radius = static_cast<float>(kWandOrbDiscPixels) / 2.0F;
  for (std::size_t row = 0; row < side; ++row) {
    for (std::size_t column = 0; column < side; ++column) {
      // Du centre du pixel au centre du disque, rapporté au rayon.
      const float dx = (static_cast<float>(column) + 0.5F - radius) / radius;
      const float dy = (static_cast<float>(row) + 0.5F - radius) / radius;
      const float reach = std::min(std::sqrt(dx * dx + dy * dy), 1.0F);
      const float alpha = std::pow(1.0F - reach, kWandOrbDiscFalloffExponent);
      const std::size_t offset = (row * side + column) * kRgbaChannels;
      image.pixels[offset + 0] = static_cast<std::uint8_t>(kChannelTop);
      image.pixels[offset + 1] = static_cast<std::uint8_t>(kChannelTop);
      image.pixels[offset + 2] = static_cast<std::uint8_t>(kChannelTop);
      image.pixels[offset + 3] = static_cast<std::uint8_t>(std::lround(alpha * kChannelTop));
    }
  }
  return image;
}

LoadedActorSprite WandOrbSprite() {
  std::vector<formats::ActAction> actions(kWandOrbPartCount);
  actions[static_cast<std::size_t>(WandOrbActionOf(WandOrbPart::kCore))].frames.push_back(
      DiscFrame(kWandOrbCoreDiameterPixels));
  // Le halo : une période de sinus, de diamètre en diamètre.
  formats::ActAction& halo = actions[static_cast<std::size_t>(WandOrbActionOf(WandOrbPart::kHalo))];
  for (std::int32_t rank = 0; rank < kWandOrbHaloPulseFrames; ++rank) {
    const float phase = 2.0F * std::numbers::pi_v<float> * static_cast<float>(rank) /
                        static_cast<float>(kWandOrbHaloPulseFrames);
    halo.frames.push_back(
        DiscFrame(kWandOrbHaloDiameterPixels * (1.0F + kWandOrbHaloPulseShare * std::sin(phase))));
  }
  actions[static_cast<std::size_t>(WandOrbActionOf(WandOrbPart::kSpark))] =
      HiddenThenRamp(kWandOrbSparkDiameterPixels,
                     kWandOrbSparkDiameterPixels * kWandOrbSparkEndShare, kWandOrbSparkFrames);
  actions[static_cast<std::size_t>(WandOrbActionOf(WandOrbPart::kBurst))] = HiddenThenRamp(
      kWandOrbBurstStartDiameterPixels, kWandOrbBurstEndDiameterPixels, kWandOrbBurstFrames);
  std::vector<formats::Image> images;
  images.push_back(WandOrbDiscImage());
  return LoadedActorSprite{formats::SprSprite::FromRgbaFrames(std::move(images)),
                           formats::ActAnimation::FromActions(std::move(actions))};
}

std::vector<WandOrbPiece> WandOrbPieces(glm::vec3 from, glm::vec3 to, std::int32_t flight_ticks,
                                        std::array<std::uint8_t, 3> rgb, std::uint32_t& generator) {
  const std::int32_t flight = std::max(flight_ticks, 1);
  std::vector<WandOrbPiece> pieces;
  pieces.push_back(WandOrbPiece{.part = WandOrbPart::kCore,
                                .birth = from,
                                .flies_to = to,
                                .life_ticks = flight,
                                .rgb = WandOrbCoreRgb(rgb),
                                .alpha = kWandOrbCoreAlpha,
                                .casts_shadow = true});
  pieces.push_back(WandOrbPiece{.part = WandOrbPart::kHalo,
                                .birth = from,
                                .flies_to = to,
                                .life_ticks = flight,
                                .frame_hold_ticks = kWandOrbHaloFrameHoldTicks,
                                .rgb = rgb,
                                .alpha = kWandOrbHaloAlpha});
  // LES ÉTINCELLES, au point où la boule passe à leur tic.
  std::int32_t sparks = 0;
  for (std::int32_t tick = kWandOrbSparkPeriodTicks;
       tick < flight && sparks < kWandOrbSparkCountMax; tick += kWandOrbSparkPeriodTicks) {
    const float share = static_cast<float>(tick) / static_cast<float>(flight);
    glm::vec3 place = from + (to - from) * share;
    place.x += JitterUnits(generator);
    place.y += JitterUnits(generator);
    place.z += JitterUnits(generator);
    pieces.push_back(WandOrbPiece{.part = WandOrbPart::kSpark,
                                  .birth = place,
                                  .life_ticks = tick + kWandOrbSparkFrames,
                                  .hidden_ticks = tick,
                                  .loops_frames = false,
                                  .rgb = rgb,
                                  .alpha = kWandOrbSparkAlpha,
                                  .fade_out_ticks = kWandOrbSparkFrames});
    ++sparks;
  }
  pieces.push_back(WandOrbPiece{.part = WandOrbPart::kBurst,
                                .birth = to,
                                .life_ticks = flight + kWandOrbBurstFrames,
                                .hidden_ticks = flight,
                                .loops_frames = false,
                                .rgb = rgb,
                                .alpha = kWandOrbBurstAlpha,
                                .fade_out_ticks = kWandOrbBurstFrames});
  return pieces;
}

}  // namespace ro::render
