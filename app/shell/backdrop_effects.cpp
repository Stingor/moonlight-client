#include "shell/backdrop_effects.h"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "ro/core/log.h"
#include "ro/render/edited_map_effects.h"
#include "ro/render/map_effect_lots.h"
#include "ro/render/map_effect_math.h"
#include "ro/render/map_smoke.h"
#include "ro/sim/movement.h"
#include "ro/sim/world.h"

#include "effects/effect_layers.h"

namespace moonclient {

BackdropEffects::BackdropEffects(ro::vfs::Vfs& vfs)
    : vfs_(&vfs), catalog_(vfs), nodes_(vfs), effects_(catalog_, nodes_, vfs) {
  if (auto renderer = ro::render::EffectRenderer::Create(); renderer) {
    renderer_ = std::move(*renderer);
  } else {
    ro::core::LogWarning("décor : les effets ne se peignent pas — {}",
                         ro::core::Describe(renderer.error()));
  }
  if (auto ground = ro::render::GroundQuadRenderer::Create(); ground) {
    ground_ = std::move(*ground);
  }
}

void BackdropEffects::DrawBehindActors(const ro::render::Camera& camera, float aspect_ratio,
                                       const ro::render::SceneFog& fog,
                                       ro::render::SpriteRenderer& sprites) {
  if (!renderer_.has_value()) {
    return;
  }
  if (const std::span<const ro::render::DrawnEffect> behind = effects_.DrawnBehindActors();
      !behind.empty()) {
    renderer_->Draw(camera, behind, aspect_ratio);
  }
  PaintClassSpritesBehindActors(effects_, sprites, camera, aspect_ratio, fog);
}

void BackdropEffects::DrawMapEffects(const MapStage& stage, const ro::render::Camera& camera,
                                     float aspect_ratio, const ro::render::SceneFog& fog) {
  if (!renderer_.has_value()) {
    return;
  }
  map_quads_.clear();
  for (const ro::render::MapEffectLot& lot : map_effects_.Lots(
           camera, stage.scene.chimney_smoke.has_value() ? &*stage.scene.chimney_smoke : nullptr)) {
    if (!effects_.ShowsEffect(static_cast<std::uint32_t>(lot.effect_id))) {
      continue;
    }
    if (const std::optional<ro::render::DrawnWorldQuads> drawn =
            ro::render::DrawnMapEffectLot(lot, stage.scene.effect_textures)) {
      map_quads_.push_back(*drawn);
    }
  }
  if (!map_quads_.empty()) {
    renderer_->DrawWorldQuads(camera, map_quads_, aspect_ratio, fog);
  }
}

void BackdropEffects::DrawAfterActors(MapStage& stage, const SoundStage& scene, ActorSounds& sounds,
                                      ItemDrops& drops, ro::render::SpriteRenderer& sprites,
                                      glm::vec3 anchor, float aspect_ratio, float viewport_width,
                                      float viewport_height, const ro::render::SceneFog& fog,
                                      std::vector<ro::formats::RswEffect> map_objects) {
  if (!renderer_.has_value() || !ground_.has_value()) {
    return;
  }
  RaiseSky(stage, anchor);
  AdvanceMapEffects(stage, scene, sounds, anchor, std::move(map_objects));
  DrawEffectLayers(effects_, nodes_, sounds, footprints_, drops, scene, sprites, *renderer_,
                   *ground_, aspect_ratio, viewport_width, viewport_height, fog);
  // ⚠ LA SECOUSSE SE DÉPOSE ET SE JETTE : la caméra du décor tourne sur son
  // orbite, et rien ne la fait trembler. La laisser en attente la ferait
  // trembler en partie, à la première image.
  static_cast<void>(effects_.TakeScreenQuake());
  const std::vector<EffectsInPlay::ActorEffectToRecord> taken = effects_.TakeActorEffects();
  to_record_.insert(to_record_.end(), taken.begin(), taken.end());
}

void BackdropEffects::Settle(ro::sim::World& world) {
  SettleActorEffects(world, std::exchange(to_record_, {}));
}

void BackdropEffects::RaiseSky(const MapStage& stage, glm::vec3 anchor) {
  if (sky_raised_) {
    return;
  }
  sky_raised_ = true;
  // L'ANCRE EST LE POINT VISÉ, et rien ne la porte : les particules du ciel
  // renaissent dans une boîte autour d'elle, et la caméra du décor tourne
  // autour de ce même point.
  for (const std::uint32_t effect_id : stage.scene.sky_effects) {
    nodes_.PlayProcedural(effect_id, anchor);
  }
  // ⚠ LA MÉTÉO SUIT `/effect`, comme dans la partie : coupé, elle ne se lève pas.
  if (effects_.EffectsShown()) {
    for (const std::uint32_t effect_id : stage.scene.weather_effects) {
      nodes_.PlayProcedural(effect_id, anchor);
    }
  }
}

void BackdropEffects::AdvanceMapEffects(MapStage& stage, const SoundStage& scene,
                                        ActorSounds& sounds, glm::vec3 anchor,
                                        std::vector<ro::formats::RswEffect> map_objects) {
  // LES OBJETS D'EFFET, comme en partie : la liste qui change — la nuit qui
  // tombe ou se lève — se relève.
  if (!map_raised_ || !ro::render::SameMapEffects(map_objects, raised_objects_)) {
    map_effects_.Raise(stage.scene.effect_table, map_objects, stage.scene.gnd.width(),
                       stage.scene.gnd.height());
    raised_objects_ = std::move(map_objects);
  }
  if (!map_raised_) {
    // LES SOURCES DE SON DE LA CARTE, dans la même passe, comme la partie.
    sounds.RaiseMapAmbience(
        stage.scene.rsw.sounds(),
        ro::render::MapEffectFrame{stage.scene.gnd.width(), stage.scene.gnd.height()},
        scene.now_ms);
    map_raised_ = true;
  }
  for (const auto& [place, owner] : effects_.TakeChimneys()) {
    if (!stage.scene.chimney_smoke.has_value()) {
      stage.scene.chimney_smoke = ro::render::LoadChimneySmoke(*vfs_);
    }
    map_effects_.AddChimney(place, owner);
  }
  map_effects_.FollowOwners([&stage, &scene](std::uint32_t owner) -> std::optional<glm::vec3> {
    const auto found = scene.world->actors().find(owner);
    if (found == scene.world->actors().end()) {
      return std::nullopt;
    }
    const ro::sim::FractionalCell cell = ro::sim::InterpolatedCell(
        found->second, scene.planner->PathFor(found->second), scene.now_ms);
    const std::array<float, 3> ground = GroundPositionAt(stage, cell.x, cell.y);
    return glm::vec3(ground[0], ground[1], ground[2]);
  });
  map_effects_.Advance(scene.now_ms, anchor);
  for (const ro::render::MapEffectSowing& sowing : map_effects_.sown()) {
    effects_.PlayById(static_cast<std::uint32_t>(sowing.effect_id), sowing.world_position,
                      scene.now_ms, 0, false, 0, true, 0, sowing.instance_scale,
                      sowing.instance_second);
  }
}

}  // namespace moonclient
