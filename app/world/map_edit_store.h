#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <string_view>

#include "ro/core/error.h"
#include "ro/mapedit/map_edit.h"
#include "ro/vfs/vfs.h"

namespace moonclient {

// LES DOCUMENTS D'ÉDITION QUE LE JEU TIENT, un par carte : voir docs/adr/0118.
//
// Le document d'une carte se lit une fois, à sa première entrée : dans le
// dossier du client s'il y est, sinon dans l'installation. Il reste ensuite en
// mémoire, et l'éditeur le change là ; « Enregistrer » l'écrit dans le dossier
// du client, à plat. Le jeu n'écrit jamais dans une archive, et le système de
// fichiers lit le disque avant elles : ce qui vient d'être écrit est ce qui se
// relit. Un client d'essai, qui lit une installation sans y écrire, garde
// ainsi ses documents à part.
class MapEditStore {
 public:
  void Open(std::filesystem::path client_root) { client_root_ = std::move(client_root); }

  // Lit le document de `map_name` s'il ne l'est pas encore. Un document du
  // client illisible n'en donne aucun, et le journal le dit : le remplacer en
  // silence par celui de l'installation ferait perdre au staff ce qu'il croit
  // enregistré.
  void Load(std::string_view map_name, ro::vfs::Vfs& vfs);

  // Le document de `map_name` ; vide pour une carte pas encore lue.
  [[nodiscard]] const ro::mapedit::MapEdit& Document(std::string_view map_name) const;
  // Le remplace ; il diffère alors de celui qui est enregistré.
  void Set(std::string_view map_name, ro::mapedit::MapEdit edit);
  [[nodiscard]] bool Unsaved(std::string_view map_name) const;

  // Le fichier où s'écrit le document de `map_name`.
  [[nodiscard]] std::filesystem::path FileOf(std::string_view map_name) const;
  // Écrit le document de `map_name`, d'un seul tenant.
  [[nodiscard]] ro::core::Result<void> Save(std::string_view map_name);

 private:
  struct Entry {
    ro::mapedit::MapEdit edit;
    bool unsaved = false;
  };
  std::filesystem::path client_root_;
  std::map<std::string, Entry, std::less<>> by_map_;
};

}  // namespace moonclient
