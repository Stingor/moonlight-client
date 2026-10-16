// LE MODE ÉDITEUR DE CARTE, dans la boucle du jeu : voir docs/adr/0118 et
// `MapEditorSession`.
//
// Le panneau rend des intentions ; ici, elles passent à la session, le décor se
// repose quand le document change, et le panneau reçoit sa vue de l'image.

#include <string>

#include "ro/core/log.h"
#include "ro/render/map_scene.h"

#include "game/game.h"

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
  map_editor_.FollowMap(stage_->name);
  bool replace = false;
  for (const ro::ui::MapEditorIntent& intent : tools.map_editor_intents) {
    replace |= map_editor_.Apply(intent, stage_->scene, map_edits_).replace_models;
  }
  tools.map_editor_intents.clear();
  if (replace) {
    ReplaceMapModels();
  }
  tools.map_editor = map_editor_.View(stage_->scene, map_edits_);
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
