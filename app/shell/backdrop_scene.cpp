#include "shell/backdrop_scene.h"

#include <array>
#include <optional>
#include <span>
#include <utility>

#include "ro/core/log.h"
#include "ro/render/edited_map_effects.h"
#include "ro/render/terrain_mesh.h"

#include "shell/backdrop_camera.h"
#include "world/scene_graphics.h"

namespace moonclient {

ro::core::Result<void> BackdropScene::Open(ro::vfs::Vfs& vfs, std::string_view map_name,
                                           const MapStageRead& read) {
  if (auto opened = lit_.Open(vfs, map_name, read); !opened) {
    return std::unexpected(opened.error());
  }
  planner_ = ro::sim::WalkPlanner(PlannedMapOf(lit_.stage()));
  // ⚠ REFAITS AVEC LA CARTE, comme le planificateur, et pour la même raison :
  // ils retiennent ce que chaque acteur a déjà fait entendre, par identifiant.
  // Une autre ville, d'autres gens — et des identifiants qui ne désignent plus
  // les mêmes.
  sounds_.emplace(vfs, kSoundDrawSeed);
  effects_.reset();
  effects_.emplace(vfs);
  told_drawn_ = false;

  ro::core::LogInfo("décor : {} montée", lit_.stage().name);
  return {};
}

void BackdropScene::WatchCell(ro::sim::CellPosition where) { watched_ = where; }

glm::vec3 BackdropScene::LookAt() const {
  // ⚠ LA GRILLE DU DÉCOR COMPTE EN DEMI-CASES : le terrain est décrit deux fois
  // plus grossièrement que la marchabilité, si bien que sa largeur EST l'abscisse
  // du centre en cellules.
  const MapStage& stage = lit_.stage();
  const std::int32_t center_x = stage.scene.gnd.width();
  const std::int32_t center_y = stage.scene.gnd.height();
  const std::int32_t look_x = watched_.has_value() ? watched_->x : center_x;
  const std::int32_t look_y = watched_.has_value() ? watched_->y : center_y;
  const std::array<float, 3> ground =
      ro::render::CellCenterPosition(stage.scene.gnd, look_x, look_y);
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
  if (!lit_.ready()) {
    return;
  }
  // LES ÉTAGES QUE LE JOUEUR RÈGLE, et eux seuls : le sol uni de l'établi du
  // staff ne passe pas par ici. Voir l'en-tête.
  //
  // 🔴 SANS BROUILLARD, quel que soit le réglage : voir `kBackdropFogShown`.
  LitFrame frame = lit_.Begin(graphics, kBackdropFogShown, elapsed_ms);
  const ro::render::Camera camera = BackdropCamera(LookAt(), elapsed_ms);

  actors_.Clear();
  // 🔴 PERSONNE NE SE PEINT SUR UNE AUTRE VILLE QUE LA SIENNE : voir l'en-tête.
  const bool peopled =
      wardrobe != nullptr && world != nullptr && !world_map.empty() && world_map == map_name();
  if (peopled) {
    ComposePassersby(camera, elapsed_ms, *wardrobe, *world, now_ms, audio, graphics,
                     frame.sun_shadows);
  }
  lit_.Paint(frame, camera, aspect_ratio, graphics, sky_hour, elapsed_ms, now_ms,
             LitActors{.bodies = actors_.drawn, .halos = actors_.behind, .veils = actors_.front},
             *this);

  // LES EFFETS APRÈS LES PASSANTS, comme dans la partie : ceux de la ville
  // toujours, ceux des passants quand ils sont sur cette carte.
  if (!effects_.has_value() || audio == nullptr || !sounds_.has_value()) {
    return;
  }
  MapStage& stage = lit_.stage();
  const SoundStage scene{audio,
                         wardrobe != nullptr ? &wardrobe->catalog() : nullptr,
                         &camera,
                         &stage,
                         peopled ? world : &empty_world_,
                         &planner_,
                         now_ms,
                         peopled ? wardrobe : nullptr,
                         &effects_->in_play(),
                         {},
                         peopled ? &actors_.head_shifts : nullptr,
                         peopled ? &actors_.hover_pixels : nullptr,
                         ProjectileLooksOf(graphics)};
  if (audio->sound_audible()) {
    sounds_->HearMapAmbience(scene);
  }
  effects_->SetMapEffectReach(MapEffectReachOf(graphics));
  // LES OBJETS D'EFFET DE LA VILLE : ceux du RSW, de son document et de ses
  // modèles, comme en partie, à la nuit de cette image.
  effects_->DrawAfterActors(
      stage, scene, *sounds_, drops_, lit_.sprites(), LookAt(), aspect_ratio, viewport_width,
      viewport_height, frame.fog,
      ro::render::SceneMapEffects(stage.scene, lit_.document(), lit_.model_book(),
                                  ro::render::NightStrength(frame.sky.light, lit_.closed_to_sky()),
                                  frame.sky.hour));
}

void BackdropScene::DrawBehindBodies(const ro::render::Camera& camera, float aspect_ratio,
                                     const LitFrame& frame) {
  // LES EFFETS QUI PASSENT DERRIÈRE LES CORPS, avant eux.
  if (effects_.has_value()) {
    effects_->DrawBehindActors(camera, aspect_ratio, frame.fog, lit_.sprites());
  }
}

void BackdropScene::DrawOverWater(const ro::render::Camera& camera, float aspect_ratio,
                                  const LitFrame& frame) {
  if (effects_.has_value()) {
    effects_->DrawMapEffects(lit_.stage(), camera, aspect_ratio, frame.fog);
  }
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
      &wardrobe, world, planner_, lit_.stage(), camera.yaw_degrees, now_ms,
      static_cast<float>(elapsed_ms),
      /*styles=*/nullptr, actors_, ghosts_, drops_, frozen_poses_,
      ActorLightingOf(graphics,
                      sun_shadows ? &lit_.stage().ground_laid_light : &lit_.stage().ground_light,
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
                           &lit_.stage(),
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
    ro::core::LogInfo("décor : {} figure(s) peinte(s) sur {}", actors_.drawn.size(),
                      lit_.stage().name);
  }
}

}  // namespace moonclient
