#pragma once

#include <filesystem>
#include <fstream>
#include <ios>

#include <yaml-cpp/yaml.h>

#include "ro/core/utf8_path.h"

namespace moonclient {

// LIRE UN FICHIER YAML PAR SON CHEMIN.
//
// 🔴 JAMAIS `YAML::LoadFile(path.string())`. yaml-cpp ouvre un nom ÉTROIT,
// que MSVC lit dans la page ANSI : un dossier dont un caractère manque à
// cette page rend le fichier introuvable, et `path.string()` lève alors
// `std::system_error` — qui traverse les `catch (YAML::Exception&)` et
// arrête le client. On ouvre donc le flux par le chemin LARGE.
//
// ⚠ LES MÊMES EXCEPTIONS QUE `YAML::LoadFile`, et c'est voulu : les appelants
// les attrapent déjà. `YAML::BadFile` quand le fichier ne s'ouvre pas,
// `YAML::ParserException` quand il est abîmé.
[[nodiscard]] inline YAML::Node LoadYamlFile(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    throw YAML::BadFile(ro::core::Utf8FromPath(path));
  }
  return YAML::Load(stream);
}

}  // namespace moonclient
