// GREYWORLD CÔTÉ PARTIE : le relief de la carte suit le réglage, et le sol
// arrondi aussi.

#include "game/game.h"
#include "world/grey_world_view.h"

namespace moonclient {

ro::core::Result<void> Game::FollowMapRelief() {
  if (auto followed = FollowGreyWorldRelief(); !followed) {
    return followed;
  }
  // ⚠ APRÈS GREYWORLD, qui relit la carte avec son sol d'origine.
  if (auto rounded =
          SyncStageGround(*stage_, terrain_, settings_->options().graphics().rounded_ground());
      !rounded) {
    return rounded;
  }
  // LE RELIEF RETOUCHÉ suit « Ombres en temps réel » : voir `ShownGroundEdits`.
  FollowMapGround();
  return {};
}

ro::core::Result<void> Game::FollowGreyWorldRelief() {
  GreyWorldOptions& page = settings_->options().pages().grey_world;
  // AVANT QUE LA PAGE NE SE COMPOSE : elle dit sous sa propre couleur que celle
  // de l'établi l'emporte.
  page.NoteStaffGround(
      StaffGroundWins(windows_.staff_tools.look.flat_ground, Rights().staff_tools));

  // 🔴 TÉMOIN D'ÉTAT, PAS D'ÉVÉNEMENT : c'est la divergence entre ce que la carte
  // affichée a demandé et ce que le réglage veut qui déclenche la relecture.
  // Une case cochée, un réglage relu du fichier ou remis à ses défauts y
  // passent tous, sans qu'aucun ait à prévenir.
  const bool wanted = ro::render::GreyWorldFlattens(page.look());
  if (!stage_.has_value() || stage_->flatten_asked == wanted) {
    return {};
  }
  const MapArrival arrival{.vfs = &*vfs_,
                           .stage = &*stage_,
                           .planner = &planner_,
                           .world = &world_,
                           .terrain = &terrain_,
                           .water = &water_,
                           .models = &models_,
                           .background = &background_,
                           .attempted = &attempted_map_,
                           .flatten_relief = wanted,
                           .map_edits = &map_edits_,
                           .real_time_shadows = settings_->options().graphics().real_time_shadows(),
                           .rounded_ground = settings_->options().graphics().rounded_ground()};
  const ro::core::Result<bool> reloaded = ReloadStageRelief(arrival);
  if (!reloaded) {
    return std::unexpected(reloaded.error());
  }
  if (!*reloaded) {
    return {};
  }
  // CE QUI S'ÉTAIT POSÉ SUR L'ANCIEN RELIEF se repose sur le nouveau : les
  // modèles repartent de leur première image, les objets d'effet de la carte se
  // relèvent à leur nouvelle place, et les unités au sol se reposent.
  //
  // ⚠ SANS LE BANDEAU DU NOM NI L'AMBIANCE SONORE, qui se lèvent avec les objets
  // à l'entrée d'une carte : on n'a pas changé de carte.
  model_clock_ = ro::render::ModelClock();
  if (map_effects_raised_) {
    FollowMapEffects(true);
  }
  if (ground_units_.has_value()) {
    ground_units_->ForgetPlacement();
  }
  return {};
}

}  // namespace moonclient
