// GREYWORLD CÔTÉ PARTIE : le relief de la carte suit le réglage.

#include <utility>
#include <vector>

#include "ro/render/map_effect_math.h"

#include "game/game.h"
#include "world/grey_world_view.h"

namespace moonclient {

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
                           .flatten_relief = wanted};
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
  //
  // 🔴 LES CHEMINÉES PORTÉES PAR UN ACTEUR SURVIVENT : la relève les efface avec
  // celles de la carte, et rien ne les redemanderait. Leur porteur les recale à
  // l'image suivante.
  model_clock_ = ro::render::ModelClock();
  const ro::render::MapEffectFrame frame{stage_->scene.gnd.width(), stage_->scene.gnd.height()};
  std::vector<std::pair<glm::vec3, std::uint32_t>> carried;
  for (const ro::render::MapChimney& chimney : map_effects_.chimneys()) {
    if (chimney.owner_id() != 0) {
      carried.emplace_back(frame.ToWorld(chimney.native_position()), chimney.owner_id());
    }
  }
  map_effects_.Raise(stage_->scene.effect_table, stage_->scene.rsw.effects(),
                     stage_->scene.gnd.width(), stage_->scene.gnd.height());
  for (const auto& [place, owner] : carried) {
    map_effects_.AddChimney(place, owner);
  }
  if (ground_units_.has_value()) {
    ground_units_->ForgetPlacement();
  }
  return {};
}

}  // namespace moonclient
