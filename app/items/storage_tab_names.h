#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>

#include "game/save_folder.h"

namespace moonclient {

// LE NOM ET L'ICÔNE QUE LE JOUEUR DONNE À UN COFFRE.
//
// 🔴 LE SERVEUR NOMME LES SIENS, ET SES NOMS NE DISENT RIEN. Il envoie
// « Storage Alt 3 » ; le joueur, lui, sait que c'est son coffre à consommables
// et veut le voir écrit — ou le reconnaître à une potion rouge. C'est
// entièrement CLIENT : aucun paquet, aucun état du serveur.
//
// ⚠ UN NOM VIDE RETOMBE SUR CELUI DU SERVEUR, et c'est ce qui permet d'annuler :
// effacer le champ rend le coffre à son nom d'origine, plutôt que de laisser un
// bouton large de rien.
struct StorageTabName {
  std::string name;
  // L'objet dont l'icône décore l'onglet, zéro pour aucune.
  std::uint32_t icon_item_id = 0;

  [[nodiscard]] bool told() const noexcept { return !name.empty() || icon_item_id != 0; }
};

// ⚠ UNE TABLE TRIÉE PAR NUMÉRO DE COFFRE : le fichier se relit à la main, et un
// ordre qui change à chaque écriture rend le diff illisible.
class StorageTabNames {
 public:
  // Rend nul quand ce coffre n'a rien de personnalisé.
  [[nodiscard]] const StorageTabName* Of(std::uint8_t storage_id) const;

  // Pose le nom, sans toucher à l'icône. ⚠ UN NOM VIDE EFFACE le nom et rien
  // d'autre — l'icône choisie survit à un renommage.
  void Name(std::uint8_t storage_id, std::string_view name);
  // Pose l'icône, sans toucher au nom. Zéro l'efface.
  void Decorate(std::uint8_t storage_id, std::uint32_t item_id);

  [[nodiscard]] bool empty() const noexcept { return told_.empty(); }
  [[nodiscard]] const std::map<std::uint8_t, StorageTabName>& all() const noexcept { return told_; }
  void Put(std::uint8_t storage_id, StorageTabName told);

 private:
  // ⚠ CE QUI NE DIT PLUS RIEN SORT DE LA TABLE : un coffre dont on efface le nom
  // ET l'icône n'a pas à laisser une entrée vide dans le fichier.
  void Forget(std::uint8_t storage_id);

  std::map<std::uint8_t, StorageTabName> told_;
};

// ── SUR LE DISQUE ────────────────────────────────────────────────────────────
inline constexpr std::string_view kStorageTabNamesFileName = "coffre_onglets.yml";

[[nodiscard]] std::filesystem::path StorageTabNamesPath(const std::filesystem::path& client_root);

void LoadStorageTabNames(const std::filesystem::path& client_root, StorageTabNames& into);

[[nodiscard]] bool SaveStorageTabNames(const std::filesystem::path& client_root,
                                       const StorageTabNames& from);

}  // namespace moonclient
