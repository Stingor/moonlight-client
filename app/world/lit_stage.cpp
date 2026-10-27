#include "world/lit_stage.h"

#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "ro/core/log.h"
#include "ro/render/actor_lights.h"
#include "ro/render/map_point_lights.h"
#include "ro/render/point_shadow_pass.h"
#include "ro/render/sky_body.h"
#include "ro/vfs/indoor_maps.h"

#include "input/camera_rules.h"
#include "world/closed_maps.h"
#include "world/scene_graphics.h"

namespace moonclient {

ro::core::Result<void> LitStage::Open(ro::vfs::Vfs& vfs, std::string_view map_name,
                                      const MapStageRead& read) {
  auto loaded = LoadMapStage(vfs, map_name, read);
  if (!loaded) {
    return std::unexpected(loaded.error());
  }
  document_ = read.edits != nullptr ? read.edits->Document(map_name) : ro::mapedit::MapEdit();
  model_book_ = read.edits != nullptr ? read.edits->Document(ro::mapedit::kModelLightsDocument)
                                      : ro::mapedit::MapEdit();
  return Mount(vfs, std::move(*loaded), map_name, read.progress);
}

ro::core::Result<void> LitStage::Open(ro::vfs::Vfs& vfs, MapStage stage,
                                      std::string_view sky_map_name,
                                      ro::core::LoadProgress* progress) {
  document_ = ro::mapedit::MapEdit();
  model_book_ = ro::mapedit::MapEdit();
  return Mount(vfs, std::move(stage), sky_map_name, progress);
}

ro::core::Result<void> LitStage::Mount(ro::vfs::Vfs& vfs, MapStage stage,
                                       std::string_view sky_map_name,
                                       ro::core::LoadProgress* progress) {
  // Une carte préparée pour la précédente ne vaut rien pour celle-ci.
  static_cast<void>(DropPrepared());
  stage_ = std::move(stage);
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
  if (auto uploaded = UploadStage(*stage_, terrain_, water_, models_, progress); !uploaded) {
    return std::unexpected(uploaded.error());
  }
  model_clock_ = ro::render::ModelClock();

  // ⚠ MONTÉ MÊME SANS PERSONNE À PEINDRE : c'est un programme de nuanceur, pas
  // une texture, et il ne coûte rien tant qu'on ne lui soumet aucun quad. Le
  // monter à la demande voudrait dire le monter au milieu d'une image, à
  // l'instant précis où la carte se peuple.
  auto sprites = ro::render::SpriteRenderer::Create();
  if (!sprites) {
    return std::unexpected(sprites.error());
  }
  sprites_ = std::move(*sprites);
  // LE CIEL DE LA CARTE, par les mêmes tables que la partie. Une table
  // illisible laisse la carte en plein air.
  auto indoor = ro::vfs::ReadIndoorMaps(vfs);
  const bool indoor_map = MapCameraTables::Parse(indoor ? *indoor : std::vector<std::string>{}, {})
                              .RulesFor(sky_map_name)
                              .indoor;
  closed_to_sky_ = indoor_map || LoadClosedMaps(vfs).Closes(sky_map_name);
  return {};
}

ro::core::Result<MapStage> LitStage::Replace(MapStage stage) {
  if (auto prepared = Prepare(std::move(stage)); !prepared) {
    return std::unexpected(prepared.error());
  }
  return MountPrepared();
}

ro::core::Result<void> LitStage::Prepare(MapStage stage) {
  if (!spare_terrain_.has_value()) {
    auto created = ro::render::TerrainRenderer::Create();
    if (!created) return std::unexpected(created.error());
    spare_terrain_ = std::move(*created);
  }
  if (!spare_models_.has_value()) {
    auto created = ro::render::ModelRenderer::Create();
    if (!created) return std::unexpected(created.error());
    spare_models_ = std::move(*created);
  }
  spare_models_ready_ = false;
  prepared_ = std::move(stage);
  // Le compte du sol de réserve passe au-delà de celui du sol montré : qui
  // garde l'ombre du sol le saura changé à l'échange.
  return spare_terrain_->BeginUpload(prepared_->scene.terrain, prepared_->scene.terrain_textures,
                                     prepared_->scene.atlas.image, terrain_.uploads());
}

bool LitStage::PrepareStep(std::size_t most_bytes) {
  if (!prepared_.has_value() || spare_models_ready_) return true;
  if (!spare_terrain_->UploadSlice(most_bytes)) return false;
  // LE DÉCOR APRÈS LE SOL, à une image à lui : il ne se coupe pas.
  spare_models_->SetTextureFilter(decor_filter_);
  if (auto uploaded = spare_models_->Upload(prepared_->scene.models, nullptr, models_.uploads());
      !uploaded) {
    ro::core::LogWarning("scène éclairée : décor préparé refusé — {}",
                         ro::core::Describe(uploaded.error()));
  }
  spare_models_ready_ = true;
  return false;
}

ro::core::Result<MapStage> LitStage::MountPrepared() {
  // Le reste du sol, s'il en reste : le renderer de réserve lit encore la carte
  // préparée tant qu'il n'a pas fini.
  while (!PrepareStep(std::numeric_limits<std::size_t>::max())) {
  }
  MapStage outgoing = std::exchange(*stage_, std::move(*prepared_));
  prepared_.reset();
  std::swap(terrain_, *spare_terrain_);
  std::swap(models_, *spare_models_);
  spare_models_ready_ = false;
  background_ = BackgroundOf(*stage_);
  model_clock_.Rebase(outgoing.scene.models, stage_->scene.models);
  if (auto uploaded = water_.Upload(stage_->scene.water, stage_->scene.water_animations);
      !uploaded) {
    return std::unexpected(uploaded.error());
  }
  return outgoing;
}

std::optional<MapStage> LitStage::DropPrepared() {
  if (!prepared_.has_value()) return std::nullopt;
  static_cast<void>(spare_terrain_->UploadSlice(std::numeric_limits<std::size_t>::max()));
  std::optional<MapStage> dropped = std::move(prepared_);
  prepared_.reset();
  spare_models_ready_ = false;
  return dropped;
}

LitFrame LitStage::Begin(const GraphicsOptions& graphics, bool fog_shown,
                         std::uint32_t elapsed_ms) {
  // LE SOL ARRONDI comme en partie. Un refus de la carte graphique laisse le sol
  // d'avant : la scène reste debout.
  if (auto rounded = SyncStageGround(*stage_, terrain_, graphics.rounded_ground()); !rounded) {
    ro::core::LogWarning("scène éclairée : sol non rebâti — {}",
                         ro::core::Describe(rounded.error()));
  }
  LitFrame frame;
  frame.fog_shown = fog_shown;
  frame.fog = FogOf(*stage_, fog_shown);
  frame.stages = TerrainStagesOf(graphics, /*lightmap_hidden=*/false);
  frame.sun_shadows = !sun_map_refused_ && SunShadowsApply(graphics, frame.stages, closed_to_sky_);
  // Les modèles animés posés AVANT l'ombre du soleil, qui les peint dans la
  // même pose que l'image.
  if (!model_clock_.started()) {
    model_clock_.Start(stage_->scene.models, elapsed_ms);
  }
  model_clock_.Advance(stage_->scene.models, elapsed_ms, ModelClockBlendOf(graphics));
  return frame;
}

void LitStage::Paint(LitFrame& frame, const ro::render::Camera& camera, float aspect_ratio,
                     const GraphicsOptions& graphics, std::optional<float> sky_hour,
                     std::uint32_t elapsed_ms, std::uint32_t now_ms, const LitActors& actors,
                     LitLayers& layers) {
  frame.sky = SkyOf(graphics, sky_hour);
  frame.sun = DrawSunShadow(frame.sun_shadows, camera, aspect_ratio, frame.sky,
                            actors.sun_casters.value_or(actors.bodies));
  if (frame.sun.map != nullptr) {
    frame.stages.shadowmap = false;
  }
  frame.points = DrawPointLights(graphics, camera, aspect_ratio, frame.sky, now_ms, actors);
  // ⚠ L'HEURE N'ASSOMBRIT LES CORPS QUE SOUS UN ASTRE, comme en partie.
  frame.body_daylight = frame.sky.body_shown ? frame.sky.light.daylight : glm::vec3(1.0F);

  terrain_.Draw(camera, frame.sky.light, aspect_ratio,
                ClearColorOf(*stage_, background_, frame.fog_shown), frame.stages, frame.fog,
                frame.sun, frame.points);
  decor_filter_ = graphics.decor_filter();
  models_.SetTextureFilter(decor_filter_);
  // Les faces à texture mêlée attendent la passe translucide, après les corps.
  models_.Draw(camera, frame.sky.light, aspect_ratio,
               ro::render::ModelStages{.blended_apart = true}, &model_clock_, frame.fog, nullptr,
               frame.sun, frame.points);
  layers.DrawBehindBodies(camera, aspect_ratio, frame);

  using ro::render::SpriteRenderer;
  // 🔴 LES CORPS SEULS ÉCRIVENT LEUR PROFONDEUR, comme dans la partie, et
  // laissent leur silhouette à l'eau et aux halos, qui se peignent après eux.
  // L'ombre du soleil, l'heure et les lumières ponctuelles n'éclairent qu'eux.
  sprites_.Draw(camera, actors.bodies, aspect_ratio, SpriteRenderer::Blending::kOver,
                SpriteRenderer::DepthWrite::kOn, SpriteRenderer::DepthTest::kOn, frame.fog,
                SpriteRenderer::DrawOrder::kFarthestFirst, SpriteRenderer::BodySilhouette::kLeft,
                frame.sun, frame.body_daylight, frame.points);
  // L'eau APRÈS les corps, comme dans la partie : elle plonge ce qui est sous sa
  // surface. Les effets de la carte passent dessus.
  water_.Draw(camera, aspect_ratio, elapsed_ms, frame.fog,
              frame.sky.light.tint * frame.sky.light.daylight);
  layers.DrawOverWater(camera, aspect_ratio, frame);
  // Les halos sur l'eau mais derrière tout corps, puis les voiles.
  sprites_.Draw(camera, actors.halos, aspect_ratio, SpriteRenderer::Blending::kOver,
                SpriteRenderer::DepthWrite::kOff, SpriteRenderer::DepthTest::kOn, frame.fog,
                SpriteRenderer::DrawOrder::kFarthestFirst,
                SpriteRenderer::BodySilhouette::kAvoided);
  sprites_.Draw(camera, actors.veils, aspect_ratio, SpriteRenderer::Blending::kOver,
                SpriteRenderer::DepthWrite::kOff, SpriteRenderer::DepthTest::kOn, frame.fog);
  // LE VIDE SOUS LA CARTE prend l'heure du ciel, avant les faces mêlées.
  ShadeVoid(frame.sky, camera, aspect_ratio);
  // LA PASSE TRANSLUCIDE DU DÉCOR, sans pose voilée : ses faces mêlées.
  models_.DrawVeiled(camera, frame.sky.light, aspect_ratio, stage_->scene.models, {}, &model_clock_,
                     frame.fog, frame.points);
}

ro::render::SkyLight LitStage::SkyOf(const GraphicsOptions& graphics,
                                     std::optional<float> sky_hour) const {
  if ((graphics.day_night_cycle() || sky_hour_imposed_) && !closed_to_sky_ &&
      sky_hour.has_value()) {
    return ro::render::SkyLightAt(stage_->scene.light, *sky_hour);
  }
  return ro::render::SkyLight{.light = stage_->scene.light};
}

ro::render::SunShadowCasters LitStage::ShadowCasters(
    std::span<const ro::render::DrawnActor> bodies) {
  return ro::render::SunShadowCasters{.terrain = &terrain_,
                                      .models = &models_,
                                      .clock = &model_clock_,
                                      .sprites = &sprites_,
                                      .actor_lots = {bodies, {}}};
}

ro::render::SunShadowView LitStage::DrawSunShadow(bool asked, const ro::render::Camera& camera,
                                                  float aspect_ratio,
                                                  const ro::render::SkyLight& sky,
                                                  std::span<const ro::render::DrawnActor> bodies) {
  if (!asked) {
    return ro::render::SunShadowView();
  }
  if (!sun_map_.valid()) {
    auto created = ro::render::SunShadowMap::Create(ro::render::kSunShadowMapTexels);
    if (!created) {
      sun_map_refused_ = true;
      ro::core::LogWarning("scène éclairée : ombre du soleil refusée — {}",
                           ro::core::Describe(created.error()));
      return ro::render::SunShadowView();
    }
    sun_map_ = std::move(*created);
  }
  // L'ASTRE DU CIEL porte l'ombre, et la lune la porte plus légère.
  return ro::render::DrawSunShadow(sun_map_, ShadowCasters(bodies), stage_->scene.terrain,
                                   sky.CastingLight(), camera, aspect_ratio,
                                   ro::render::kFullSunShadow * sky.shadow_strength);
}

ro::render::PointLightSet LitStage::DrawPointLights(const GraphicsOptions& graphics,
                                                    const ro::render::Camera& camera,
                                                    float aspect_ratio,
                                                    const ro::render::SkyLight& sky,
                                                    std::uint32_t now_ms, const LitActors& actors) {
  // LES LUMIÈRES DE LA CARTE, retouches comprises, comme en partie, puis
  // celles que portent les corps.
  const ro::render::PointLightSet points = ro::render::WithActorLights(
      ro::render::MapPointLights(
          ro::render::SceneLights(document_, model_book_, stage_->scene),
          ro::render::MapEffectFrame{stage_->scene.gnd.width(), stage_->scene.gnd.height()},
          sky.light, closed_to_sky_, camera.look_at, static_cast<double>(now_ms), sky.hour),
      actors.carried_lights, camera.look_at);
  if (points.lights.empty() || point_stage_refused_) {
    return ro::render::PointLightSet();
  }
  if (!point_stage_.valid()) {
    auto created = ro::render::PointLightStage::Create();
    if (!created) {
      point_stage_refused_ = true;
      ro::core::LogWarning("scène éclairée : lumières ponctuelles refusées — {}",
                           ro::core::Describe(created.error()));
      return ro::render::PointLightSet();
    }
    point_stage_ = std::move(*created);
  }
  // SANS OMBRE, les lumières éclairent quand même.
  const ro::render::SunShadowCasters casters = ShadowCasters(actors.bodies);
  return ro::render::DrawPointShadows(point_stage_,
                                      graphics.real_time_shadows() ? &casters : nullptr, points,
                                      camera, aspect_ratio);
}

void LitStage::ShadeVoid(const ro::render::SkyLight& sky, const ro::render::Camera& camera,
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

std::string_view LitStage::map_name() const noexcept {
  return stage_.has_value() ? std::string_view(stage_->name) : std::string_view();
}

}  // namespace moonclient
