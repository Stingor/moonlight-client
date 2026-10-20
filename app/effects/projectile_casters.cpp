#include "effects/projectile_casters.h"

#include <utility>

#include "ro/render/sprite_frame.h"

namespace moonclient {

void ProjectileCasters::Clear() {
  projectiles_.clear();
  silhouettes_.clear();
  ground_shadows_.clear();
}

void ProjectileCasters::ComposeRoundShadow(const ro::render::LoadedActorSprite* shadow_sprite) {
  round_shadow_known_ = true;
  if (shadow_sprite == nullptr || shadow_sprite->animation.actions().empty()) {
    return;
  }
  auto composed = std::make_unique<ro::render::ComposedActor>();
  composed->quads =
      ro::render::BuildFrameQuads(shadow_sprite->sprite, shadow_sprite->animation, 0, 0);
  for (ro::render::SpriteQuad& quad : composed->quads) {
    for (glm::vec2& corner : quad.corners) {
      corner *= kProjectileShadowScale;
    }
    composed->sources.push_back(ro::render::SpriteImageKey{&shadow_sprite->sprite, nullptr,
                                                           quad.section, quad.image_index});
  }
  if (!composed->quads.empty()) {
    round_shadow_ = std::move(composed);
  }
}

void ProjectileCasters::Add(const ro::render::ComposedActor& projectile, glm::vec3 flight_position,
                            std::optional<float> ground_height) {
  projectiles_.push_back(projectile);
  ro::render::DrawnActor silhouette;
  silhouette.actor = &projectiles_.back();
  silhouette.ground_position = flight_position;
  silhouettes_.push_back(silhouette);
  if (round_shadow_ != nullptr) {
    ro::render::DrawnActor shadow;
    shadow.shadow = round_shadow_.get();
    shadow.ground_position = flight_position;
    shadow.shadow_ground_height = ground_height;
    ground_shadows_.push_back(shadow);
  }
}

std::span<const ro::render::DrawnActor> ProjectileCasters::GroundShadows(
    bool sun_shadows) const noexcept {
  if (sun_shadows) {
    return {};
  }
  return ground_shadows_;
}

std::vector<ro::render::DrawnActor> JoinedCasterLots(
    std::initializer_list<std::span<const ro::render::DrawnActor>> lots) {
  std::vector<ro::render::DrawnActor> joined;
  for (const std::span<const ro::render::DrawnActor> lot : lots) {
    joined.insert(joined.end(), lot.begin(), lot.end());
  }
  return joined;
}

}  // namespace moonclient
