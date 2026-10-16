#pragma once

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ro/mapedit/edit_history.h"
#include "ro/mapedit/map_edit.h"
#include "ro/render/map_scene.h"
#include "ro/ui/map_editor_panel.h"

#include "world/map_edit_store.h"
#include "world/map_editor_gizmo.h"

namespace moonclient {

// LE MODE ÉDITEUR DE CARTE : voir docs/adr/0118.
//
// La session tient ce que le panneau montre et ce que le staff fait : le mode
// ouvert ou non, la sélection, l'historique, le geste en cours. Le document
// lui-même est dans `MapEditStore`. Elle ne dessine rien et ne touche à aucun
// renderer : elle dit seulement quand le décor doit se reposer.
class MapEditorSession {
 public:
  // Ce qu'une intention demande à l'appelant.
  struct Outcome {
    // Le document a changé : le décor doit se reposer.
    bool replace_models = false;
  };

  [[nodiscard]] bool active() const noexcept { return active_; }
  void Leave() noexcept;

  // SUIT LA CARTE AFFICHÉE : une autre carte vide l'historique, la sélection
  // et le geste en cours, qui ne valent que pour la leur, et relit les noms de
  // ses modèles pour l'arbre de la scène.
  void FollowMap(std::string_view map_name, const ro::render::MapScene& scene);

  // Le modèle désigné, par son rang dans le RSW.
  [[nodiscard]] std::optional<std::size_t> selected() const noexcept {
    return selection_.model_index;
  }

  // Le modèle sous un clic dans le monde, ou rien.
  void Pick(std::optional<std::size_t> model_index);

  // LES POIGNÉES DE L'IMAGE, posées par l'appelant : leur repère, et ce
  // qu'elles montrent à l'écran.
  void SetGizmo(const GizmoFrame& frame, std::vector<GizmoHandle> handles);
  [[nodiscard]] ro::ui::MapEditorTool tool() const noexcept { return tool_; }
  // Les traits des poignées, la saisie distinguée.
  [[nodiscard]] std::vector<ro::ui::MapEditorLine> GizmoTrace() const;
  // Un appui en `pointer` saisit-il une poignée ? Le placement de départ est
  // celui que montre `view`, la vue de l'image.
  [[nodiscard]] bool Grab(glm::vec2 pointer, const ro::ui::MapEditorView& view);
  [[nodiscard]] bool dragging() const noexcept { return drag_.has_value(); }
  // Le glissé de la poignée saisie jusqu'en `pointer`, puis son relâcher : ils
  // deviennent des intentions de placement, que `TakeQueued` rend.
  void DragTo(glm::vec2 pointer);
  void Release();
  [[nodiscard]] std::vector<ro::ui::MapEditorIntent> TakeQueued();

  [[nodiscard]] Outcome Apply(const ro::ui::MapEditorIntent& intent,
                              const ro::render::MapScene& scene, MapEditStore& store);

  // Ce que le panneau montre.
  [[nodiscard]] ro::ui::MapEditorView View(const ro::render::MapScene& scene,
                                           const MapEditStore& store) const;

  [[nodiscard]] const std::string& map_name() const noexcept { return map_name_; }

 private:
  Outcome Place(const ro::ui::map_editor::Place& place, const ro::render::MapScene& scene,
                MapEditStore& store);
  Outcome SetWhy(std::string why, MapEditStore& store);
  Outcome Revert(MapEditStore& store);
  void Save(MapEditStore& store);
  // Retient un geste dans l'historique, s'il a changé quelque chose.
  void Record(std::string label, const ro::mapedit::MapEdit& before,
              const ro::mapedit::MapEdit& after);

  bool active_ = false;
  std::string map_name_;
  ro::mapedit::EditHistory history_;
  ro::mapedit::EditSelection selection_;
  // Le document d'avant le geste qui glisse, jusqu'à son relâcher.
  std::optional<ro::mapedit::MapEdit> gesture_before_;
  // Les raisons saisies pour un modèle qui n'est pas encore retouché : elles
  // rejoignent sa retouche quand elle naît.
  std::map<std::size_t, std::string> pending_why_;
  std::string status_;
  // Le fichier de chaque modèle de la carte, en UTF-8, dans l'ordre du RSW :
  // converti une fois par carte, et non à chaque image.
  std::vector<std::string> model_files_;
  ro::ui::MapEditorTool tool_ = ro::ui::MapEditorTool::kMove;
  GizmoFrame gizmo_frame_;
  std::vector<GizmoHandle> handles_;
  // LA POIGNÉE SAISIE : son rang, le point de l'appui, le placement d'alors, et
  // le dernier placement qu'elle a donné.
  struct Drag {
    std::size_t handle = 0;
    GizmoHandle grabbed;
    GizmoFrame frame;
    glm::vec2 from{0.0F, 0.0F};
    ro::ui::map_editor::Place start;
    ro::ui::map_editor::Place last;
  };
  std::optional<Drag> drag_;
  std::vector<ro::ui::MapEditorIntent> queued_;
};

}  // namespace moonclient
