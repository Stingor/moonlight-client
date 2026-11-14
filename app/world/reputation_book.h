#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/error.h"
#include "ro/vfs/vfs.h"

namespace moonclient {

// LE LIVRE DES RÉPUTATIONS : les factions et leurs groupes.
//
// Lu dans `reputation.yml`, exporté des tables du serveur par
// `scripts/yaml_tables/reputation.py`. Le serveur n'envoie que des numéros : les
// noms et les bornes viennent d'ici.
//
// ⚠ UNE ABSENCE N'EST PAS UNE PANNE : le fichier arrive avec le patch. Sans lui, la
// fenêtre de réputation le dit au lieu de montrer des factions.

inline constexpr std::string_view kReputationBookPath = "data\\yaml_files\\reputation.yml";

// QUAND UNE FACTION SE MONTRE.
enum class ReputationShown : std::uint8_t {
  kAlways,
  kNever,
  // Dès qu'elle a des points, positifs ou négatifs.
  kWithPoints,
};

struct ReputationFaction {
  std::uint64_t faction_id = 0;
  std::string name;
  // Les bornes de ses points : `least` négatif ou nul, `most` positif ou nul.
  std::int64_t least = 0;
  std::int64_t most = 0;
  ReputationShown shown = ReputationShown::kAlways;
};

struct ReputationGroup {
  std::uint64_t group_id = 0;
  std::string name;
  // Ses factions, dans l'ordre du serveur.
  std::vector<std::uint64_t> faction_ids;
};

class ReputationBook {
 public:
  // Échoue sur un YAML illisible ou d'une autre version. Une section absente se lit
  // comme vide.
  [[nodiscard]] static ro::core::Result<ReputationBook> Parse(std::string_view yaml_text);

  [[nodiscard]] const ReputationFaction* FindFaction(std::uint64_t faction_id) const noexcept;

  // Par numéro croissant, comme la table.
  [[nodiscard]] const std::vector<ReputationFaction>& factions() const noexcept {
    return factions_;
  }
  [[nodiscard]] const std::vector<ReputationGroup>& groups() const noexcept { return groups_; }
  // Le livre a-t-il été lu ? Un livre vide d'une installation qui le porte n'est
  // pas un livre absent.
  [[nodiscard]] bool loaded() const noexcept { return loaded_; }

 private:
  std::vector<ReputationFaction> factions_;
  std::vector<ReputationGroup> groups_;
  bool loaded_ = false;
};

// Lit le livre de l'installation. Absent : un livre non lu, et une ligne au journal.
[[nodiscard]] ReputationBook ReadReputationBook(ro::vfs::Vfs& vfs);

}  // namespace moonclient
