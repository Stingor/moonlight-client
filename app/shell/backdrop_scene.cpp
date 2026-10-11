#include "shell/backdrop_scene.h"

#include <array>
#include <span>
#include <utility>

#include "ro/core/log.h"
#include "ro/render/terrain_mesh.h"

#include "shell/backdrop_camera.h"
#include "world/scene_graphics.h"

namespace moonclient {

ro::core::Result<void> BackdropScene::Open(ro::vfs::Vfs& vfs, std::string_view map_name) {
  auto loaded = LoadMapStage(vfs, map_name);
  if (!loaded) {
    return std::unexpected(loaded.error());
  }
  stage_ = std::move(*loaded);
  background_ = BackgroundOf(*stage_);

  auto terrain = ro::render::TerrainRenderer::Create();
  if (!terrain) {
    return std::unexpected(terrain.error());
  }
  terrain_ = std::move(*terrain);
  if (auto uploaded = terrain_.Upload(stage_->scene.terrain, stage_->scene.terrain_textures,
                                      stage_->scene.atlas.image);
      !uploaded) {
    return std::unexpected(uploaded.error());
  }

  auto water = ro::render::WaterRenderer::Create();
  if (!water) {
    return std::unexpected(water.error());
  }
  water_ = std::move(*water);
  if (auto uploaded = water_.Upload(stage_->scene.water, stage_->scene.water_animations);
      !uploaded) {
    return std::unexpected(uploaded.error());
  }

  auto models = ro::render::ModelRenderer::Create();
  if (!models) {
    return std::unexpected(models.error());
  }
  models_ = std::move(*models);
  if (auto uploaded = models_.Upload(stage_->scene.models); !uploaded) {
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
                         const GraphicsOptions& graphics) {
  if (!stage_.has_value()) {
    return;
  }
  const ro::render::Camera camera = BackdropCamera(LookAt(), elapsed_ms);
  // LES ÉTAGES QUE LE JOUEUR RÈGLE, et eux seuls : le sol uni de l'établi du
  // staff ne passe pas par ici. Voir l'en-tête.
  //
  // 🔴 SANS BROUILLARD, quel que soit le réglage : voir `kBackdropFogShown`.
  const ro::render::SceneFog fog = FogOf(*stage_, kBackdropFogShown);
  terrain_.Draw(camera, stage_->scene.light, aspect_ratio,
                ClearColorOf(*stage_, background_, kBackdropFogShown),
                TerrainStagesOf(graphics, /*lightmap_hidden=*/false), fog);
  if (!model_clock_.started()) {
    model_clock_.Start(stage_->scene.models, elapsed_ms);
  }
  model_clock_.Advance(stage_->scene.models, elapsed_ms);
  models_.SetTextureFilter(graphics.decor_filter());
  // Les faces à texture mêlée attendent la passe translucide, après les passants.
  models_.Draw(camera, stage_->scene.light, aspect_ratio,
               ro::render::ModelStages{.blended_apart = true}, &model_clock_, fog);
  // LES EFFETS QUI PASSENT DERRIÈRE LES CORPS, avant eux.
  if (effects_.has_value()) {
    effects_->DrawBehindActors(camera, aspect_ratio, fog, sprites_);
  }

  actors_.Clear();
  // 🔴 PERSONNE NE SE PEINT SUR UNE AUTRE VILLE QUE LA SIENNE : voir l'en-tête.
  const bool peopled =
      wardrobe != nullptr && world != nullptr && !world_map.empty() && world_map == map_name();
  if (peopled) {
    DrawPassersby(camera, aspect_ratio, elapsed_ms, *wardrobe, *world, now_ms, audio, fog,
                  graphics);
  }
  // L'eau APRÈS les corps des passants, comme dans la partie : elle plonge ce qui
  // est sous sa surface. C'est `elapsed_ms` qui l'anime, la même horloge que
  // l'orbite de la caméra. Les effets de la carte passent dessus.
  water_.Draw(camera, aspect_ratio, elapsed_ms, fog);
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
  // LA PASSE TRANSLUCIDE DU DÉCOR, sans pose voilée : ses faces mêlées.
  models_.DrawVeiled(camera, stage_->scene.light, aspect_ratio, stage_->scene.models, {},
                     &model_clock_, fog);

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
                         &effects_->in_play()};
  if (audio->sound_audible()) {
    sounds_->HearMapAmbience(scene);
  }
  effects_->DrawAfterActors(*stage_, scene, *sounds_, drops_, sprites_, LookAt(), aspect_ratio,
                            viewport_width, viewport_height, fog);
}

void BackdropScene::DrawPassersby(const ro::render::Camera& camera, float aspect_ratio,
                                  std::uint32_t elapsed_ms, ro::render::ActorWardrobe& wardrobe,
                                  const ro::sim::World& world, std::uint32_t now_ms,
                                  ro::audio::AudioEngine* audio, const ro::render::SceneFog& fog,
                                  const GraphicsOptions& graphics) {
  // ⚠ LA MÊME PORTE QUE LA PARTIE, et sans styles : les recettes de couleur
  // arrivent par des paquets de JEU, qu'une session spectateur ne reçoit pas.
  // Fondus, geste lisible et lumière de la carte suivent la page « Graphics »,
  // comme en partie ; les altérations n'ont pas de teinte ici.
  TimeComposedScene(graphics, actors_);
  static_cast<void>(ComposeScene(&wardrobe, world, planner_, *stage_, camera.yaw_degrees, now_ms,
                                 static_cast<float>(elapsed_ms),
                                 /*styles=*/nullptr, actors_, ghosts_, drops_, frozen_poses_,
                                 ActorLightingOf(graphics, &stage_->ground_light,
                                                 /*ailment_tints=*/nullptr)));
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
  // 🔴 LES CORPS SEULS ÉCRIVENT LEUR PROFONDEUR, comme dans la partie, et
  // laissent leur silhouette à l'eau et aux halos, qui se peignent après eux :
  // voir `Draw`.
  sprites_.Draw(camera, actors_.drawn, aspect_ratio, ro::render::SpriteRenderer::Blending::kOver,
                ro::render::SpriteRenderer::DepthWrite::kOn,
                ro::render::SpriteRenderer::DepthTest::kOn, fog,
                ro::render::SpriteRenderer::DrawOrder::kFarthestFirst,
                ro::render::SpriteRenderer::BodySilhouette::kLeft);
}

std::string_view BackdropScene::map_name() const noexcept {
  return stage_.has_value() ? std::string_view(stage_->name) : std::string_view();
}

}  // namespace moonclient
