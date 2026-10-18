#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "ro/core/error.h"
#include "ro/vfs/vfs.h"

namespace ro::vfs {

// LES LIVRES QU'ON LIT : la liste des items qui en sont, et leur texte.
//
//   data\yaml_files\book_items.yml   la liste, que
//                                    `scripts/yaml_tables/install_text_tables.py`
//                                    tire de `data\bookitemnametable.txt` :
//                                    « version: 1 », « books: [11000, …] »
//   data\book\<item>.txt             le texte, balisé comme une description —
//                                    « ^RRGGBB » change l'encre
//
// 🔴 LA PREMIÈRE LIGNE D'UN LIVRE PEUT ÊTRE SA COULEUR DE FOND : « %faf0e6 ».
// Elle ne s'affiche pas ; sans elle, le fond est celui du client.

inline constexpr std::string_view kBookListPath = "data\\yaml_files\\book_items.yml";

// Les items qui sont des livres. Vide quand l'installation n'a pas la table.
struct BookList {
  std::unordered_set<std::uint32_t> item_ids;
  [[nodiscard]] bool Has(std::uint32_t item_id) const { return item_ids.contains(item_id); }
};

// Le texte de `book_items.yml`. Refuse un texte qui n'est pas du YAML, dont
// `version` n'est pas 1, ou dont `books` n'est pas une liste d'identifiants.
[[nodiscard]] core::Result<BookList> ParseBookList(std::string_view yaml_text);

// ABSENTE, la table rend une liste vide ; illisible, une erreur.
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
