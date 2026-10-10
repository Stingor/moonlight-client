#pragma once

// LES TABLES DE QUÊTES : un texte, la langue et la version du fichier.
// Partagé par le catalogue des quêtes et celui des recommandations ; en-tête
// privé de `moonclient_quests`.

#include <string>
#include <string_view>

#include <yaml-cpp/yaml.h>

#include "game/yaml_text.h"

namespace moonclient::quest_yaml {

// Un texte, choisi dans la langue demandée — voir `YamlText::In`.
//
// ⚠ UNE FORME INATTENDUE LÈVE, ni chaîne ni table : les catalogues écartent
// alors l'entrée qui la porte, plutôt que d'en montrer une sans titre.
[[nodiscard]] inline std::string Text(const YAML::Node& node, std::string_view requested,
                                      std::string_view file_language) {
  if (node && !node.IsNull() && !node.IsScalar() && !node.IsMap()) {
    throw YAML::BadConversion(node.Mark());
  }
  return std::string(ReadYamlText(node).In(requested, file_language));
}

// La langue du fichier, `language:` en tête ; vide s'il ne la dit pas.
[[nodiscard]] inline std::string FileLanguage(const YAML::Node& whole) {
  const YAML::Node declared = whole["language"];
  return declared && declared.IsScalar() ? declared.as<std::string>() : std::string();
}

// La version du schéma, `version:` en tête ; zéro si elle manque ou ne se lit pas.
[[nodiscard]] inline int SchemaVersion(const YAML::Node& whole) {
  try {
    const YAML::Node version = whole["version"];
    return version ? version.as<int>() : 0;
  } catch (const YAML::Exception&) {
    return 0;
  }
}

}  // namespace moonclient::quest_yaml
