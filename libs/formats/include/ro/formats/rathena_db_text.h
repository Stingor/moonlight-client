#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/error.h"

namespace ro::formats {

// ── UNE BASE RATHENA, SOUS LA FORME STRICTE QUE SON SCRIPT ÉCRIT ────────────
//
// `item_db.yml` et `mob_db.yml` (`scripts/yaml_tables/server_db.py`) gardent le
// format rAthena entier — `Header`, `Body`, tous les champs —, mais écrit d'une
// seule façon, qui reste du YAML valide :
//
//   Header:
//     Type: ITEM_DB
//     Version: 3
//
//   Body:
//     - Id: 501
//       Name: Red Potion                 un texte nu, jusqu'au bout de ligne
//       Name: "Catalog: #1"              un texte à la JSON, sur une ligne
//       Jobs: {All: true, Thief: false}  une table de textes, sur une ligne
//       Classes: {}                      une table vide ; `[]` une suite vide
//       Script: |                        un bloc : `|`, `|-` ou `|+`, ses
//         itemheal rand(45,65),0;        lignes à six espaces
//       Drops:                           une suite de tables d'une ligne
//         - {Item: Jellopy, Rate: 7000}
//
// et des commentaires `#` en tête. Un analyseur YAML général lit cinq
// mégaoctets en secondes ; ce curseur les parcourt ligne à ligne. Ce qui sort
// de cette forme est REFUSÉ, avec sa ligne : le script vérifie qu'il l'écrit.

// Un couple d'une table d'une ligne.
struct RathenaPair {
  std::string_view key;
  std::string value;
};

// Un champ d'une entrée : un texte, une table d'une ligne, ou une suite de
// tables d'une ligne.
enum class RathenaShape : std::uint8_t { kText, kMap, kList };

struct RathenaField {
  std::string_view key;
  RathenaShape shape = RathenaShape::kText;
  std::string text;
  std::vector<RathenaPair> map;
  std::vector<std::vector<RathenaPair>> list;
};

// Les champs d'une entrée de `Body`, dans l'ordre écrit.
using RathenaEntry = std::vector<RathenaField>;

// Lit l'en-tête — son `Type` doit valoir `expected_type` — puis rend chaque
// entrée de `Body` à `visit`, dans l'ordre. ⚠ L'ENTRÉE NE VIT QUE LE TEMPS DE
// L'APPEL : ses clés désignent `text`, ses valeurs se recopient.
[[nodiscard]] core::Result<void> ReadRathenaDb(
    std::string_view text, std::string_view expected_type, std::string_view file_label,
    const std::function<void(const RathenaEntry&)>& visit);

}  // namespace ro::formats
