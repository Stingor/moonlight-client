#pragma once

#include <cstdint>
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
//
// LE SERVEUR TIENT LA RÉVISION (docs/adr/0124) : chaque document porte le
// numéro que le serveur lui a donné, 0 s'il n'a jamais été servi. Elle vit à
// côté du document et non dedans : l'historique de l'éditeur ne la reprend pas
// en annulant.
class MapEditStore {
 public:
  void Open(std::filesystem::path client_root) { client_root_ = std::move(client_root); }

  // Lit le document de `map_name` s'il ne l'est pas encore. Un document du
  // client illisible n'en donne aucun, et le journal le dit : le remplacer en
  // silence par celui de l'installation ferait perdre au staff ce qu'il croit
  // enregistré.
  //
  // LE DOCUMENT LIVRÉ PAR LE MIROIR (docs/adr/0128) l'emporte sur celui du
  // client quand sa révision est plus haute, et tient lieu de document quand le
  // client n'en a pas. Il ne s'écrit pas dans le dossier du client.
  void Load(std::string_view map_name, ro::vfs::Vfs& vfs);

  // Le document de `map_name` ; vide pour une carte pas encore lue.
  [[nodiscard]] const ro::mapedit::MapEdit& Document(std::string_view map_name) const;
  // Le remplace ; il diffère alors de celui qui est enregistré.
  void Set(std::string_view map_name, ro::mapedit::MapEdit edit);
  [[nodiscard]] bool Unsaved(std::string_view map_name) const;
  // La révision du document de `map_name` ; 0 s'il n'a jamais été servi.
  [[nodiscard]] std::uint32_t Revision(std::string_view map_name) const;
  // Le serveur a accepté le document sous `revision` : le prochain envoi s'y
  // fonde. Rien ne s'écrit, et les retouches non enregistrées restent.
  void SetRevision(std::string_view map_name, std::uint32_t revision);
  // ADOPTE LE DOCUMENT SERVI : il s'écrit, sous sa révision, puis remplace celui
  // que le jeu tient, retouches non enregistrées comprises.
  [[nodiscard]] ro::core::Result<void> Adopt(std::string_view map_name, ro::mapedit::MapEdit edit,
                                             std::uint32_t revision);
  // Écrit le document servi sans toucher à celui que le jeu tient : le staff a
  // des retouches non enregistrées, que l'adoption effacerait.
  [[nodiscard]] ro::core::Result<void> WriteServed(std::string_view map_name,
                                                   const ro::mapedit::MapEdit& edit,
                                                   std::uint32_t revision) const;

  // Le fichier où s'écrit le document de `map_name`.
  [[nodiscard]] std::filesystem::path FileOf(std::string_view map_name) const;
  // Écrit le document de `map_name`, d'un seul tenant.
  [[nodiscard]] ro::core::Result<void> Save(std::string_view map_name);

 private:
  struct Entry {
    ro::mapedit::MapEdit edit;
    std::uint32_t revision = 0;
    bool unsaved = false;
  };
  std::filesystem::path client_root_;
  std::map<std::string, Entry, std::less<>> by_map_;
};

}  // namespace moonclient
