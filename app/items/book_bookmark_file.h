#pragma once

#include <cstdint>
#include <filesystem>
#include <string_view>
#include <unordered_map>

namespace moonclient {

// LES SIGNETS DES LIVRES sur le disque, tous personnages dans un fichier, comme
// les presets d'équipement : chaque entrée porte son personnage, le livre et la
// page où il a été refermé.

inline constexpr std::string_view kBookBookmarkFileName = "book_bookmarks.yml";

// Le livre (son identifiant d'objet) → la page, comptée à partir de un.
using BookBookmarks = std::unordered_map<std::uint32_t, int>;

[[nodiscard]] std::filesystem::path BookBookmarkFilePath(const std::filesystem::path& client_root);

// Un fichier absent ou illisible ne rend aucun signet : le livre s'ouvre à la
// première page.
[[nodiscard]] BookBookmarks LoadBookBookmarks(const std::filesystem::path& client_root,
                                              std::uint32_t character_id);

// Remplace les signets de ce personnage et garde ceux des autres. Un fichier
// illisible ne se réécrit pas : il porte les signets des autres personnages.
// Rend faux quand rien n'a été écrit.
bool SaveBookBookmarks(const std::filesystem::path& client_root, std::uint32_t character_id,
                       const BookBookmarks& bookmarks);

}  // namespace moonclient
