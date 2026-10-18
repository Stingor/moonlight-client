#pragma once

#include <filesystem>
#include <format>
#include <fstream>
#include <ios>
#include <optional>
#include <source_location>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include <yaml-cpp/yaml.h>

#include "ro/core/error.h"
#include "ro/core/file_write.h"
#include "ro/core/log.h"
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

// LIRE UNE TABLE DU JOUEUR, un fichier de `savedata` qu'il peut corriger à la
// main : sa racine, ou rien.
//
// Rien, sans un mot, quand le fichier n'existe pas : c'est la première partie.
// Rien quand il est illisible, après l'avoir dit au journal — « <sujet> :
// fichier illisible (<cause>) — <conséquence> » : un fichier abîmé ne doit pas
// empêcher de jouer. Et rien quand sa racine n'est pas une table : elle n'a pas
// de champs, et l'indexer lèverait hors de tout `try`.
//
// `from` laisse au journal le fichier et la fonction de l'appelant.
[[nodiscard]] inline std::optional<YAML::Node> LoadYamlMap(
    const std::filesystem::path& path, std::string_view subject, std::string_view consequence,
    std::source_location from = std::source_location::current()) {
  std::error_code missing;
  if (!std::filesystem::exists(path, missing)) {
    return std::nullopt;
  }
  YAML::Node whole;
  try {
    whole = LoadYamlFile(path);
  } catch (const YAML::Exception& broken) {
    ro::core::WriteLog(
        ro::core::LogLevel::kWarning, from,
        std::format("{} : fichier illisible ({}) — {}", subject, broken.what(), consequence));
    return std::nullopt;
  }
  if (!whole.IsMap()) {
    return std::nullopt;
  }
  return whole;
}

// ÉCRIRE UNE TABLE DU JOUEUR : `header`, ce que `emitted` a émis et un saut de
// ligne, d'une écriture atomique.
//
// Un échec se dit au journal — « <sujet> : écriture impossible (<cause>) »,
// suivi de « — <conséquence> » quand il y en a une — et rend faux.
[[nodiscard]] inline bool SaveYamlFile(
    const std::filesystem::path& path, std::string_view header, const YAML::Emitter& emitted,
    std::string_view subject, std::string_view consequence = {},
    std::source_location from = std::source_location::current()) {
  const std::string text = std::string(header) + emitted.c_str() + "\n";
  if (const ro::core::Result<void> written = ro::core::WriteFileAtomically(path, text); !written) {
    std::string said =
        std::format("{} : écriture impossible ({})", subject, ro::core::Describe(written.error()));
    if (!consequence.empty()) {
      said += std::format(" — {}", consequence);
    }
    ro::core::WriteLog(ro::core::LogLevel::kWarning, from, std::move(said));
    return false;
  }
  return true;
}

}  // namespace moonclient
