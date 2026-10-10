#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <unordered_set>

#include "ro/core/error.h"
#include "ro/vfs/vfs.h"

namespace ro::vfs {

// LA LISTE DES CARTES de l'installation : un identifiant d'objet par ligne,
// `<identifiant>#`, sans second champ. Le nom de la carte n'y est pas : c'est
// celui de l'objet, que porte la table des objets.
//
// ⚠ ELLE COMPLÈTE LES DEUX AUTRES TABLES QUI DISENT QU'UN OBJET EST UNE CARTE,
// celle des illustrations et celle des mots : une carte qui n'a ni dessin ni
// mot n'est connue que d'elle, et la recherche d'échoppes doit pourtant
// l'accepter dans son champ « carte ».
struct CardItemList {
  std::unordered_set<std::uint32_t> item_ids;

  [[nodiscard]] bool Contains(std::uint32_t item_id) const { return item_ids.contains(item_id); }

  [[nodiscard]] std::size_t size() const noexcept { return item_ids.size(); }
  [[nodiscard]] bool empty() const noexcept { return item_ids.empty(); }
};

inline constexpr std::string_view kCardItemListPath = "data\\carditemnametable.txt";

// Lit la liste. ABSENTE, elle rend une liste vide plutôt qu'une erreur : les
// cartes ne se reconnaissent alors qu'à leur illustration ou à leur mot. Une
// ligne sans identifiant numérique en tête — commentaire, ligne vide ou
// abîmée — est sautée.
[[nodiscard]] core::Result<CardItemList> ReadCardItemList(Vfs& vfs);

}  // namespace ro::vfs
