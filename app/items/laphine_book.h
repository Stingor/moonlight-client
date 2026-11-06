#pragma once

#include <array>
#include <string_view>
#include <vector>

#include "ro/core/error.h"
#include "ro/sim/laphine.h"
#include "ro/vfs/vfs.h"

namespace moonclient {

// LE LIVRE DU LAPHINE : ce que chaque objet déclencheur accepte.
//
// Lu dans `laphine.yml`, exporté des tables du serveur par
// `scripts/yaml_tables/laphine.py`. Le serveur n'envoie à l'ouverture que
// l'identifiant du déclencheur : c'est ici la seule source.
//
// ⚠ UNE ABSENCE N'EST PAS UNE PANNE : le fichier arrive avec le patch. Sans lui, une
// fenêtre du Laphine se referme en le disant.

inline constexpr std::string_view kLaphineBookPath = "data\\yaml_files\\laphine.yml";

class LaphineBook {
 public:
  // Échoue sur un YAML illisible ou d'une autre version. Une section absente se lit
  // comme vide.
  [[nodiscard]] static ro::core::Result<LaphineBook> Parse(std::string_view yaml_text);

  // La recette de ce déclencheur, nulle si le livre ne la porte pas.
  [[nodiscard]] const ro::sim::LaphineRecipe* Find(ro::sim::LaphineKind kind,
                                                   std::uint32_t trigger_item_id) const noexcept;

  [[nodiscard]] const std::vector<ro::sim::LaphineRecipe>& synthesis() const noexcept {
    return synthesis_;
  }
  [[nodiscard]] const std::vector<ro::sim::LaphineRecipe>& upgrade() const noexcept {
    return upgrade_;
  }

 private:
  std::vector<ro::sim::LaphineRecipe> synthesis_;
  std::vector<ro::sim::LaphineRecipe> upgrade_;
};

// Lit le livre de l'installation. Absent : un livre vide, et une ligne au journal.
[[nodiscard]] LaphineBook ReadLaphineBook(ro::vfs::Vfs& vfs);

}  // namespace moonclient
