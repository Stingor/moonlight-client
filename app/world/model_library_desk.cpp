#include "world/model_library_desk.h"

#include <algorithm>
#include <utility>
#include <variant>

#include "ro/core/error.h"
#include "ro/core/log.h"
#include "ro/core/utf8_path.h"
#include "ro/render/decor_vignette_cache.h"

namespace moonclient {

void ModelLibraryDesk::NoteIntents(std::span<const ro::ui::MapEditorIntent> intents) {
  bool changed = false;
  for (const ro::ui::MapEditorIntent& intent : intents) {
    if (const auto* added = std::get_if<ro::ui::map_editor::AddModel>(&intent);
        added != nullptr && !added->file.empty()) {
      RememberRecent(shelf_, added->file);
      changed = true;
    }
  }
  if (changed) {
    ShelfChanged();
  }
}

void ModelLibraryDesk::Serve(ro::vfs::Vfs& vfs, ro::ui::MapEditorView& view,
                             ro::ui::MapEditorPanelState& panel) {
  ro::ui::ModelLibraryWindowState& window = panel.library_window;
  if (files_ == nullptr) {
    files_ = std::make_shared<const std::vector<std::string>>(ro::render::ListDecorModels(vfs));
    tree_ =
        std::make_shared<const ro::ui::ModelLibraryTree>(ro::ui::BuildModelLibraryTree(*files_));
  }
  if (!shelf_loaded_) {
    LoadShelf(vfs.root(), window);
  }
  // CE QUE LE PANNEAU A DEMANDÉ À L'IMAGE PRÉCÉDENTE, lu une seule fois.
  bool changed = false;
  for (const std::string& file : window.favorite_toggles) {
    changed |= ToggleFavorite(shelf_, file);
  }
  window.favorite_toggles.clear();
  if (window.cell_px_committed) {
    window.cell_px_committed = false;
    shelf_.cell_px = window.cell_px;
    changed = true;
  }
  if (changed) {
    ShelfChanged();
  }
  previews_.BeginFrame();
  // Le survol d'abord : c'est le geste le plus pressé.
  view.preview = previews_.Show(vfs, panel.hovered_model);
  view.library = ro::ui::ModelLibraryView{.files = files_,
                                          .tree = tree_,
                                          .favorites = favorites_,
                                          .recents = recents_,
                                          .vignettes = previews_.ShowMany(vfs, window.wanted)};
}

void ModelLibraryDesk::LoadShelf(const std::filesystem::path& client_root,
                                 ro::ui::ModelLibraryWindowState& window) {
  shelf_loaded_ = true;
  client_root_ = client_root;
  ro::core::Result<ModelShelf> read = LoadModelShelf(client_root_);
  if (!read) {
    shelf_unreadable_ = true;
    ro::core::LogWarning("bibliothèque de modèles : {} illisible, et laissé tel quel — {}",
                         ro::core::Utf8FromPath(ModelShelfFilePath(client_root_)),
                         ro::core::Describe(read.error()));
  } else {
    // Les récents de cette session, posés avant la première ouverture, passent
    // devant ceux du fichier.
    std::vector<std::string> fresh = std::move(shelf_.recents);
    shelf_ = std::move(*read);
    for (auto file = fresh.rbegin(); file != fresh.rend(); ++file) {
      RememberRecent(shelf_, *file);
    }
  }
  if (shelf_.cell_px > 0.0F) {
    window.cell_px = std::clamp(shelf_.cell_px, ro::ui::ModelLibraryWindowState::kSmallestCellPx,
                                ro::ui::ModelLibraryWindowState::kLargestCellPx);
  }
  favorites_ = std::make_shared<const std::vector<std::string>>(shelf_.favorites);
  recents_ = std::make_shared<const std::vector<std::string>>(shelf_.recents);
}

void ModelLibraryDesk::ShelfChanged() {
  favorites_ = std::make_shared<const std::vector<std::string>>(shelf_.favorites);
  recents_ = std::make_shared<const std::vector<std::string>>(shelf_.recents);
  // Avant la première ouverture, le dossier du client n'est pas encore connu :
  // l'étagère s'écrira au premier geste suivant.
  if (!shelf_loaded_ || shelf_unreadable_) {
    return;
  }
  if (const ro::core::Result<void> written = SaveModelShelf(client_root_, shelf_); !written) {
    ro::core::LogWarning("bibliothèque de modèles : écriture impossible — {}",
                         ro::core::Describe(written.error()));
  }
}

}  // namespace moonclient
