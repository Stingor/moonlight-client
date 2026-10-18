// L'ENREGISTREMENT DU DOCUMENT ET L'HISTORIQUE DES GESTES de l'éditeur de
// carte : voir `MapEditorSession`.

#include <string>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/core/utf8_path.h"

#include "world/map_editor_reasons.h"
#include "world/map_editor_session.h"

namespace moonclient {

bool MapEditorSession::Save(MapEditStore& store) {
  // UNE RETOUCHE SANS RAISON NE S'ÉCRIT PAS : le document la refuserait à la
  // relecture, et toutes les autres avec elle.
  if (std::string refused = SaveRefusal(store.Document(map_name_), original_count_);
      !refused.empty()) {
    status_ = std::move(refused);
    return false;
  }
  if (auto saved = store.Save(map_name_); !saved) {
    status_ = ro::core::Describe(saved.error());
    ro::core::LogError("édition de {} : {}", map_name_, status_);
    return false;
  }
  status_ = ro::core::Tr("Enregistré.");
  ro::core::LogInfo("édition de {} enregistrée dans {}", map_name_,
                    ro::core::Utf8FromPath(store.FileOf(map_name_)));
  return true;
}

void MapEditorSession::Record(std::string label, const ro::mapedit::MapEdit& before,
                              const ro::mapedit::MapEdit& after) {
  if (!(before == after)) {
    history_.Record(std::move(label), before, after);
  }
}

}  // namespace moonclient
