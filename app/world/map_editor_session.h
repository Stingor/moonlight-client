#pragma once

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>

#include "ro/mapedit/edit_history.h"
#include "ro/mapedit/map_edit.h"
#include "ro/render/map_scene.h"
#include "ro/ui/map_editor_panel.h"

#include "world/map_edit_store.h"

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
  // et le geste en cours, qui ne valent que pour la leur.
  void FollowMap(std::string_view map_name);

  // Le modèle sous un clic dans le monde, ou rien.
  void Pick(std::optional<std::size_t> model_index);

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
};

}  // namespace moonclient
