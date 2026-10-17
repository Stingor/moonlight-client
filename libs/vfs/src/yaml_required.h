#pragma once

// UN CHAMP OBLIGATOIRE D'UNE TABLE YAML, absent : une exception, comme yaml-cpp
// signale un champ qui n'entre pas dans son type. Le lecteur qui l'emploie la
// rattrape à sa frontière et la rend en erreur.
//
// En-tête PRIVÉ de `libs/vfs/src`.

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

}  // namespace ro::vfs
