#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <yaml-cpp/yaml.h>

namespace moonclient {

// ── UN TEXTE DE NOS TABLES YAML ─────────────────────────────────────────────
//
// Deux formes, au choix de qui écrit la table : une chaîne nue, ou une table
// « langue: texte » — `{ fr: …, en: …, es: … }`. Les tables du champ de bataille
// et celles des quêtes s'écrivent ainsi.

// La langue d'un texte nu, et celle qu'on prend quand aucune autre ne répond :
// nos tables s'écrivent d'abord en français.
inline constexpr std::string_view kYamlTextFallbackLanguage = "fr";

// TOUTES LES LANGUES D'UN TEXTE, dans l'ordre du fichier.
//
// ⚠ GARDÉES, ET NON CHOISIES À LA LECTURE : le joueur change de langue en jeu,
// et un texte choisi une fois resterait dans l'ancienne.
struct YamlText {
  std::vector<std::pair<std::string, std::string>> by_language;

  // Le texte dans `requested` ; à défaut dans `file_language` — la langue que
  // le fichier déclare —, puis en français, puis dans la première langue écrite.
  // Vide si aucune.
  [[nodiscard]] std::string_view In(std::string_view requested,
                                    std::string_view file_language = {}) const;
  [[nodiscard]] bool empty() const noexcept { return by_language.empty(); }
};

// Lit un texte. Une chaîne nue vaut pour le français ; une langue sans texte,
// ou dont la valeur n'est pas une chaîne, est ignorée ; un nœud absent ou d'une
// autre forme rend un texte vide.
[[nodiscard]] YamlText ReadYamlText(const YAML::Node& node);

}  // namespace moonclient
