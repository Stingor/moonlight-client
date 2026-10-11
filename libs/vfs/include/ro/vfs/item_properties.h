#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include "ro/core/error.h"
#include "ro/vfs/item_stacking.h"
#include "ro/vfs/vfs.h"

namespace ro::vfs {

// CE QUE L'ITEM_DB DU SERVEUR DIT D'UN ITEM, et dont les commerces ont besoin
// pour refuser avant lui : s'il s'empile, s'il s'achète en buying store, et
// combien pèse une unité.
//
// La table est exportée par `scripts/yaml_tables/item_properties.py`, qui en
// dit les règles :
//
//     version: 1
//     non_stackable: [1101, 1102, …]
//     buying_store: [501, 502, …]
//     weight: {501: 70, 502: 100, …}
//
// `version` et `non_stackable` ont la forme de l'ancienne table des items non
// empilables : `ParseItemStacking` lit celle-ci telle quelle.
//
// 🔴 UNE TABLE ABSENTE NE DIT RIEN : chaque question rend `nullopt`, et
// l'appelant laisse le refus au serveur. Présente, un item qu'elle ne nomme
// pas s'empile, ne s'achète pas en buying store, et pèse 0 — ou n'existe pas
// sur le serveur, qui ne le vendra ni ne l'achètera.
class ItemProperties {
 public:
  ItemProperties() = default;

  // Une table lue en entier.
  [[nodiscard]] static ItemProperties Known(
      std::unordered_set<std::uint32_t> non_stackable_ids,
      std::unordered_set<std::uint32_t> buying_store_ids,
      std::unordered_map<std::uint32_t, std::uint32_t> weight_tenths_by_id);

  // Seule l'ancienne table des items non empilables a été lue : le reste est
  // inconnu.
  [[nodiscard]] static ItemProperties StackingOnly(ItemStacking stacking);

  [[nodiscard]] const ItemStacking& stacking() const noexcept { return stacking_; }
  [[nodiscard]] std::optional<bool> IsStackable(std::uint32_t item_id) const {
    return stacking_.IsStackable(item_id);
  }

  // Vrai si un joueur ordinaire peut mettre cet item en achat. Vide si la
  // table n'a pas été lue.
  [[nodiscard]] std::optional<bool> BuyingStoreAllowed(std::uint32_t item_id) const;

  // Le poids d'une unité, en dixièmes. Vide si la table n'a pas été lue.
  [[nodiscard]] std::optional<std::uint32_t> WeightTenthsOf(std::uint32_t item_id) const;

  // Vrai quand la table entière a été lue, pas seulement l'empilement.
  [[nodiscard]] bool known() const noexcept { return known_; }
  [[nodiscard]] std::size_t buying_store_count() const noexcept { return buying_store_ids_.size(); }
  [[nodiscard]] std::size_t weighed_count() const noexcept { return weight_tenths_by_id_.size(); }

 private:
  bool known_ = false;
  ItemStacking stacking_;
  std::unordered_set<std::uint32_t> buying_store_ids_;
  std::unordered_map<std::uint32_t, std::uint32_t> weight_tenths_by_id_;
};

inline constexpr std::string_view kItemPropertiesPath = "data\\yaml_files\\item_properties.yml";

// Lit la table depuis son texte. Refuse un texte qui n'est pas du YAML, dont
// `version` n'est pas 1, dont `non_stackable` ou `buying_store` n'est pas une
// liste d'identifiants, ou dont `weight` n'est pas une table d'identifiants à
// poids.
[[nodiscard]] core::Result<ItemProperties> ParseItemProperties(std::string_view yaml_text);

// Lit la table de l'installation. ABSENTE, elle se rabat sur l'ancienne table
// des items non empilables, que porte encore une installation qui n'a pas la
// nouvelle ; absentes toutes deux, elle rend une table inconnue plutôt qu'une
// erreur. Illisible, une erreur.
[[nodiscard]] core::Result<ItemProperties> ReadItemProperties(Vfs& vfs);

}  // namespace ro::vfs
