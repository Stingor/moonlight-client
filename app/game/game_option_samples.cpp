// LES CADRES D'ESSAI DES OPTIONS, recomposés tant que leur page est ouverte :
// celui des entités, et celui des trajectoires de dégâts.

#include <cstddef>
#include <cstdint>
#include <utility>

#include <glm/glm.hpp>

#include "ro/render/actor_pick.h"
#include "ro/render/camera.h"
#include "ro/sim/movement.h"
#include "ro/ui/options_window.h"

#include "game/game.h"
#include "game/ui_stage.h"
#include "guild/sample_emblem.h"
#include "options/damage_page_names.h"
#include "options/entity_page_tabs.h"
#include "world/damage_path_sample.h"
#include "world/entity_sample.h"
#include "world/hit_popup_rules.h"

namespace moonclient {

void Game::ComposeOptionSamples(std::uint32_t now_ms) {
  // LES CADRES D'ESSAI, seulement quand leur page est sous les yeux : une
  // vignette animée se recompose à chaque image. Celui des entités, et celui des
  // dégâts — sur les onglets de l'allure, des trajectoires et des couleurs.
  const bool entity_frame =
      windows_.options->open && settings_->options().entity_bars().tuning &&
      windows_.options->section == static_cast<int>(Rank(OptionSection::kEntityBars));
  const bool path_frame =
      windows_.options->open &&
      windows_.options->section == static_cast<int>(Rank(OptionSection::kDamage)) &&
      windows_.options->section_tab != static_cast<int>(DamageTab::kShown);
  if (!entity_frame && !path_frame) return;
  const auto width = static_cast<float>(shell_.width());
  const auto height = static_cast<float>(shell_.height());
  const float anchor =
      ro::render::ScreenAnchorScale(height, ro::render::ApparentScale(camera_), 1.0F);
  // L'ÉCHELLE DU SPRITE SE MESURE SUR LE PERSONNAGE DU JOUEUR, tel que la
  // carte le dessine ; faute de lui, celle d'ancrage. Ses pieds servent de sol
  // au cadre des trajectoires.
  float sprite_scale = anchor;
  glm::vec3 player_ground = camera_.look_at;
  for (std::size_t rank = 0; rank < scene_actors_.ids.size() && rank < scene_actors_.drawn.size();
       ++rank) {
    if (scene_actors_.ids[rank] == world_.player_id()) {
      sprite_scale = ro::render::ActorSpriteScreenScale(camera_, aspect_ratio_,
                                                        scene_actors_.drawn[rank], width, height)
                         .value_or(anchor);
      player_ground = scene_actors_.drawn[rank].ground_position;
    }
  }
  if (path_frame) {
    const HitPopupLook& look = settings_->options().damage_look();
    const ro::sim::Actor* player = world_.player();
    DamagePathStage stage;
    stage.target = pieces_.entity_sampler->Compose(
        pieces_.wardrobe->has_value() ? &**pieces_.wardrobe : nullptr, &styles_,
        PathTargetShown(DamagePathOf(look.path_shown)), world_, sprite_scale, anchor, now_ms);
    stage.sprite_scale = sprite_scale;
    stage.ground = SampleGroundOf(
        camera_, aspect_ratio_, width, height, player_ground, sprite_scale,
        player != nullptr ? ClientBodyDegrees(*player, ro::sim::FacingAt(*player, now_ms)) : 0.0F);
    stage.font = interface_->DamageFont(look.font_name, look.font_bold);
    settings_->options().pages().damage_path_sample =
        pieces_.damage_path_sampler->Compose(pieces_.numbers, look, std::move(stage), now_ms);
  }
  if (entity_frame) {
    settings_->options().pages().entity_sample = pieces_.entity_sampler->Compose(
        pieces_.wardrobe->has_value() ? &**pieces_.wardrobe : nullptr, &styles_,
        EntityPageDrawn(EntityPageTabOf(windows_.options->section_tab)), world_, sprite_scale,
        anchor, now_ms);
    // L'EMBLÈME DE SA GUILDE OU DE SON CLAN : le cadre montre son personnage.
    ro::ui::EntitySample& sample = settings_->options().pages().entity_sample;
    sample.emblem = SampleEmblem(
        sample, world_, guild_book_, guild_emblems_.has_value() ? &*guild_emblems_ : nullptr,
        pieces_.game_images->has_value() ? &**pieces_.game_images : nullptr,
        settings_->options().entity_names());
  }
}

}  // namespace moonclient
