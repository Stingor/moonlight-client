#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/vfs/server_db.h"

namespace ro::vfs {

// ── CHERCHER DANS LA BASE ───────────────────────────────────────────────────
//
// UNE SAISIE TOUTE EN CHIFFRES EST UN IDENTIFIANT : elle désigne cet objet ou ce
// monstre, et lui seul. Sinon tout ce dont la clé de recherche CONTIENT celle
// de la saisie (`core::SearchKey` : ni casse ni accents). Vide : tout.
//
// Les clés sont celles de l'appelant, au même rang que la base : c'est lui qui
// sait le nom que le joueur lit — celui de la table des objets du client, qui
// peut être traduit.

// L'identifiant qu'une saisie toute en chiffres désigne ; rien sinon.
[[nodiscard]] std::optional<std::uint32_t> SearchedId(std::string_view text);

// Pas de borne de niveau.
inline constexpr int kNoLevelBound = 0;

struct ItemSearch {
  std::string_view text;
  // Le `Type` voulu, sans la casse ; vide : tous.
  std::string_view type;
  // Un objet qui lève l'un de ces emplacements, sans la casse ; vide : tous.
  std::span<const std::string_view> locations;
  // Le niveau requis (`EquipLevelMin`), bornes comprises.
  int level_min = kNoLevelBound;
  int level_max = kNoLevelBound;
};

// Les rangs, dans `db.items()`, des objets trouvés, par identifiant croissant.
// `name_keys` porte la clé de recherche de chaque objet, au même rang.
[[nodiscard]] std::vector<std::size_t> SearchItems(const ServerDb& db,
                                                   std::span<const std::string> name_keys,
                                                   const ItemSearch& search);

// Tous les rangs de monstre.
inline constexpr int kAnyMonsterTrait = -1;

struct MonsterSearch {
  std::string_view text;
  // Les nombres du serveur ; `kAnyMonsterTrait` : tous.
  int race = kAnyMonsterTrait;
  int element = kAnyMonsterTrait;
  int size = kAnyMonsterTrait;
  // 0 ordinaire, 1 mini-boss, 2 MVP ; `kAnyMonsterTrait` : tous.
  int rank = kAnyMonsterTrait;
  int level_min = kNoLevelBound;
  int level_max = kNoLevelBound;
};

[[nodiscard]] std::vector<std::size_t> SearchMonsters(const ServerDb& db,
                                                      std::span<const std::string> name_keys,
                                                      const MonsterSearch& search);

}  // namespace ro::vfs
