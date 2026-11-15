#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "ro/core/error.h"
#include "ro/sim/item_enchant.h"
#include "ro/vfs/vfs.h"

namespace moonclient {

// LE LIVRE DE L'ENCHANTEMENT : ce que chaque groupe admet, coûte et pose.
//
// Lu dans `item_enchant.yml`, exporté des tables du serveur par
// `scripts/yaml_tables/item_enchant.py`. Le serveur n'envoie à l'ouverture que le
// numéro du groupe : c'est ici la seule source.
//
// ⚠ UNE ABSENCE N'EST PAS UNE PANNE : le fichier arrive avec le patch. Sans lui, une
// fenêtre d'enchantement se referme en le disant.

inline constexpr std::string_view kItemEnchantBookPath = "data\\yaml_files\\item_enchant.yml";

class ItemEnchantBook {
 public:
  // Échoue sur un YAML illisible ou d'une autre version. Une section absente se lit
  // comme vide.
  [[nodiscard]] static ro::core::Result<ItemEnchantBook> Parse(std::string_view yaml_text);

  // Ce groupe, nul si le livre ne le porte pas.
  [[nodiscard]] const ro::sim::ItemEnchantGroup* Find(std::uint64_t group_id) const noexcept;

  [[nodiscard]] const std::vector<ro::sim::ItemEnchantGroup>& groups() const noexcept {
    return groups_;
  }

 private:
  std::vector<ro::sim::ItemEnchantGroup> groups_;
};

// Lit le livre de l'installation. Absent : un livre vide, et une ligne au journal.
[[nodiscard]] ItemEnchantBook ReadItemEnchantBook(ro::vfs::Vfs& vfs);

}  // namespace moonclient
