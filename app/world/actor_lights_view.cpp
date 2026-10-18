#include "world/actor_lights_view.h"

namespace moonclient {

std::span<const ro::render::PointLight> SceneActorLights::Gather(
    std::span<const ro::render::ActorLightBearer> bearers, const ro::render::SpriteTables* tables,
    bool asked, double time_ms) {
  lights_.clear();
  if (!asked || tables == nullptr) {
    return lights_;
  }
  if (read_from_ != &tables->class_sprites) {
    catalogue_ = ro::render::ActorLightCatalogue::FromClassSprites(tables->class_sprites);
    read_from_ = &tables->class_sprites;
  }
  ro::render::ActorLightsOf(bearers, catalogue_, time_ms, lights_);
  return lights_;
}

}  // namespace moonclient
