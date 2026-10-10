#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "ro/core/error.h"
#include "ro/vfs/vfs.h"

namespace ro::vfs {

// CE QUE REND UN CONTENEUR : boîte, album d'objets, branche.
//
// Le client en porte la table dans `packageitem.lub`, en clair : une table
// globale `tbl` indexée par l'objet conteneur, dont chaque entrée
// `{ id, prob, name, group }` est un tirage possible. Sa fonction `main` la
// verse dans la base du jeu ; on en reproduit ici l'effet, sans l'exécuter :
//
//   · `group = 99` désigne un MONSTRE (une branche en invoque), rangé au
//     groupe 6 ;
//   · tout autre groupe désigne un OBJET, rangé à son groupe.
//
// Chaque groupe est un tirage : ses entrées se partagent la SOMME de leurs
// poids — c'est la règle du serveur, dont le `Rate` de chaque entrée est la
// source de ces poids. Le groupe 0 est le lot GARANTI.
//
// ⚠ C'EST UN FICHIER D'AFFICHAGE. La vérité est la base de groupes du serveur,
// dont ce fichier est engendré : un écart entre les deux se corrige là-bas.

struct PackageEntry {
  std::uint32_t id = 0;
  bool is_monster = false;
  int weight = 0;
  // Le libellé que la table donne, en UTF-8 — celui que la fenêtre de
  // probabilités du client affiche. Vide quand la table n'en porte pas.
  std::string name;
};

struct PackageGroup {
  int id = 0;
  // La somme des poids du groupe : le dénominateur du tirage.
  int total = 0;
  std::vector<PackageEntry> entries;
};

struct PackageTable {
  std::vector<PackageGroup> groups;
};

// Le lot garanti : aucun tirage, tout sort.
inline constexpr int kGuaranteedPackageGroup = 0;
// Le groupe qu'on donne à un monstre, et celui qui le désigne dans le fichier.
inline constexpr int kMonsterPackageGroup = 6;
inline constexpr int kMonsterPackageMarker = 99;

struct PackageItems {
  std::unordered_map<std::uint32_t, PackageTable> by_package;

  [[nodiscard]] const PackageTable* Find(std::uint32_t package_id) const;
  [[nodiscard]] bool empty() const noexcept { return by_package.empty(); }
};

inline constexpr std::string_view kPackageItemsPath =
    "data\\luafiles514\\lua files\\probabilityinfo\\packageitem.lub";

// La chance d'une entrée, en POUR-CENT, arrondie comme le client l'affiche : au
// dix-millième de pour-cent, par le demi supérieur. Le lot garanti vaut 100.
[[nodiscard]] double PackagePercent(int group_id, int weight, int total);

// Lit la table depuis ses octets — compilés ou en clair.
[[nodiscard]] core::Result<PackageItems> ParsePackageItems(std::span<const std::uint8_t> bytes);

// Lit la table de l'installation. ABSENTE, elle rend une table vide plutôt
// qu'une erreur : les fenêtres qui s'en servent se taisent alors, faute de
// savoir.
[[nodiscard]] core::Result<PackageItems> ReadPackageItems(Vfs& vfs);

}  // namespace ro::vfs
