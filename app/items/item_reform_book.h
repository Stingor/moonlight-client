#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "ro/core/error.h"
#include "ro/sim/item_reform.h"
#include "ro/vfs/vfs.h"

namespace moonclient {

// LE LIVRE DE LA RÉFORME : ce que chaque objet déclencheur réforme.
//
// Lu dans `item_reform.yml`, exporté des tables du serveur par
// `scripts/yaml_tables/item_reform.py`. Le serveur n'envoie à l'ouverture que
// l'identifiant du déclencheur : c'est ici la seule source.
//
// ⚠ UNE ABSENCE N'EST PAS UNE PANNE : le fichier arrive avec le patch. Sans lui, une
// fenêtre de réforme se referme en le disant.

inline constexpr std::string_view kItemReformBookPath = "data\\yaml_files\\item_reform.yml";

class ItemReformBook {
 public:
  // Échoue sur un YAML illisible ou d'une autre version. Une section absente se lit
  // comme vide.
  [[nodiscard]] static ro::core::Result<ItemReformBook> Parse(std::string_view yaml_text);

  // Ce que ce déclencheur réforme, nul si le livre ne le porte pas.
  [[nodiscard]] const ro::sim::ItemReformTrigger* Find(
      std::uint32_t trigger_item_id) const noexcept;

  [[nodiscard]] const std::vector<ro::sim::ItemReformTrigger>& triggers() const noexcept {
    return triggers_;
  }

 private:
  std::vector<ro::sim::ItemReformTrigger> triggers_;
};

// Lit le livre de l'installation. Absent : un livre vide, et une ligne au journal.
[[nodiscard]] ItemReformBook ReadItemReformBook(ro::vfs::Vfs& vfs);

}  // namespace moonclient
