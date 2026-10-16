// LE CIEL DE L'IMAGE ET L'OMBRE DU SOLEIL : quelle heure il fait, où se tient
// l'astre, et ce qu'il ombre. À part du dessin du monde, qui ne fait que lire
// leur résultat.

#include <chrono>
#include <optional>
#include <span>
#include <utility>

#include "ro/core/log.h"
#include "ro/render/sky_body.h"
#include "ro/render/sky_clock.h"
#include "ro/render/sun_shadow_pass.h"

#include "game/game.h"
#include "world/scene_graphics.h"
#include "world/server_sky_hour.h"

namespace moonclient {

bool Game::SunShadowsAsked(const ro::render::TerrainStages& stages) const {
  return !sun_map_refused_ && SunShadowsApply(settings_->options().graphics(), stages,
                                              camera_rig_.limits().rules.indoor);
}

ro::render::SkyLight Game::SkyOfFrame(std::uint32_t now_ms) {
  const std::optional<std::uint32_t> last_ms = std::exchange(last_sky_ms_, now_ms);
  ro::ui::StaffSkyClock& clock = windows_.staff_tools.sky;
  const ro::render::SceneLight& map_light = pieces_.night_light.light();
  // LA COURSE que l'établi règle, pour l'heure forcée comme pour le cycle.
  const ro::ui::StaffToolsLook& look = windows_.staff_tools.look;
  const ro::render::SkyCourse course =
      Rights().staff_tools && look.sky_course_tuned ? look.sky_course : ro::render::SkyCourse();
  if (!Rights().staff_tools || !clock.forced) {
    // LE CYCLE DU JOUEUR, en plein air : l'heure qu'il fait sur le serveur. Un
    // intérieur, une caverne ou un donjon n'a ni soleil ni lune.
    const bool closed = camera_rig_.limits().rules.indoor ||
                        (stage_.has_value() && closed_maps_.Closes(stage_->name));
    if (settings_->options().graphics().day_night_cycle() && !closed) {
      if (const std::optional<float> hour =
              ServerSkyHour(shell_.server_time_zone(), std::chrono::system_clock::now());
          hour.has_value()) {
        return ro::render::SkyLightAt(map_light, *hour, course);
      }
      if (!sky_zone_unknown_reported_) {
        sky_zone_unknown_reported_ = true;
        ro::core::LogWarning(
            "cycle jour/nuit : fuseau « {} » inconnu du système — lumière de "
            "la carte gardée",
            shell_.server_time_zone());
      }
    }
    // SINON, le ciel de la carte : la lumière du RSW, teintée par la nuit que le
    // serveur pose.
    return ro::render::SkyLight{.light = map_light};
  }
  if (clock.running && last_ms.has_value()) {
    clock.hour = ro::render::AdvanceSkyHour(clock.hour, static_cast<float>(now_ms - *last_ms),
                                            clock.HoursPerMinute());
  }
  return ro::render::SkyLightAt(map_light, clock.hour, course);
}

ro::render::SunShadowView Game::DrawSunShadowIfAsked(bool asked, bool models_shown,
                                                     std::span<const ro::render::DrawnActor> carts,
                                                     const ro::render::SkyLight& sky) {
  if (!asked) {
    return ro::render::SunShadowView();
  }
  if (!sun_map_.valid()) {
    auto created = ro::render::SunShadowMap::Create(ro::render::kSunShadowMapTexels);
    if (!created) {
      sun_map_refused_ = true;
      return ro::render::SunShadowView();
    }
    sun_map_ = std::move(*created);
  }
  const bool actor_models_shown = link_.is_open() && pieces_.actor_models.has_value() &&
                                  pieces_.actor_model_renderer.has_value();
  const ro::render::SunShadowCasters casters{
      .terrain = &terrain_,
      .models = models_shown ? &models_ : nullptr,
      .clock = &model_clock_,
      .sprites = &sprites_,
      .actor_lots = {scene_actors_.drawn, carts},
      .actor_model_renderer = actor_models_shown ? &*pieces_.actor_model_renderer : nullptr,
      .actor_models = actor_models_shown ? pieces_.actor_models->drawn()
                                         : std::span<const ro::render::DrawnActorModel>()};
  // L'ASTRE DU CIEL porte l'ombre, et la lune la porte plus légère.
  return ro::render::DrawSunShadow(sun_map_, casters, stage_->scene.terrain, sky.CastingLight(),
                                   camera_, aspect_ratio_,
                                   ro::render::kFullSunShadow * sky.shadow_strength);
}

void Game::ShadeVoid(const ro::render::SkyLight& sky) {
  if ((sky.light.daylight == glm::vec3(1.0F) && !sky.body_shown) || void_shade_refused_) {
    return;
  }
  if (!void_shade_.valid()) {
    auto created = ro::render::VoidShade::Create();
    if (!created) {
      void_shade_refused_ = true;
      return;
    }
    void_shade_ = std::move(*created);
  }
  void_shade_.Draw(sky.light.daylight);
  // L'ASTRE, à sa vraie place : la caméra de jeu, qui plonge, ne le voit pas.
  if (sky.body_shown) {
    void_shade_.DrawBody(ro::render::PlaceSkyBody(sky.body_direction, camera_, aspect_ratio_,
                                                  sky.moon ? ro::render::kMoonAngularRadiusDegrees
                                                           : ro::render::kSunAngularRadiusDegrees),
                         sky.moon, aspect_ratio_);
  }
}

}  // namespace moonclient
