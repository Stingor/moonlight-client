#pragma once

#include <cstdint>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "ro/core/error.h"
#include "ro/vfs/vfs.h"

namespace ro::vfs {

// CE QUE REND UN CONTENEUR : boîte, album d'objets, sac, branche.
//
// `data\yaml_files\item_groups.yml` est exporté des bases du serveur par
// `scripts/yaml_tables/item_groups.py`, qui y résout le script de chaque
// conteneur contre ses groupes d'items et d'invocation :
//
//     version: 1
//     packages: {
//       603: [
//         {rolls: 1, guaranteed: false, monsters: false, total: 9172,
//          entries: [[501, 12, 1], …]}
//       ],
//       …
//     }
//
// Un conteneur rend un ou plusieurs TIRAGES. Un tirage garanti rend toutes ses
// entrées ; un tirage au sort en rend UNE, chacune avec la chance poids / total.
// Le total peut dépasser la somme des poids : une entrée qui ne rend rien y
// garde sa part.

struct PackageEntry {
  // Un item, ou un monstre invoqué quand `is_monster`.
  std::uint32_t id = 0;
  bool is_monster = false;
  // Le poids de l'entrée dans son tirage ; nul dans un tirage garanti.
  int weight = 0;
  // Combien d'exemplaires sortent.
  int amount = 0;
};

struct PackageGroup {
  // Le rang du tirage dans le conteneur, à partir de 1.
  int id = 0;
  // Toutes les entrées sortent : ni poids ni total.
  bool guaranteed = false;
  // Combien de fois le conteneur répète ce tirage.
  int rolls = 1;
  // Le dénominateur du tirage.
  int total = 0;
  std::vector<PackageEntry> entries;
};

struct PackageTable {
  std::vector<PackageGroup> groups;
};

struct PackageItems {
  std::unordered_map<std::uint32_t, PackageTable> by_package;

  [[nodiscard]] const PackageTable* Find(std::uint32_t package_id) const;
  [[nodiscard]] bool empty() const noexcept { return by_package.empty(); }
};

inline constexpr std::string_view kPackageItemsPath = "data\\yaml_files\\item_groups.yml";

// La chance d'une entrée, en POUR-CENT, arrondie comme le client l'affiche : au
// dix-millième de pour-cent, par le demi supérieur. Un tirage garanti vaut 100.
[[nodiscard]] double PackagePercent(bool guaranteed, int weight, int total);

// Lit la table depuis son texte YAML. Un texte refusé ne rend rien de partiel.
[[nodiscard]] core::Result<PackageItems> ParsePackageItems(std::string_view yaml_text);

// Lit la table de l'installation. ABSENTE, elle rend une table vide plutôt
// qu'une erreur : les fenêtres qui s'en servent se taisent alors, faute de
// savoir.
[[nodiscard]] core::Result<PackageItems> ReadPackageItems(Vfs& vfs);

}  // namespace ro::vfs
