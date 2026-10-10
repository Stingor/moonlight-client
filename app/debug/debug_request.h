#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace moonclient {

// ── UNE REQUÊTE DE LA CONSOLE DE MISE AU POINT ──────────────────────────────
//
// Une ligne de mots séparés par des espaces : `<id> <méthode> [clé=valeur]…`,
// par exemple `7 acteurs rayon=20 genre=monstre`. Le pont MCP traduit ses appels
// en ces lignes ; la console n'a donc pas à lire de JSON.
//
// ⚠ UNE VALEUR NE PORTE PAS D'ESPACE : aucune requête n'en a besoin, et c'est ce
// qui garde l'analyse en une ligne.
struct DebugRequest {
  std::int64_t id = 0;
  std::string method;
  std::map<std::string, std::string, std::less<>> params;

  // La valeur entière d'un paramètre, ou `fallback` s'il manque ou ne se lit pas.
  [[nodiscard]] std::int64_t IntParam(std::string_view key, std::int64_t fallback) const;
  // La valeur texte d'un paramètre, ou vide.
  [[nodiscard]] std::string_view TextParam(std::string_view key) const;
};

// Rien quand la ligne n'a pas au moins un identifiant entier et une méthode.
[[nodiscard]] std::optional<DebugRequest> ParseDebugRequest(std::string_view line);

}  // namespace moonclient
