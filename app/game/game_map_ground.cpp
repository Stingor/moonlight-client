// L'OUTIL RELIEF DU MODE ÉDITEUR, dans la boucle du jeu : voir docs/adr/0118
// et map_editor_ground.cpp.
//
// La brosse suit la souris sur le sol ; le sol suit le document — ses hauteurs
// à chaque image, son maillage quand il en a le temps : voir
// `kLiveGroundRemeshMs` —, et seulement sous « Ombres en temps réel » : voir
// `ShownGroundEdits`. Le suivi vaut pour tout joueur, éditeur ouvert ou non :
// couper le réglage rend le sol d'origine sans relire la carte.

#include <array>
#include <chrono>
#include <optional>

#include <SDL3/SDL.h>

#include "ro/core/error.h"
#include "ro/core/log.h"
#include "ro/render/ground_pick.h"

#include "game/game.h"
#include "world/map_editor_overlay.h"

namespace moonclient {

void Game::ServeGroundBrush() {
  // SANS LES OMBRES EN TEMPS RÉEL, le relief retouché ne se voit pas : l'outil
  // se refuse. Voir `ShownGroundEdits`.
  map_editor_.SetGroundAllowed(settings_->options().graphics().real_time_shadows());
  if (!map_editor_.active() || map_editor_.tool() != ro::ui::MapEditorTool::kRelief) {
    map_editor_.SetGroundPointer(std::nullopt);
    return;
  }
  // LE SOL DESSINÉ, pas celui où l'on marche : la brosse retouche ce qu'on voit,
  // et un pont ne doit pas lui cacher la vallée qu'il enjambe.
  const std::optional<std::array<float, 3>> hit = ro::render::PickGround(
      stage_->scene.gnd, nullptr, camera_, aspect_ratio_, mouse_.x, mouse_.y,
      static_cast<float>(shell_.width()), static_cast<float>(shell_.height()));
  map_editor_.SetGroundPointer(
      hit.has_value() ? std::optional(CornerPointOf(stage_->scene.gnd, (*hit)[0], (*hit)[2]))
                      : std::nullopt);
  const double now_ms =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch())
          .count();
  const bool lower = (SDL_GetModState() & SDL_KMOD_SHIFT) != 0;
  map_editor_.StrokeGround(now_ms, lower, stage_->scene, map_edits_);
}

void Game::FollowMapGround() {
  if (!stage_.has_value()) {
    return;
  }
  static_cast<void>(ReshapeStageGround(
      *stage_, ShownGroundEdits(map_edits_.Document(stage_->name),
                                settings_->options().graphics().real_time_shadows())));
  const bool live = RemeshesWhileStroking(*stage_);
  const bool stroking = map_editor_.stroking_ground();
  if (stage_->ground_mesh_stale && (live || !stroking)) {
    if (auto remeshed = RemeshStageGround(*stage_, terrain_); !remeshed) {
      ro::core::LogError("édition de {} : le sol ne se téléverse pas — {}", stage_->name,
                         ro::core::Describe(remeshed.error()));
    } else if (!stroking && stage_->ground_mesh_ms.has_value()) {
      ro::core::LogDebug("éditeur de {} : relief remaillé en {:.1f} ms, {} point(s) retouché(s)",
                         stage_->name, *stage_->ground_mesh_ms, stage_->scene.ground_edits.size());
    }
  }
  map_editor_.SetGroundRemesh(stage_->ground_mesh_ms, RemeshesWhileStroking(*stage_));
}

}  // namespace moonclient
