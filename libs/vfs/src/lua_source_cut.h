#pragma once

#include <string_view>

namespace ro::vfs {

// 🔴 CERTAINES TABLES DE L'INSTALLATION SE TERMINENT PAR UN BLOC CONDITIONNEL,
// qui charge un complément facultatif quand il existe. Le lecteur refuse toute
// instruction conditionnelle — c'est son contrat, et il le tient pour tous les
// fichiers —, si bien que le fichier entier se refuserait pour ces quelques
// lignes.
//
// ⚠ ON COUPE DONC LE SOURCE À LA PREMIÈRE LIGNE QUI COMMENCE PAR `if` en colonne
// zéro. Ce qui tombe ne déclare aucune donnée : c'est à l'appelant de lire le
// complément lui-même s'il en a l'usage.
[[nodiscard]] inline std::string_view WithoutTrailingConditional(std::string_view source) {
  constexpr std::string_view kOpener = "if ";
  std::size_t line_start = 0;
  while (line_start < source.size()) {
    if (source.substr(line_start, kOpener.size()) == kOpener) return source.substr(0, line_start);
    const std::size_t line_end = source.find('\n', line_start);
    if (line_end == std::string_view::npos) break;
    line_start = line_end + 1;
  }
  return source;
}

}  // namespace ro::vfs
