// LES EFFETS DE LA CARTE : ses objets d'effet levés, avancés et peints, et ce
// que ses semeurs lancent.

#include <optional>
#include <vector>

#include "ro/render/grey_world.h"
#include "ro/render/map_effect_lots.h"
#include "ro/sim/actor.h"

#include "game/game.h"
#include "options/game_options.h"

namespace moonclient {

void Game::RaiseMapSky(std::uint32_t now_ms) {
  if (!stage_.has_value() ||
      (stage_->scene.sky_effects.empty() && stage_->scene.weather_effects.empty())) {
    return;
  }
  // `/effect` ET `/skyeffect` RETIRENT LE CIEL ET LA MÉTÉO, GreyWorld le ciel
  // seul ; tous les rendent quand on rebascule. C'est l'écart entre ce qui est
  // levé et ce que les réglages veulent qui décide, comme pour le relief.
  //
  // ⚠ LA LISTE BLANCHE DE `/effect` N'Y CHANGE RIEN : c'est la bascule qui se
  // lit, pas l'effet. Et taire l'horloge ne suffit pas — les nœuds du ciel
  // RENAISSENT sur place et vivraient aussi longtemps que la carte : il faut
  // éteindre l'effet.
  GameOptions& options = settings_->options();
  const bool sky_cut = !options.sky_shown();
  const bool hide_sky =
      sky_cut || ro::render::GreyWorldHidesMapEffects(options.pages().grey_world.look());
  const bool hide_weather = sky_cut;
  if (sky_raised_ && hide_sky == sky_hidden_ && hide_weather == weather_hidden_) {
    return;
  }
  // 🔴 LE CIEL S'ATTACHE AU JOUEUR, et il faut donc l'attendre. Ses particules
  // se sèment dans une boîte de quinze cellules autour de leur ancre et y
  // RENAISSENT ; posées au centre de la carte, elles resteraient une nuée
  // lointaine qu'on ne croiserait jamais. Le client fait de même — il recopie à
  // chaque tic la position que sa scène lui donne.
  //
  // ⚠ ET LE JOUEUR N'EXISTE PAS ENCORE À L'OUVERTURE DE LA CARTE : en ligne, sa
  // fiche arrive par le réseau quelques images plus tard. C'est pourquoi le ciel
  // se lève ICI, à l'image qui peut, et non au chargement.
  const ro::sim::Actor* local_player = world_.player();
  if (local_player == nullptr) {
    return;
  }
  const ro::sim::FractionalCell cell =
      ro::sim::InterpolatedCell(*local_player, planner_.PathFor(*local_player), now_ms);
  const std::array<float, 3> ground = GroundPositionAt(*stage_, cell.x, cell.y);
  // LE CIEL ET LA MÉTÉO, au même instant et sur le même porteur. Un groupe ne se
  // touche que si son état change : relever un groupe déjà levé empilerait ses
  // nœuds.
  const auto follow = [&](const std::vector<std::uint32_t>& effect_ids, bool hide,
                          bool was_hidden) {
    if (sky_raised_ && hide == was_hidden) {
      return;
    }
    for (const std::uint32_t effect_id : effect_ids) {
      if (hide) {
        effect_nodes_->StopEffect(effect_id, world_.player_id());
      } else {
        effect_nodes_->PlayProcedural(effect_id, glm::vec3(ground[0], ground[1], ground[2]),
                                      std::nullopt, 0.0F, world_.player_id());
      }
    }
  };
  follow(stage_->scene.sky_effects, hide_sky, sky_hidden_);
  follow(stage_->scene.weather_effects, hide_weather, weather_hidden_);
  sky_hidden_ = hide_sky;
  weather_hidden_ = hide_weather;
  sky_raised_ = true;
}

void Game::AdvanceMapEffects(std::uint32_t now_ms) {
  if (!map_effects_raised_) {
    map_effects_.Raise(stage_->scene.effect_table, stage_->scene.rsw.effects(),
                       stage_->scene.gnd.width(), stage_->scene.gnd.height());
    // LES SOURCES DE SON, au même instant : le client les crée dans la même
    // passe que les objets d'effet, juste après eux.
    if (actor_sounds_.has_value()) {
      actor_sounds_->RaiseMapAmbience(
          stage_->scene.rsw.sounds(),
          ro::render::MapEffectFrame{stage_->scene.gnd.width(), stage_->scene.gnd.height()},
          now_ms);
    }
    // LE BANDEAU DU NOM, au même instant : le client le lève en entrant dans le
    // monde de la carte, juste après ses objets.
    pieces_.map_sign.Enter(pieces_.map_display_names.has_value()
                               ? pieces_.map_display_names->SignOf(MapSignFileOf(stage_->name))
                               : nullptr,
                           settings_->options().pages().map_sign.mode(), now_ms);
    map_effects_raised_ = true;
  }
  // LES CHEMINÉES QU'UN EFFET ANNONCÉ A DEMANDÉES rejoignent celles du RSW, et
  // APRÈS la levée, qui les effacerait. Leur sprite se charge à la première,
  // sur une carte qui n'en avait pas.
  if (effects_.has_value()) {
    for (const auto& [place, owner] : effects_->TakeChimneys()) {
      if (!stage_->scene.chimney_smoke.has_value()) {
        stage_->scene.chimney_smoke = ro::render::LoadChimneySmoke(*vfs_);
      }
      map_effects_.AddChimney(place, owner);
    }
  }
  // Et elles suivent leur porteur, ou partent avec lui : sa place du moment,
  // prise sur son chemin comme celle de tout acteur dessiné.
  map_effects_.FollowOwners([this, now_ms](std::uint32_t owner) -> std::optional<glm::vec3> {
    const auto found = world_.actors().find(owner);
    if (found == world_.actors().end()) {
      return std::nullopt;
    }
    const ro::sim::FractionalCell cell =
        ro::sim::InterpolatedCell(found->second, planner_.PathFor(found->second), now_ms);
    const std::array<float, 3> ground = GroundPositionAt(*stage_, cell.x, cell.y);
    return glm::vec3(ground[0], ground[1], ground[2]);
  });
  // 🔴 LA PORTÉE SE MESURE DEPUIS LE PERSONNAGE, pas depuis la caméra : le
  // client lit la place de son joueur. Sans
  // joueur, le point visé en tient lieu.
  glm::vec3 reference = camera_.look_at;
  std::optional<glm::vec3> player_position;
  if (const ro::sim::Actor* local_player = world_.player(); local_player != nullptr) {
    const ro::sim::FractionalCell cell =
        ro::sim::InterpolatedCell(*local_player, planner_.PathFor(*local_player), now_ms);
    const std::array<float, 3> ground = GroundPositionAt(*stage_, cell.x, cell.y);
    reference = glm::vec3(ground[0], ground[1], ground[2]);
    player_position = reference;
  }
  // LES NŒUDS AUSSI LISENT LA PLACE DU JOUEUR : certains sèment autour de lui.
  if (effect_nodes_.has_value()) {
    effect_nodes_->SetLocalPlayerPosition(player_position);
  }
  map_effects_.Advance(now_ms, reference);

  // 🔴 LES SEMEURS NE DESSINENT RIEN : ils LANCENT l'effet ordinaire de leur
  // numéro, et c'est le moteur d'effets qui le joue. `render` ne le connaît
  // pas — il rend la liste de ce qui vient d'être semé, et la partie l'engage.
  //
  // Un semis est une entrée en scène ordinaire : le client crée une instance
  // d'effet comme pour n'importe quel lancer, et
  // rien dans ce chemin ne retire son bruit. Les défauts s'appliquent donc.
  //
  // GREYWORLD TAIT LES SEMEURS sans les arrêter : ils gardent leur cadence, et
  // reprennent au même pas quand le réglage les rend.
  if (effects_.has_value() &&
      !ro::render::GreyWorldHidesMapEffects(settings_->options().pages().grey_world.look())) {
    for (const ro::render::MapEffectSowing& sowing : map_effects_.sown()) {
      effects_->PlayById(static_cast<std::uint32_t>(sowing.effect_id), sowing.world_position,
                         now_ms, 0, false, 0, true, 0, sowing.instance_scale,
                         sowing.instance_second);
    }
  }
}

void Game::DrawMapEffects() {
  map_effect_quads_.clear();
  const bool hides_map_effects =
      ro::render::GreyWorldHidesMapEffects(settings_->options().pages().grey_world.look());
  for (const ro::render::MapEffectLot& lot : map_effects_.Lots(
           camera_,
           stage_->scene.chimney_smoke.has_value() ? &*stage_->scene.chimney_smoke : nullptr)) {
    // ⚠ `/effect` COUPÉ, LE CLIENT NE LES PEINT PLUS, mais les laisse vivre :
    // seul son dessin lit le réglage.
    if (effects_.has_value() && !effects_->ShowsEffect(static_cast<std::uint32_t>(lot.effect_id))) {
      continue;
    }
    // GreyWorld fait de même pour ceux de la carte, pas pour ceux d'un acteur.
    if (hides_map_effects && !lot.carried) {
      continue;
    }
    if (const std::optional<ro::render::DrawnWorldQuads> drawn =
            ro::render::DrawnMapEffectLot(lot, stage_->scene.effect_textures)) {
      map_effect_quads_.push_back(*drawn);
    }
  }
  if (!map_effect_quads_.empty()) {
    effect_renderer_.DrawWorldQuads(camera_, map_effect_quads_, aspect_ratio_,
                                    FogOf(*stage_, settings_->options().graphics().fog()));
  }
}

}  // namespace moonclient
