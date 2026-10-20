#include "shell/backdrop_scene.h"

#include <array>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "ro/core/log.h"
#include "ro/mapedit/map_edit.h"
#include "ro/render/map_point_lights.h"
#include "ro/render/point_shadow_pass.h"
#include "ro/render/sky_body.h"
#include "ro/render/terrain_mesh.h"
#include "ro/vfs/indoor_maps.h"

#include "input/camera_rules.h"
#include "shell/backdrop_camera.h"
#include "world/closed_maps.h"
#include "world/scene_graphics.h"

namespace moonclient {

ro::core::Result<void> BackdropScene::Open(ro::vfs::Vfs& vfs, std::string_view map_name,
                                           const MapStageRead& read) {
  auto loaded = LoadMapStage(vfs, map_name, read);
  if (!loaded) {
    return std::unexpected(loaded.error());
  }
  stage_ = std::move(*loaded);
  document_ = read.edits != nullptr ? read.edits->Document(map_name) : ro::mapedit::MapEdit();
  model_book_ = read.edits != nullptr ? read.edits->Document(ro::mapedit::kModelLightsDocument)
                                      : ro::mapedit::MapEdit();
  background_ = BackgroundOf(*stage_);

  auto terrain = ro::render::TerrainRenderer::Create();
  if (!terrain) {
    return std::unexpected(terrain.error());
  }
  terrain_ = std::move(*terrain);
  auto water = ro::render::WaterRenderer::Create();
  if (!water) {
    return std::unexpected(water.error());
  }
  water_ = std::move(*water);
  auto models = ro::render::ModelRenderer::Create();
  if (!models) {
    return std::unexpected(models.error());
  }
  models_ = std::move(*models);
  if (auto uploaded = UploadStage(*stage_, terrain_, water_, models_, read.progress); !uploaded) {
    return std::unexpected(uploaded.error());
  }
  model_clock_ = ro::render::ModelClock();

  // ⚠ MONTÉ MÊME POUR LE REPLI LOCAL, qui n'a personne à peindre : c'est un
  // programme de nuanceur, pas une texture, et il ne coûte rien tant qu'on ne
  // lui soumet aucun quad. Le monter à la demande voudrait dire le monter au
  // milieu d'une image, à l'instant précis où la ville se peuple.
  auto sprites = ro::render::SpriteRenderer::Create();
  if (!sprites) {
    return std::unexpected(sprites.error());
  }
  sprites_ = std::move(*sprites);
  planner_ = ro::sim::WalkPlanner(PlannedMapOf(*stage_));
  // ⚠ REFAITS AVEC LA CARTE, comme le planificateur, et pour la même raison :
  // ils retiennent ce que chaque acteur a déjà fait entendre, par identifiant.
  // Une autre ville, d'autres gens — et des identifiants qui ne désignent plus
  // les mêmes.
  sounds_.emplace(vfs, kSoundDrawSeed);
  effects_.reset();
  effects_.emplace(vfs);
  told_drawn_ = false;
  // LE CIEL DE LA VILLE, par les mêmes tables que la partie. Une table illisible
  // laisse la ville en plein air : c'est ce que sont les villes.
  auto indoor = ro::vfs::ReadIndoorMaps(vfs);
  const bool indoor_map = MapCameraTables::Parse(indoor ? *indoor : std::vector<std::string>{}, {})
                              .RulesFor(stage_->name)
                              .indoor;
  closed_to_sky_ = indoor_map || LoadClosedMaps(vfs).Closes(stage_->name);

  ro::core::LogInfo("décor : {} montée", stage_->name);
  return {};
}

void BackdropScene::WatchCell(ro::sim::CellPosition where) { watched_ = where; }

glm::vec3 BackdropScene::LookAt() const {
  // ⚠ LA GRILLE DU DÉCOR COMPTE EN DEMI-CASES : le terrain est décrit deux fois
  // plus grossièrement que la marchabilité, si bien que sa largeur EST l'abscisse
  // du centre en cellules.
  const std::int32_t center_x = stage_->scene.gnd.width();
  const std::int32_t center_y = stage_->scene.gnd.height();
  const std::int32_t look_x = watched_.has_value() ? watched_->x : center_x;
  const std::int32_t look_y = watched_.has_value() ? watched_->y : center_y;
  const std::array<float, 3> ground =
      ro::render::CellCenterPosition(stage_->scene.gnd, look_x, look_y);
  return glm::vec3(ground[0], ground[1], ground[2]);
}

void BackdropScene::Settle(ro::sim::World& world, std::uint32_t now_ms) {
  SettleWorld(world, actors_, now_ms);
  if (effects_.has_value()) {
    effects_->Settle(world);
  }
}

void BackdropScene::Draw(float aspect_ratio, std::uint32_t elapsed_ms,
                         ro::render::ActorWardrobe* wardrobe, const ro::sim::World* world,
                         std::string_view world_map, std::uint32_t now_ms,
                         ro::audio::AudioEngine* audio, float viewport_width, float viewport_height,
                         const GraphicsOptions& graphics, std::optional<float> sky_hour) {
  if (!stage_.has_value()) {
    return;
  }
  // LE SOL ARRONDI comme en partie. Un refus de la carte graphique laisse le sol
  // d'avant : le fond reste debout.
  if (auto rounded = SyncStageGround(*stage_, terrain_, graphics.rounded_ground()); !rounded) {
    ro::core::LogWarning("fond : sol non rebâti — {}", ro::core::Describe(rounded.error()));
  }
  const ro::render::Camera camera = BackdropCamera(LookAt(), elapsed_ms);
  // LES ÉTAGES QUE LE JOUEUR RÈGLE, et eux seuls : le sol uni de l'établi du
  // staff ne passe pas par ici. Voir l'en-tête.
  //
  // 🔴 SANS BROUILLARD, quel que soit le réglage : voir `kBackdropFogShown`.
  const ro::render::SceneFog fog = FogOf(*stage_, kBackdropFogShown);
  ro::render::TerrainStages stages = TerrainStagesOf(graphics, /*lightmap_hidden=*/false);
  const bool sun_asked = !sun_map_refused_ && SunShadowsApply(graphics, stages, closed_to_sky_);
  // Les modèles animés posés AVANT l'ombre du soleil, qui les peint dans la
  // même pose que l'image.
  if (!model_clock_.started()) {
    model_clock_.Start(stage_->scene.models, elapsed_ms);
  }
  model_clock_.Advance(stage_->scene.models, elapsed_ms);

  actors_.Clear();
  // 🔴 PERSONNE NE SE PEINT SUR UNE AUTRE VILLE QUE LA SIENNE : voir l'en-tête.
  const bool peopled =
      wardrobe != nullptr && world != nullptr && !world_map.empty() && world_map == map_name();
  if (peopled) {
    ComposePassersby(camera, elapsed_ms, *wardrobe, *world, now_ms, audio, graphics, sun_asked);
  }

  FrameLight lit;
  lit.sky = SkyOf(graphics, sky_hour);
  lit.sun = DrawSunShadow(sun_asked, camera, aspect_ratio, lit.sky);
  if (lit.sun.map != nullptr) {
    stages.shadowmap = false;
  }
  lit.points = DrawPointLights(graphics, camera, aspect_ratio, lit.sky, now_ms);
  // ⚠ L'HEURE N'ASSOMBRIT LES CORPS QUE SOUS UN ASTRE, comme en partie.
  lit.body_daylight = lit.sky.body_shown ? lit.sky.light.daylight : glm::vec3(1.0F);

  terrain_.Draw(camera, lit.sky.light, aspect_ratio,
                ClearColorOf(*stage_, background_, kBackdropFogShown), stages, fog, lit.sun,
                lit.points);
  models_.SetTextureFilter(graphics.decor_filter());
  // Les faces à texture mêlée attendent la passe translucide, après les passants.
  models_.Draw(camera, lit.sky.light, aspect_ratio, ro::render::ModelStages{.blended_apart = true},
               &model_clock_, fog, nullptr, lit.sun, lit.points);
  // LES EFFETS QUI PASSENT DERRIÈRE LES CORPS, avant eux.
  if (effects_.has_value()) {
    effects_->DrawBehindActors(camera, aspect_ratio, fog, sprites_);
  }
  if (peopled) {
    DrawPassersby(camera, aspect_ratio, fog, lit);
  }
  // L'eau APRÈS les corps des passants, comme dans la partie : elle plonge ce qui
  // est sous sa surface. C'est `elapsed_ms` qui l'anime, la même horloge que
  // l'orbite de la caméra. Les effets de la carte passent dessus.
  water_.Draw(camera, aspect_ratio, elapsed_ms, fog, lit.sky.light.tint * lit.sky.light.daylight);
  if (effects_.has_value()) {
    effects_->DrawMapEffects(*stage_, camera, aspect_ratio, fog);
  }
  // Les halos sur l'eau mais derrière tout corps, puis les voiles.
  using ro::render::SpriteRenderer;
  sprites_.Draw(camera, actors_.behind, aspect_ratio, SpriteRenderer::Blending::kOver,
                SpriteRenderer::DepthWrite::kOff, SpriteRenderer::DepthTest::kOn, fog,
                SpriteRenderer::DrawOrder::kFarthestFirst,
                SpriteRenderer::BodySilhouette::kAvoided);
  sprites_.Draw(camera, actors_.front, aspect_ratio, SpriteRenderer::Blending::kOver,
                SpriteRenderer::DepthWrite::kOff, SpriteRenderer::DepthTest::kOn, fog);
  // LE VIDE SOUS LA VILLE prend l'heure du ciel, avant les faces mêlées.
  ShadeVoid(lit.sky, camera, aspect_ratio);
  // LA PASSE TRANSLUCIDE DU DÉCOR, sans pose voilée : ses faces mêlées.
  models_.DrawVeiled(camera, lit.sky.light, aspect_ratio, stage_->scene.models, {}, &model_clock_,
                     fog, lit.points);

  // LES EFFETS APRÈS LES PASSANTS, comme dans la partie : ceux de la ville
  // toujours, ceux des passants quand ils sont sur cette carte.
  if (!effects_.has_value() || audio == nullptr || !sounds_.has_value()) {
    return;
  }
  const SoundStage scene{audio,
                         wardrobe != nullptr ? &wardrobe->catalog() : nullptr,
                         &camera,
                         &*stage_,
                         peopled ? world : &empty_world_,
                         &planner_,
                         now_ms,
                         peopled ? wardrobe : nullptr,
                         &effects_->in_play(),
                         {},
                         peopled ? &actors_.head_shifts : nullptr,
                         peopled ? &actors_.hover_pixels : nullptr};
  if (audio->sound_audible()) {
    sounds_->HearMapAmbience(scene);
  }
  effects_->DrawAfterActors(*stage_, scene, *sounds_, drops_, sprites_, LookAt(), aspect_ratio,
                            viewport_width, viewport_height, fog);
}

void BackdropScene::ComposePassersby(const ro::render::Camera& camera, std::uint32_t elapsed_ms,
                                     ro::render::ActorWardrobe& wardrobe,
                                     const ro::sim::World& world, std::uint32_t now_ms,
                                     ro::audio::AudioEngine* audio, const GraphicsOptions& graphics,
                                     bool sun_shadows) {
  // ⚠ LA MÊME PORTE QUE LA PARTIE, et sans styles : les recettes de couleur
  // arrivent par des paquets de JEU, qu'une session spectateur ne reçoit pas.
  // Fondus, geste lisible et lumière de la carte suivent la page « Graphics »,
  // comme en partie ; les altérations n'ont pas de teinte ici.
  TimeComposedScene(graphics, actors_);
  static_cast<void>(ComposeScene(
      &wardrobe, world, planner_, *stage_, camera.yaw_degrees, now_ms,
      static_cast<float>(elapsed_ms),
      /*styles=*/nullptr, actors_, ghosts_, drops_, frozen_poses_,
      ActorLightingOf(graphics, sun_shadows ? &stage_->ground_laid_light : &stage_->ground_light,
                      /*ailment_tints=*/nullptr)));
  // SOUS LE SOLEIL, UN CORPS N'A PLUS D'OMBRE RONDE : sa silhouette en porte une.
  if (sun_shadows) {
    for (ro::render::DrawnActor& drawn : actors_.drawn) {
      if (drawn.actor != nullptr) {
        drawn.shadow = nullptr;
      }
    }
  }
  // CE QUE LES PASSANTS FONT ENTENDRE À CETTE IMAGE.
  //
  // ⚠ APRÈS LA COMPOSITION, comme dans la partie : elle seule sait quelle image
  // chacun montre, et c'est l'image qui dit le bruit.
  if (audio != nullptr && audio->sound_audible() && sounds_.has_value()) {
    const SoundStage heard{audio,
                           &wardrobe.catalog(),
                           &camera,
                           &*stage_,
                           &world,
                           &planner_,
                           now_ms,
                           &wardrobe,
                           effects_.has_value() ? &effects_->in_play() : nullptr};
    sounds_->Hear(heard, actors_.drawn, actors_.ids);
  }

  ghosts_.Drawn(now_ms, actors_.behind);
  // 🔴 CE QUI EST SOUMIS, ET NON CE QUE LE MONDE CONTIENT. Un monde peuplé
  // dont personne ne sort composé est exactement ce à quoi ressemble un
  // chemin d'appel complet qui ne produit rien — sprites introuvables,
  // apparence illisible, tenue vide. Dit une fois par ville.
  if (!told_drawn_ && !actors_.drawn.empty()) {
    told_drawn_ = true;
    ro::core::LogInfo("décor : {} figure(s) peinte(s) sur {}", actors_.drawn.size(), stage_->name);
  }
}

void BackdropScene::DrawPassersby(const ro::render::Camera& camera, float aspect_ratio,
                                  const ro::render::SceneFog& fog, const FrameLight& lit) {
  // 🔴 LES CORPS SEULS ÉCRIVENT LEUR PROFONDEUR, comme dans la partie, et
  // laissent leur silhouette à l'eau et aux halos, qui se peignent après eux :
  // voir `Draw`. L'ombre du soleil, l'heure et les lumières ponctuelles
  // n'éclairent qu'eux.
  sprites_.Draw(
      camera, actors_.drawn, aspect_ratio, ro::render::SpriteRenderer::Blending::kOver,
      ro::render::SpriteRenderer::DepthWrite::kOn, ro::render::SpriteRenderer::DepthTest::kOn, fog,
      ro::render::SpriteRenderer::DrawOrder::kFarthestFirst,
      ro::render::SpriteRenderer::BodySilhouette::kLeft, lit.sun, lit.body_daylight, lit.points);
}

ro::render::SkyLight BackdropScene::SkyOf(const GraphicsOptions& graphics,
                                          std::optional<float> sky_hour) const {
  if (graphics.day_night_cycle() && !closed_to_sky_ && sky_hour.has_value()) {
    return ro::render::SkyLightAt(stage_->scene.light, *sky_hour);
  }
  return ro::render::SkyLight{.light = stage_->scene.light};
}

ro::render::SunShadowCasters BackdropScene::ShadowCasters() {
  return ro::render::SunShadowCasters{.terrain = &terrain_,
                                      .models = &models_,
                                      .clock = &model_clock_,
                                      .sprites = &sprites_,
                                      .actor_lots = {actors_.drawn, {}}};
}

ro::render::SunShadowView BackdropScene::DrawSunShadow(bool asked, const ro::render::Camera& camera,
                                                       float aspect_ratio,
                                                       const ro::render::SkyLight& sky) {
  if (!asked) {
    return ro::render::SunShadowView();
  }
  if (!sun_map_.valid()) {
    auto created = ro::render::SunShadowMap::Create(ro::render::kSunShadowMapTexels);
    if (!created) {
      sun_map_refused_ = true;
      ro::core::LogWarning("décor : ombre du soleil refusée — {}",
                           ro::core::Describe(created.error()));
      return ro::render::SunShadowView();
    }
    sun_map_ = std::move(*created);
  }
  // L'ASTRE DU CIEL porte l'ombre, et la lune la porte plus légère.
  return ro::render::DrawSunShadow(sun_map_, ShadowCasters(), stage_->scene.terrain,
                                   sky.CastingLight(), camera, aspect_ratio,
                                   ro::render::kFullSunShadow * sky.shadow_strength);
}

ro::render::PointLightSet BackdropScene::DrawPointLights(const GraphicsOptions& graphics,
                                                         const ro::render::Camera& camera,
                                                         float aspect_ratio,
                                                         const ro::render::SkyLight& sky,
                                                         std::uint32_t now_ms) {
  // LES LUMIÈRES DE LA CARTE, retouches comprises, comme en partie.
  const ro::render::PointLightSet points = ro::render::MapPointLights(
      ro::render::SceneLights(document_, model_book_, stage_->scene),
      ro::render::MapEffectFrame{stage_->scene.gnd.width(), stage_->scene.gnd.height()}, sky.light,
      closed_to_sky_, camera.look_at, static_cast<double>(now_ms), sky.hour);
  if (points.lights.empty() || point_stage_refused_) {
    return ro::render::PointLightSet();
  }
  if (!point_stage_.valid()) {
    auto created = ro::render::PointLightStage::Create();
    if (!created) {
      point_stage_refused_ = true;
      ro::core::LogWarning("décor : lumières ponctuelles refusées — {}",
                           ro::core::Describe(created.error()));
      return ro::render::PointLightSet();
    }
    point_stage_ = std::move(*created);
  }
  // SANS OMBRE, les lumières éclairent quand même.
  const ro::render::SunShadowCasters casters = ShadowCasters();
  return ro::render::DrawPointShadows(point_stage_,
                                      graphics.real_time_shadows() ? &casters : nullptr, points,
                                      camera, aspect_ratio);
}

void BackdropScene::ShadeVoid(const ro::render::SkyLight& sky, const ro::render::Camera& camera,
                              float aspect_ratio) {
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
  if (sky.body_shown) {
    void_shade_.DrawBody(ro::render::PlaceSkyBody(sky.body_direction, camera, aspect_ratio,
                                                  sky.moon ? ro::render::kMoonAngularRadiusDegrees
                                                           : ro::render::kSunAngularRadiusDegrees),
                         sky.moon, aspect_ratio);
  }
}

std::string_view BackdropScene::map_name() const noexcept {
  return stage_.has_value() ? std::string_view(stage_->name) : std::string_view();
}

}  // namespace moonclient
