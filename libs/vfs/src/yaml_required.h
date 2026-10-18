#pragma once

// LES LECTURES STRICTES D'UNE TABLE YAML : un champ obligatoire absent, ou un
// nombre hors de ses bornes, lève une exception, comme yaml-cpp signale un champ
// qui n'entre pas dans son type. Le lecteur qui les emploie la rattrape à sa
// frontière et la rend en erreur. La racine d'une table versionnée, elle, se
// refuse avec sa raison, que le lecteur rend sous le nom de sa table.
//
// En-tête PRIVÉ de `libs/vfs/src`.

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

#include <yaml-cpp/yaml.h>

namespace ro::vfs {

// La racine d'une table versionnée : une table YAML dont la clé `version` vaut
// `supported_version`. Un texte illisible, ou une version qui n'est pas un
// entier, lève l'exception de yaml-cpp.
[[nodiscard]] inline std::expected<YAML::Node, const char*> VersionedRoot(
    std::string_view yaml_text, int supported_version) {
  const YAML::Node whole = YAML::Load(std::string(yaml_text));
  if (!whole.IsMap()) {
    return std::unexpected("le texte n'est pas une table YAML");
  }
  const YAML::Node version = whole["version"];
  if (!version || version.as<int>() != supported_version) {
    return std::unexpected("version non gérée");
  }
  return whole;
}

[[nodiscard]] inline YAML::Node Required(const YAML::Node& map, const char* key) {
  YAML::Node value = map[key];
  if (!value) {
    throw YAML::KeyNotFound(map.Mark(), std::string(key));
  }
  return value;
}

// Un entier entre `least` et `most`, bornes comprises, ou une exception.
[[nodiscard]] inline std::int64_t Whole(const YAML::Node& node, std::int64_t least,
                                        std::int64_t most) {
  const auto value = node.as<std::int64_t>();
  if (value < least || value > most) {
    throw YAML::RepresentationException(node.Mark(), "un nombre hors de ses bornes");
  }
  return value;
}

}  // namespace ro::vfs
