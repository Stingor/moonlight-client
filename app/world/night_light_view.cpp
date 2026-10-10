#include "world/night_light_view.h"

#include "ro/sim/status_effects.h"

namespace moonclient {

void NightLightView::Advance(const MapStage& stage, const ro::sim::World& world,
                             std::uint32_t now_ms) {
  const Entered now{.map_name = stage.name, .models = stage.scene.models.data()};
  if (!entered_.has_value() || entered_->map_name != now.map_name ||
      entered_->models != now.models) {
    day_night_.Enter(stage.scene.light);
    entered_ = now;
    last_ms_.reset();
  }
  const std::uint32_t elapsed_ms =
      last_ms_.has_value() && now_ms > *last_ms_ ? now_ms - *last_ms_ : 0;
  last_ms_ = now_ms;
  const ro::sim::Actor* player = world.player();
  const bool night = player != nullptr && ro::sim::DarkensTheMap(player->status_effects);
  day_night_.Advance(night, static_cast<float>(elapsed_ms));
  light_ = day_night_.Lit(stage.scene.light);
}

}  // namespace moonclient
