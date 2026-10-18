#pragma once

#include <algorithm>
#include <functional>
#include <iterator>
#include <span>

namespace ro::core {

// ── LA RECHERCHE DANS UNE TABLE TRIÉE ───────────────────────────────────────
//
// Une table est une plage CONTIGUË de lignes — `std::array`, `std::vector`,
// `std::span` — et la clé d'une ligne se lit par une projection, le plus
// souvent un pointeur de membre : `&EffectEntry::effect_id`.
//
// ⚠ PRÉCONDITION : la table est TRIÉE par cette clé, dans l'ordre de « < ».
// Une table qui ne l'est pas ne fait aucune erreur : elle rend des lignes
// fausses, ou n'en rend pas.
//
// La clé cherchée se compare telle qu'elle est passée, sans conversion vers le
// type du champ : un identifiant sur 32 bits ne se tronque pas pour se
// comparer à un champ de 16.
//
// ⚠ CE QUI EST RENDU DÉSIGNE LES LIGNES DE LA TABLE, et ne vit pas plus
// longtemps qu'elles.

// Les lignes dont la clé vaut `key`, côte à côte et dans l'ordre de la table ;
// une plage vide quand aucune ne la porte.
template <typename Rows, typename Key, typename Projection>
[[nodiscard]] constexpr auto RowsWithKey(const Rows& rows, const Key& key, Projection projection) {
  using Row = std::iter_value_t<decltype(std::ranges::begin(rows))>;
  const auto found = std::ranges::equal_range(rows, key, {}, projection);
  return std::span<const Row>(found.begin(), found.end());
}

// La PREMIÈRE ligne dont la clé vaut `key`, ou nullptr.
template <typename Rows, typename Key, typename Projection>
[[nodiscard]] constexpr auto RowWithKey(const Rows& rows, const Key& key, Projection projection) {
  using Row = std::iter_value_t<decltype(std::ranges::begin(rows))>;
  const auto found = std::ranges::lower_bound(rows, key, {}, projection);
  const Row* row = nullptr;
  if (found != std::ranges::end(rows) && std::invoke(projection, *found) == key) {
    row = &*found;
  }
  return row;
}

}  // namespace ro::core
