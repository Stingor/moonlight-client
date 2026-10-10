#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "ro/core/error.h"
#include "ro/vfs/vfs.h"

namespace ro::vfs {

// LES LIVRES QU'ON LIT : la liste des items qui en sont, et leur texte.
//
//   data\bookitemnametable.txt   une ligne « <item>#» par livre, des lignes
//                                vides entre les séries
//   data\book\<item>.txt         le texte, balisé comme une description —
//                                « ^RRGGBB » change l'encre
//
// 🔴 LA PREMIÈRE LIGNE D'UN LIVRE PEUT ÊTRE SA COULEUR DE FOND : « %faf0e6 ».
// Elle ne s'affiche pas ; sans elle, le fond est celui du client.

inline constexpr const char* kBookListPath = "data\\bookitemnametable.txt";

// Les items qui sont des livres. Vide quand l'installation n'a pas la table.
struct BookList {
  std::unordered_set<std::uint32_t> item_ids;
  [[nodiscard]] bool Has(std::uint32_t item_id) const { return item_ids.contains(item_id); }
};

[[nodiscard]] core::Result<BookList> ReadBookList(Vfs& vfs);

// La couleur de fond, en rouge, vert, bleu.
struct BookBackground {
  std::uint8_t red = 0;
  std::uint8_t green = 0;
  std::uint8_t blue = 0;
};

struct Book {
  std::optional<BookBackground> background;
  // Les lignes, en UTF-8, balisage compris, SANS la ligne de fond.
  std::vector<std::string> lines;
};

[[nodiscard]] core::Result<Book> ReadBook(Vfs& vfs, std::uint32_t item_id);

// Le texte d'un livre déjà lu en mémoire — pour les essais, et pour `ReadBook`.
[[nodiscard]] Book ParseBook(std::string_view cp949);

}  // namespace ro::vfs
