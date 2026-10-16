// LE MODE ÉDITEUR DE CARTE, dans la boucle du jeu : voir docs/adr/0118 et
// `MapEditorSession`.
//
// Le panneau rend des intentions ; ici, elles passent à la session, le décor se
// repose quand le document change, et le panneau reçoit sa vue de l'image.

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/log.h"
#include "ro/render/map_scene.h"

#include "game/game.h"
#include "world/map_editor_mesh.h"
#include "world/map_editor_overlay.h"

namespace moonclient {
namespace {

constexpr std::string_view kModelFolder = "data\\model\\";
constexpr std::string_view kModelExtension = ".rsm";

// LES MODÈLES DE L'INSTALLATION, relatifs à data\model\ : ce que la
// bibliothèque de l'éditeur propose.
std::vector<std::string> ModelLibrary(ro::vfs::Vfs& vfs) {
  std::vector<std::string> files = vfs.List(kModelFolder, kModelExtension);
  for (std::string& file : files) {
    if (file.size() > kModelFolder.size()) {
      file.erase(0, kModelFolder.size());
    }
  }
  return files;
}

}  // namespace

void Game::ServeMapEditor() {
  ServeMapEditorTools();
  // LES LUMIÈRES SE MONTRENT TOUJOURS, quel que soit l'outil : un cercle sur
  // chacune.
  ro::ui::MapEditorView& view = windows_.staff_tools.map_editor;
  if (!map_editor_.active() || !stage_.has_value()) {
    map_editor_.SetLightDots({});
    return;
  }
  map_editor_.SetLightDots(LightDots(view, stage_->scene, camera_, aspect_ratio_,
                                     static_cast<float>(shell_.width()),
                                     static_cast<float>(shell_.height())));
  const std::vector<ro::ui::MapEditorLine> marks = map_editor_.LightTrace();
  view.lines.insert(view.lines.end(), marks.begin(), marks.end());
  // LA LUMIÈRE DÉSIGNÉE a la flèche de déplacement, comme une pose : aucun
  // modèle n'est alors désigné.
  if (view.selected_light.has_value() && *view.selected_light < view.lights.size() &&
      !view.selected.has_value()) {
    const ro::render::MapEffectFrame map_frame{stage_->scene.gnd.width(),
                                               stage_->scene.gnd.height()};
    const GizmoFrame frame =
        GizmoFrameOf(map_frame, view.lights[*view.selected_light].position, camera_);
    map_editor_.SetGizmo(frame, GizmoHandles(ro::ui::MapEditorTool::kMove, frame, camera_,
                                             aspect_ratio_, static_cast<float>(shell_.width()),
                                             static_cast<float>(shell_.height())));
    const std::vector<ro::ui::MapEditorLine> handles = map_editor_.GizmoTrace();
    view.lines.insert(view.lines.end(), handles.begin(), handles.end());
  }
}

void Game::ServeMapEditorTools() {
  ro::ui::StaffToolsState& tools = windows_.staff_tools;
  // SANS LE DROIT, le mode se ferme : il peut se perdre en cours de session.
  if (!Rights().staff_tools || !stage_.has_value()) {
    map_editor_.Leave();
    tools.map_editor_intents.clear();
    tools.map_editor = ro::ui::MapEditorView();
    return;
  }
  map_editor_.FollowMap(stage_->name, stage_->scene);
  const ro::render::MapEffectFrame map_frame{stage_->scene.gnd.width(), stage_->scene.gnd.height()};
  // UN MODÈLE AJOUTÉ SE POSE où regarde la caméra.
  const glm::vec3 drop = map_frame.ToNative(camera_.look_at);
  map_editor_.SetDropPoint({drop.x, drop.y, drop.z});
  bool replace = false;
  for (const ro::ui::MapEditorIntent& intent : tools.map_editor_intents) {
    replace |= map_editor_.Apply(intent, stage_->scene, map_edits_).replace_models;
  }
  tools.map_editor_intents.clear();
  // LES GESTES DES POIGNÉES ET DU CLAVIER, mis en file.
  for (const ro::ui::MapEditorIntent& intent : map_editor_.TakeQueued()) {
    replace |= map_editor_.Apply(intent, stage_->scene, map_edits_).replace_models;
  }
  if (replace) {
    ReplaceMapModels();
  }
  tools.map_editor = map_editor_.View(stage_->scene, map_edits_);
  // LA BIBLIOTHÈQUE, lue une fois dans l'installation, à la première ouverture.
  if (map_editor_.active() && model_library_ == nullptr) {
    model_library_ = std::make_shared<const std::vector<std::string>>(ModelLibrary(*vfs_));
  }
  tools.map_editor.library = model_library_;
  if (!map_editor_.active() || !map_editor_.selected().has_value()) {
    map_editor_.SetGizmo(GizmoFrame(), {});
    map_editor_.SetVertexContext(std::nullopt);
    vertex_context_.Forget();
    return;
  }
  const auto width = static_cast<float>(shell_.width());
  const auto height = static_cast<float>(shell_.height());
  tools.map_editor.lines = SelectionOutline(stage_->scene, *map_editor_.selected(), camera_,
                                            aspect_ratio_, width, height);
  if (map_editor_.tool() == ro::ui::MapEditorTool::kVertices) {
    ServeVertexTool(map_frame, width, height);
    return;
  }
  map_editor_.SetVertexContext(std::nullopt);
  map_editor_.SetVertexDots({});
  vertex_context_.Forget();
  // LES POIGNÉES, sur le pivot du placement que montre la vue.
  const GizmoFrame frame = GizmoFrameOf(map_frame, tools.map_editor.position, camera_);
  map_editor_.SetGizmo(
      frame, GizmoHandles(map_editor_.tool(), frame, camera_, aspect_ratio_, width, height));
  const std::vector<ro::ui::MapEditorLine> handles = map_editor_.GizmoTrace();
  tools.map_editor.lines.insert(tools.map_editor.lines.end(), handles.begin(), handles.end());
}

void Game::ServeVertexTool(const ro::render::MapEffectFrame& map_frame, float width, float height) {
  ro::ui::MapEditorView& view = windows_.staff_tools.map_editor;
  if (vertex_context_.Refresh(stage_->scene, *map_editor_.selected(),
                              map_edits_.Document(stage_->name), *vfs_)) {
    map_editor_.SetVertexContext(vertex_context_.context());
  }
  const std::optional<MapEditorSession::VertexContext>& context = vertex_context_.context();
  if (!context.has_value()) {
    map_editor_.SetVertexContext(std::nullopt);
    map_editor_.SetVertexDots({});
    map_editor_.SetGizmo(GizmoFrame(), {});
    return;
  }
  map_editor_.SetVertexDots(VertexDots(*context, camera_, aspect_ratio_, width, height));
  // LA FLÈCHE, sur le sommet désigné.
  std::vector<GizmoHandle> handles;
  GizmoFrame frame;
  if (const auto vertex = map_editor_.selected_vertex();
      vertex.has_value() && vertex->first < context->nodes.size() &&
      vertex->second < context->nodes[vertex->first].world.size()) {
    const glm::vec3 native =
        map_frame.ToNative(context->nodes[vertex->first].world[vertex->second]);
    frame = GizmoFrameOf(map_frame, {native.x, native.y, native.z}, camera_);
    handles =
        GizmoHandles(ro::ui::MapEditorTool::kMove, frame, camera_, aspect_ratio_, width, height);
  }
  map_editor_.SetGizmo(frame, std::move(handles));
  // LA VUE SE RECOMPOSE : elle a été faite avant le contexte, et ne savait pas
  // encore nommer le sommet désigné. Ses traits gardent la boîte du modèle.
  std::vector<ro::ui::MapEditorLine> lines = std::move(view.lines);
  view = map_editor_.View(stage_->scene, map_edits_);
  view.library = model_library_;
  const std::vector<ro::ui::MapEditorLine> dots = map_editor_.VertexTrace();
  const std::vector<ro::ui::MapEditorLine> arrows = map_editor_.GizmoTrace();
  lines.insert(lines.end(), dots.begin(), dots.end());
  lines.insert(lines.end(), arrows.begin(), arrows.end());
  view.lines = std::move(lines);
}

void Game::ReplaceMapModels() {
  const auto started = std::chrono::steady_clock::now();
  ro::render::MapScene& scene = stage_->scene;
  const ro::mapedit::MapEditReport report =
      ro::render::ReplaceSceneModels(scene, map_edits_.Document(stage_->name), &*vfs_);
  for (const std::string& refusal : report.refused) {
    ro::core::LogWarning("édition de {} refusée — {}", stage_->name, refusal);
  }
  if (auto uploaded = models_.Upload(scene.models); !uploaded) {
    ro::core::LogError("édition de {} : le décor ne se téléverse pas — {}", stage_->name,
                       ro::core::Describe(uploaded.error()));
  }
  // L'horloge des modèles animés repart sur les nouvelles poses.
  model_clock_ = ro::render::ModelClock();
  // LE BILAN, une fois par seconde de glissé : ce que coûte de reposer le décor.
  const auto finished = std::chrono::steady_clock::now();
  const double elapsed_ms = std::chrono::duration<double, std::milli>(finished - started).count();
  const double now_ms =
      std::chrono::duration<double, std::milli>(finished.time_since_epoch()).count();
  if (replace_tally_.Add(elapsed_ms, models_.baked_batches(), now_ms)) {
    ro::core::LogDebug(
        "éditeur de {} : décor reposé {} fois, {:.1f} ms en moyenne, {:.1f} au pire, {} lot(s) "
        "recuit(s) sur {} modèle(s)",
        stage_->name, replace_tally_.count, replace_tally_.average_ms(), replace_tally_.worst_ms,
        replace_tally_.baked_batches, scene.models.size());
    replace_tally_ = ReplaceTally();
  }
}

}  // namespace moonclient
