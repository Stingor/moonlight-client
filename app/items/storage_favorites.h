#pragma once

#include <cstdint>
#include <filesystem>
#include <set>
#include <string_view>

#include "game/save_folder.h"

namespace moonclient {

// LES FAVORIS DU COFFRE, ET ILS SONT ENTIÈREMENT À NOUS.
//
// 🔴 LE SERVEUR N'EN A AUCUN. Un objet du SAC porte un drapeau de favori que le
// serveur retient (`CZ 0x0907`) ; un objet du COFFRE n'en a pas — sa structure
// n'a pas le champ. C'est pourquoi ce client tient les favoris du coffre
// lui-même, dans son propre fichier.
//
// ⚠ LA CLÉ EST L'IDENTIFIANT DE L'OBJET, pas sa place : toutes les piles d'un
// même objet sont favorites ensemble, et un objet sorti puis remis retrouve son
// étoile.
//
// ⚠ UN ENSEMBLE TRIÉ et non une table de hachage : il se relit dans un fichier
// que l'on corrige à la main, et un ordre qui change à chaque écriture rendrait
// le diff illisible.
class StorageFavorites {
 public:
  [[nodiscard]] bool Has(std::uint32_t item_id) const { return ids_.contains(item_id); }

  // Bascule, et rend ce que l'objet est DEVENU. ⚠ ZÉRO NE SE MARQUE PAS : ce
  // n'est pas un objet, c'est l'absence d'objet.
  bool Toggle(std::uint32_t item_id);

  void Add(std::uint32_t item_id);
  void Clear() noexcept { ids_.clear(); }

  [[nodiscard]] bool empty() const noexcept { return ids_.empty(); }
  [[nodiscard]] const std::set<std::uint32_t>& ids() const noexcept { return ids_; }

 private:
  std::set<std::uint32_t> ids_;
};

// ── SUR LE DISQUE ────────────────────────────────────────────────────────────
//
// ⚠ SON PROPRE FICHIER, comme le placement libre du sac : c'est une LISTE, et le
// fichier de réglages ne sait décrire que des valeurs nommées.
inline constexpr std::string_view kStorageFavoritesFileName = "coffre_favoris.yaml";

[[nodiscard]] std::filesystem::path StorageFavoritesPath(const std::filesystem::path& client_root);

// Lit la liste. Un fichier absent, illisible ou vide laisse l'onglet vide — ce
// qui est exactement ce qu'un joueur qui n'a rien marqué attend.
void LoadStorageFavorites(const std::filesystem::path& client_root, StorageFavorites& into);

// Écrit la liste. Rend faux si le fichier n'a pas pu être écrit.
[[nodiscard]] bool SaveStorageFavorites(const std::filesystem::path& client_root,
                                        const StorageFavorites& from);

}  // namespace moonclient
