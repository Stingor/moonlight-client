#include "ro/vfs/npc_illustration_paths.h"

#include <array>

#include "ro/vfs/encoding.h"

namespace ro::vfs {
namespace {

// Les deux dossiers, dans l'ordre où l'on cherche. Écrits en UTF-8 ici, et
// transcodés : les archives nomment leurs entrées en CP949.
constexpr std::array<std::string_view, 2> kFolders = {
    "data\\texture\\유저인터페이스\\illust\\",
    "data\\texture\\유저인터페이스\\cardbmp\\",
};

// Ce qu'on essaie derrière un nom sans extension, dans l'ordre.
constexpr std::array<std::string_view, 2> kGuessedExtensions = {".bmp", ".png"};

}  // namespace

std::vector<std::string> NpcIllustrationPaths(std::string_view cp949_name) {
  std::vector<std::string> paths;
  if (cp949_name.empty()) {
    return paths;
  }
  // ⚠ LE POINT SE CHERCHE DANS LE NOM, pas dans le chemin : c'est lui seul qui
  // dit si le script a écrit une extension.
  const bool has_extension = cp949_name.find('.') != std::string_view::npos;
  for (const std::string_view folder : kFolders) {
    auto prefix = Utf8ToCp949(folder);
    if (!prefix) {
      continue;
    }
    if (has_extension) {
      paths.push_back(*prefix + std::string(cp949_name));
      continue;
    }
    for (const std::string_view extension : kGuessedExtensions) {
      paths.push_back(*prefix + std::string(cp949_name) + std::string(extension));
    }
  }
  return paths;
}

}  // namespace ro::vfs
