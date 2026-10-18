#pragma once

// LES LECTURES STRICTES D'UNE TABLE YAML : un champ obligatoire absent, ou un
// nombre hors de ses bornes, lève une exception, comme yaml-cpp signale un champ
// qui n'entre pas dans son type. Le lecteur qui les emploie la rattrape à sa
// frontière et la rend en erreur.
//
// En-tête PRIVÉ de `libs/vfs/src`.

#include <cstdint>
#include <string>

#include <yaml-cpp/yaml.h>

namespace ro::vfs {

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
