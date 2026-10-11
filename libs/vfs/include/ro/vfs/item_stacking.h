#pragma once

#include <cstdint>
#include <optional>
#include <string_view>
#include <unordered_set>

#include "ro/core/error.h"
#include "ro/vfs/vfs.h"

namespace ro::vfs {

// LES ITEMS QUE LE SERVEUR N'EMPILE PAS : armes, armures, œufs, équipements de
// familier et d'ombre. Chacun prend une case du sac par exemplaire.
//
// La liste est exportée de l'item_db du serveur par
// `scripts/yaml_tables/item_properties.py`, qui en dit la règle, dans la
// table des propriétés d'item (`item_properties.h`). Cette forme suffit à la
// lire :
//
//     version: 1
//     non_stackable: [1101, 1102, …]
//
// `kItemStackingPath` est l'ANCIENNE table, qui ne portait que cette liste :
// une installation qui l'a sans la nouvelle la fait encore lire.
//
// 🔴 UNE TABLE ABSENTE NE DIT PAS « TOUT S'EMPILE », elle dit « on ne sait
// pas » : `IsStackable` rend alors `nullopt` pour tout item, et l'appelant
// garde le comportement qu'il aurait sans elle. Présente, un item qu'elle ne
// nomme pas s'empile — ou n'existe pas sur le serveur, qui n'en crée alors
// aucun.
class ItemStacking {
 public:
  ItemStacking() = default;

  // Une table lue : les items qu'elle nomme ne s'empilent pas.
  [[nodiscard]] static ItemStacking Known(std::unordered_set<std::uint32_t> non_stackable_ids);

  // Vide si la table n'a pas été lue.
  [[nodiscard]] std::optional<bool> IsStackable(std::uint32_t item_id) const;
  [[nodiscard]] bool known() const noexcept { return known_; }
  [[nodiscard]] std::size_t non_stackable_count() const noexcept {
    return non_stackable_ids_.size();
  }

 private:
  bool known_ = false;
  std::unordered_set<std::uint32_t> non_stackable_ids_;
};

inline constexpr std::string_view kItemStackingPath = "data\\yaml_files\\non_stackable_items.yml";

// Lit la table depuis son texte. Refuse un texte qui n'est pas du YAML, dont
// `version` n'est pas 1, ou dont `non_stackable` n'est pas une liste
// d'identifiants.
[[nodiscard]] core::Result<ItemStacking> ParseItemStacking(std::string_view yaml_text);

// Lit la table de l'installation. ABSENTE, elle rend une table inconnue plutôt
// qu'une erreur ; illisible, une erreur.
[[nodiscard]] core::Result<ItemStacking> ReadItemStacking(Vfs& vfs);

}  // namespace ro::vfs
