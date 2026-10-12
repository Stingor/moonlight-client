// Le Warp Agent de la carte du monde. Voir l'en-tête.

#include "world/world_map_warp_agent.h"

#include <algorithm>

#include "ro/render/sprite_frame.h"

namespace moonclient {
namespace {

constexpr std::uint32_t kTourMs = 4000;
constexpr float kFirstScale = 0.5F;
constexpr float kScaleStep = 0.2F;
// La boîte où il se compose, assez grande pour la plus grande échelle.
constexpr std::int32_t kBoxPixels = 256;

}  // namespace

std::size_t WorldMapWarpAgentFrame(std::size_t frames, std::uint32_t now_ms) {
  if (frames == 0) {
    return 0;
  }
  const std::uint32_t into_tour = now_ms % kTourMs;
  const auto per_frame = static_cast<std::uint32_t>(kTourMs / frames);
  const std::size_t frame = into_tour == 0 || per_frame == 0 ? 0 : into_tour / per_frame;
  return std::min(frame, frames - 1);
}

float WorldMapWarpAgentScale(std::size_t size_tier) {
  return kFirstScale + kScaleStep * static_cast<float>(size_tier);
}

WorldMapWarpAgent::Shown WorldMapWarpAgent::Compose(std::uint32_t job, float scale,
                                                    std::uint32_t now_ms) {
  ro::render::ActorLook look;
  look.job = job;
  const ro::render::ActorOutfit* outfit = wardrobe_->OutfitFor(look);
  if (outfit == nullptr) {
    return {};
  }
  ro::render::ActorVignette::Pose pose;
  pose.look = look;
  pose.animation = ro::render::kAnimationIdle;
  pose.direction = 0;
  pose.held_frame =
      WorldMapWarpAgentFrame(outfit->BodyActionFrameCount(ro::render::kAnimationIdle, 0), now_ms);
  pose.box.width = kBoxPixels;
  pose.box.height = kBoxPixels;
  pose.box.scale = scale;
  const ro::render::IconTexture& texture = vignette_.Get(pose);
  if (!texture.usable() || !vignette_.framing().usable()) {
    return {};
  }
  const ro::render::VignetteFraming& framing = vignette_.framing();
  return Shown{.image = ro::ui::SkinImage{.id = static_cast<ImTextureID>(texture.texture.name()),
                                          .width = static_cast<float>(texture.width_px),
                                          .height = static_cast<float>(texture.height_px)},
               .feet = ImVec2(framing.origin.x, framing.origin.y),
               .height = -vignette_.drawn_bounds().min.y * framing.scale};
}

}  // namespace moonclient
