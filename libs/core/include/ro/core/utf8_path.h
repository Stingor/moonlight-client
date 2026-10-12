#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace ro::core {

// UN CHEMIN ET SON TEXTE UTF-8, dans les deux sens.
//
// 🔴 TOUT LE TEXTE DE CE CLIENT EST EN UTF-8, et c'est précisément ce que
// `std::filesystem::path` ne suppose pas sous MSVC : un `char*` y est lu dans
// la page ANSI du système, et `path::string()` y retourne. Un dossier
// « C:\Jeux\Ragnarök » s'abîme donc à l'aller — le fichier devient
// introuvable — et `string()` LÈVE `std::system_error` au retour dès qu'un
// caractère manque à la page ANSI — une exception qu'aucun `catch` d'erreur
// de fichier n'attend.
//
// ⚠ On passe donc par `char8_t`, que les deux plateformes lisent en UTF-8.

[[nodiscard]] inline std::filesystem::path PathFromUtf8(std::string_view utf8) {
  return {std::u8string(reinterpret_cast<const char8_t*>(utf8.data()), utf8.size())};
}

[[nodiscard]] inline std::string Utf8FromPath(const std::filesystem::path& path) {
  const std::u8string text = path.u8string();
  return {reinterpret_cast<const char*>(text.data()), text.size()};
}

}  // namespace ro::core
