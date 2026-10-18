#pragma once

#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "ro/ui/map_editor_panel.h"
#include "ro/ui/model_library_index.h"
#include "ro/vfs/vfs.h"

#include "world/model_previews.h"
#include "world/model_shelf_file.h"

namespace moonclient {

// LA BIBLIOTHÈQUE DE MODÈLES DE L'ÉDITEUR DE CARTE, côté jeu : les fichiers de
// l'installation et l'arbre de leurs dossiers, lus à la première ouverture ;
// les favoris et les récents, retenus dans `savedata\` (voir `ModelShelf`) ;
// les vignettes, sous le budget de chaque image (voir `ModelPreviews`).
//
// Le panneau dit ce qu'il veut — le fichier survolé, les vignettes de la
// grille, les favoris basculés — et le bureau le lui donne à l'image suivante.
class ModelLibraryDesk {
 public:
  // Les modèles qu'ajoutent ces intentions passent en tête des récents.
  void NoteIntents(std::span<const ro::ui::MapEditorIntent> intents);

  // 🔴 UNE FOIS PAR IMAGE, mode ouvert, après la composition de la vue : lit ce
  // que le panneau a demandé à l'image précédente, et pose dans `view` la
  // bibliothèque et l'aperçu survolé. Un contexte OpenGL doit être courant.
  void Serve(ro::vfs::Vfs& vfs, ro::ui::MapEditorView& view, ro::ui::MapEditorPanelState& panel);

  // Jette les vignettes, mode fermé. ⚠ ENTRE DEUX IMAGES.
  void Clear() { previews_.Clear(); }

 private:
  // Lit l'étagère du client à la première ouverture.
  void LoadShelf(const std::filesystem::path& client_root, ro::ui::ModelLibraryWindowState& window);
  // Republie les listes partagées, et écrit le fichier.
  void ShelfChanged();

  std::filesystem::path client_root_;
  std::shared_ptr<const std::vector<std::string>> files_;
  std::shared_ptr<const ro::ui::ModelLibraryTree> tree_;
  ModelShelf shelf_;
  std::shared_ptr<const std::vector<std::string>> favorites_;
  std::shared_ptr<const std::vector<std::string>> recents_;
  bool shelf_loaded_ = false;
  // Le fichier n'a pas pu se lire : il ne se réécrit pas, il porterait
  // l'étagère d'avant.
  bool shelf_unreadable_ = false;
  ModelPreviews previews_;
};

// LES BIBLIOTHÈQUES DE L'ÉDITEUR, lues dans l'installation à sa première
// ouverture : les modèles, relatifs à data\model\, avec leurs vignettes et
// l'étagère du staff ; les sons, relatifs à data\wav\.
struct MapEditorLibraries {
  ModelLibraryDesk models;
  std::shared_ptr<const std::vector<std::string>> sounds;
};

}  // namespace moonclient
