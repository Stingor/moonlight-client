#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/error.h"
#include "ro/vfs/vfs.h"

namespace ro::vfs {

// LES MONSTRES QUE LE SERVEUR CONNAÎT — la table exportée de sa mob_db, pour
// qu'une liste propose aussi ceux que le client n'a jamais vus.
//
// La table est exportée par `scripts/yaml_tables/monster_list.py`, qui en dit
// les règles :
//
//     version: 1
//     monsters: {
//       1002: ["PORING", "Poring", 1, 3, 1, 1, 1, 0],
//       …
//     }
//
// Chaque ligne : AegisName, nom affiché, niveau, race, élément, niveau
// d'élément, taille, rang. Race, élément et taille sont les nombres du serveur,
// ceux de la fiche de monstre ; le rang vaut 0 (ordinaire), 1 (mini-boss) ou
// 2 (MVP).
//
// 🔴 UNE TABLE ABSENTE NE DIT RIEN : `known()` est faux, et un monstre que la
// table ne nomme pas n'en est pas moins possible — l'appelant garde alors ceux
// qu'il a vus.

// Le rang d'un monstre, celui de la fiche du serveur.
inline constexpr std::uint8_t kMonsterOrdinaryRank = 0;
inline constexpr std::uint8_t kMonsterBossRank = 1;
inline constexpr std::uint8_t kMonsterMvpRank = 2;

struct MonsterListEntry {
  std::uint16_t monster_id = 0;
  std::string aegis_name;
  // Le nom que le serveur montre, en UTF-8.
  std::string name;
  std::uint16_t level = 0;
  std::uint8_t race = 0;
  std::uint8_t element = 0;
  std::uint8_t element_level = 0;
  std::uint8_t size = 0;
  std::uint8_t rank = kMonsterOrdinaryRank;
};

class MonsterList {
 public:
  MonsterList() = default;

  // Une table lue en entier ; les entrées se rangent par identifiant.
  [[nodiscard]] static MonsterList Known(std::vector<MonsterListEntry> entries);

  [[nodiscard]] bool known() const noexcept { return known_; }
  // Par identifiant croissant.
  [[nodiscard]] std::span<const MonsterListEntry> entries() const noexcept { return entries_; }
  // Nul si la table ne nomme pas ce monstre.
  [[nodiscard]] const MonsterListEntry* Find(std::uint16_t monster_id) const;

 private:
  bool known_ = false;
  std::vector<MonsterListEntry> entries_;
};

inline constexpr std::string_view kMonsterListPath = "data\\yaml_files\\monster_list.yml";

// Lit la table depuis son texte. Refuse un texte qui n'est pas du YAML, dont
// `version` n'est pas 1, dont `monsters` n'est pas une table, ou dont une ligne
// n'a pas ses huit champs, un identifiant nul ou un rang inconnu.
[[nodiscard]] core::Result<MonsterList> ParseMonsterList(std::string_view yaml_text);

// Lit la table de l'installation. ABSENTE, elle rend une table inconnue plutôt
// qu'une erreur ; illisible, une erreur.
[[nodiscard]] core::Result<MonsterList> ReadMonsterList(Vfs& vfs);

}  // namespace ro::vfs
