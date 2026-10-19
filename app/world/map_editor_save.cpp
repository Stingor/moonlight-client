// L'ENREGISTREMENT DU DOCUMENT ET L'HISTORIQUE DES GESTES de l'éditeur de
// carte : voir `MapEditorSession`.

#include <string>
#include <utility>
#include <vector>

#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/core/utf8_path.h"

#include "world/map_editor_reasons.h"
#include "world/map_editor_session.h"

namespace moonclient {

std::vector<std::string> MapEditorSession::Save(MapEditStore& store) {
  // UNE RETOUCHE SANS RAISON NE S'ÉCRIT PAS : le document la refuserait à la
  // relecture, et toutes les autres avec elle.
  if (std::string refused = SaveRefusal(store.Document(map_name_), original_count_);
      !refused.empty()) {
    status_ = std::move(refused);
    return {};
  }
  // LE DOCUMENT DE LA CARTE s'écrit toujours : « Enregistrer » le publie, même
  // inchangé. CELUI DES LUMIÈRES DE MODÈLE, commun à toutes les cartes, quand
  // il a changé.
  std::vector<std::string> documents{map_name_};
  if (store.Unsaved(ro::mapedit::kModelLightsDocument)) {
    documents.emplace_back(ro::mapedit::kModelLightsDocument);
  }
  std::vector<std::string> saved;
  for (const std::string& document : documents) {
    if (auto written = store.Save(document); !written) {
      status_ = ro::core::Describe(written.error());
      ro::core::LogError("édition de {} : {}", document, status_);
      return saved;
    }
    ro::core::LogInfo("édition de {} enregistrée dans {}", document,
                      ro::core::Utf8FromPath(store.FileOf(document)));
    saved.push_back(document);
  }
  status_ = ro::core::Tr("Enregistré.");
  return saved;
}

MapEditorSession::Outcome MapEditorSession::StepHistory(bool redo, MapEditStore& store) {
  const std::string recorded = redo ? history_.RedoDocument() : history_.UndoDocument();
  std::optional<ro::mapedit::MapEdit> document = redo ? history_.Redo() : history_.Undo();
  if (!document.has_value()) {
    return {};
  }
  store.Set(recorded.empty() ? map_name_ : recorded, std::move(*document));
  return {.replace_models = true};
}

void MapEditorSession::Record(std::string label, const ro::mapedit::MapEdit& before,
                              const ro::mapedit::MapEdit& after) {
  if (!(before == after)) {
    history_.Record(std::move(label), before, after);
  }
}

void MapEditorSession::RecordModelLights(std::string label, const ro::mapedit::MapEdit& before,
                                         const ro::mapedit::MapEdit& after) {
  if (!(before == after)) {
    history_.Record(std::move(label), before, after,
                    std::string(ro::mapedit::kModelLightsDocument));
  }
}

}  // namespace moonclient
