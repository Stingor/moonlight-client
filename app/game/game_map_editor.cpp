// LE MODE ÉDITEUR DE CARTE, dans la boucle du jeu : voir docs/adr/0118 et
// `MapEditorSession`.
//
// Le panneau rend des intentions ; ici, elles passent à la session, le décor se
// repose quand le document change, et le panneau reçoit sa vue de l'image.

#include <string>

#include "ro/core/log.h"
#include "ro/render/map_scene.h"

#include "game/game.h"
#include "world/map_editor_overlay.h"

namespace moonclient {

void Game::ServeMapEditor() {
  ro::ui::StaffToolsState& tools = windows_.staff_tools;
  // SANS LE DROIT, le mode se ferme : il peut se perdre en cours de session.
  if (!Rights().staff_tools || !stage_.has_value()) {
    map_editor_.Leave();
    tools.map_editor_intents.clear();
    tools.map_editor = ro::ui::MapEditorView();
    return;
  }
  map_editor_.FollowMap(stage_->name, stage_->scene);
  bool replace = false;
  for (const ro::ui::MapEditorIntent& intent : tools.map_editor_intents) {
    replace |= map_editor_.Apply(intent, stage_->scene, map_edits_).replace_models;
  }
  tools.map_editor_intents.clear();
  // LES GESTES DES POIGNÉES, mis en file par la souris.
  for (const ro::ui::MapEditorIntent& intent : map_editor_.TakeQueued()) {
    replace |= map_editor_.Apply(intent, stage_->scene, map_edits_).replace_models;
  }
  if (replace) {
    ReplaceMapModels();
  }
  tools.map_editor = map_editor_.View(stage_->scene, map_edits_);
  if (!map_editor_.active() || !map_editor_.selected().has_value()) {
    map_editor_.SetGizmo(GizmoFrame(), {});
    return;
  }
  const auto width = static_cast<float>(shell_.width());
  const auto height = static_cast<float>(shell_.height());
  tools.map_editor.lines = SelectionOutline(stage_->scene, *map_editor_.selected(), camera_,
                                            aspect_ratio_, width, height);
  // LES POIGNÉES, sur le pivot du placement que montre la vue.
  const ro::render::MapEffectFrame map_frame{stage_->scene.gnd.width(), stage_->scene.gnd.height()};
  const GizmoFrame frame = GizmoFrameOf(map_frame, tools.map_editor.position, camera_);
  map_editor_.SetGizmo(
      frame, GizmoHandles(map_editor_.tool(), frame, camera_, aspect_ratio_, width, height));
  const std::vector<ro::ui::MapEditorLine> handles = map_editor_.GizmoTrace();
  tools.map_editor.lines.insert(tools.map_editor.lines.end(), handles.begin(), handles.end());
}

void Game::ReplaceMapModels() {
  ro::render::MapScene& scene = stage_->scene;
  const ro::mapedit::MapEditReport report =
      ro::render::ReplaceSceneModels(scene, map_edits_.Document(stage_->name));
  for (const std::string& refusal : report.refused) {
    ro::core::LogWarning("édition de {} refusée — {}", stage_->name, refusal);
  }
  if (auto uploaded = models_.Upload(scene.models); !uploaded) {
    ro::core::LogError("édition de {} : le décor ne se téléverse pas — {}", stage_->name,
                       ro::core::Describe(uploaded.error()));
  }
  // L'horloge des modèles animés repart sur les nouvelles poses.
  model_clock_ = ro::render::ModelClock();
}

}  // namespace moonclient
