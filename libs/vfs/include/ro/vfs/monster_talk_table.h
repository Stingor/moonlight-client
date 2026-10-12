#pragma once

#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "ro/core/error.h"
#include "ro/vfs/vfs.h"

namespace ro::vfs {

// CE QUE DISENT LES MONSTRES QUAND ILS FRAPPENT, lu dans
// `data\yaml_files\monster_talk.yml`, que `scripts/yaml_tables/monster_talk.py`
// tire du fichier de répliques du jeu :
//
//     version: 1
//     species:
//       poring:
//         attack: [« … », …]
//
// L'espèce est le nom de ressource de la classe, en minuscules ; le coup
// (`attack`) est la seule occasion que le client demande.

inline constexpr std::string_view kMonsterTalkPath = "data\\yaml_files\\monster_talk.yml";

struct MonsterTalkTable {
  std::unordered_map<std::string, std::vector<std::string>> attack_lines_by_species;

  // Les répliques entre lesquelles le client tire au sort quand un acteur de
  // cette espèce frappe. Vide quand elle n'a rien à dire. Le nom se compare
  // en tenant compte de la casse.
  [[nodiscard]] std::span<const std::string> AttackLinesOf(std::string_view species) const;
};

// Le texte de `monster_talk.yml`. Refuse un texte qui n'est pas du YAML, dont
// `version` n'est pas 1, ou dont une espèce ou une occasion porte un nom
// inconnu ou une forme inattendue.
[[nodiscard]] core::Result<MonsterTalkTable> ParseMonsterTalkTable(std::string_view yaml_text);

// ABSENTE OU ILLISIBLE, la table est vide et le journal le dit : les monstres
// se taisent.
[[nodiscard]] MonsterTalkTable ReadMonsterTalkTable(Vfs& vfs);

}  // namespace ro::vfs
