// L'OUTIL RELIEF DU MODE ÉDITEUR, dans la boucle du jeu : voir docs/adr/0118
// et map_editor_ground.cpp.
//
// La brosse suit la souris sur le sol, et le mode Sommets y montre les points
// de la grille avec la poignée du désigné ; le sol suit le document — ses hauteurs
// à chaque image, son maillage quand il en a le temps : voir
// `kLiveGroundRemeshMs` —, et seulement sous « Ombres en temps réel » : voir
// `ShownGroundEdits`. Le suivi vaut pour tout joueur, éditeur ouvert ou non :
// couper le réglage rend le sol d'origine sans relire la carte.

#include <array>
#include <chrono>
#include <optional>
#include <utility>
#include <vector>

#include <SDL3/SDL.h>

#include "ro/core/error.h"
#include "ro/core/log.h"
#include "ro/render/ground_pick.h"

#include "game/game.h"
#include "world/map_editor_gizmo.h"
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

void Game::ServeGroundPoints(float width, float height) {
  if (!map_editor_.ground_points_mode()) {
    map_editor_.SetGroundDots({});
    return;
  }
  ro::ui::MapEditorView& view = windows_.staff_tools.map_editor;
  // LES POINTS AUTOUR DE LA SOURIS, ou du point que regarde la caméra quand la
  // souris n'est pas sur le sol, dans le rayon de la brosse.
  const std::array<float, 2> center = map_editor_.ground_pointer().value_or(
      CornerPointOf(stage_->scene.gnd, camera_.look_at.x, camera_.look_at.z));
  map_editor_.SetGroundDots(
      GroundPointDots(stage_->scene, map_edits_.Document(stage_->name).ground(), center,
                      map_editor_.Brush().radius_cubes, map_editor_.selected_ground_points(),
                      camera_, aspect_ratio_, width, height));
  // LA POIGNÉE VERTICALE, sur le point désigné.
  GizmoFrame frame;
  std::vector<GizmoHandle> handles;
  if (const std::optional<MapEditorSession::CornerPoint>& designated =
          map_editor_.designated_ground_point();
      designated.has_value()) {
    if (const std::optional<glm::vec3> world = CornerPointWorld(stage_->scene.gnd, *designated)) {
      frame = VerticalGizmoFrame(*world, camera_);
      handles = VerticalGizmoHandles(frame, camera_, aspect_ratio_, width, height);
    }
  }
  map_editor_.SetGizmo(frame, std::move(handles));
  const std::vector<ro::ui::MapEditorLine> dots = map_editor_.GroundPointTrace();
  const std::vector<ro::ui::MapEditorLine> arrows = map_editor_.GizmoTrace();
  view.lines.insert(view.lines.end(), dots.begin(), dots.end());
  view.lines.insert(view.lines.end(), arrows.begin(), arrows.end());
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
