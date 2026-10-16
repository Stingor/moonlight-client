#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ro/core/error.h"
#include "ro/vfs/vfs.h"

namespace ro::vfs {

// LES TABLES DU FAMILIER.
//
// Le serveur ne dit d'un familier que sa classe — un identifiant de monstre. Ce
// qui relie cette classe à son œuf, à sa nourriture, à son nom d'espèce, et ce
// que l'œuf peut devenir, se lit dans deux sources :
//
//   * `data\yaml_files\pets.yml`, exporté de la pet_db du serveur par
//     `scripts/yaml_tables/pets.py` : l'œuf, la nourriture, l'objet de
//     capture, l'accessoire, le droit à l'auto-alimentation et les évolutions
//     de chaque familier. C'est la table avec laquelle le serveur juge une
//     évolution ;
//
//         version: 1
//         pets: {
//           1002: {egg: 9001, food: 531, tame: 619, accessory: 10013,
//                  auto_feed: false,
//                  evolutions: [{egg: 9069, materials: [[610, 10], [619, 3]]}]},
//           …
//         }
//
//   * `data\yaml_files\pet_info.yml`, que `scripts/yaml_tables/pet_info.py`
//     tire des tables de l'installation pour les familiers du serveur : ce
//     que le serveur ignore, le nom de l'espèce et le .act d'un accessoire.
//     Le catalogue des sprites en lit aussi les .act (`ParsePetInfo`), pour
//     habiller un familier paré.
//
//         version: 1
//         species: {1002: "Poring", …}
//         accessories: {10013: "ACC_PORING.act", …}

inline constexpr std::string_view kPetDatabasePath = "data\\yaml_files\\pets.yml";
inline constexpr std::string_view kPetInfoPath = "data\\yaml_files\\pet_info.yml";

// Un matériau d'une recette : l'item, et combien il en faut.
struct PetEvolutionMaterial {
  std::uint32_t item_id = 0;
  std::uint32_t amount = 0;
};

// Ce qu'un œuf peut devenir : l'œuf CIBLE, et les matériaux qu'il coûte.
struct PetEvolutionRecipe {
  std::uint32_t target_egg_id = 0;
  std::vector<PetEvolutionMaterial> materials;
};

struct PetTables {
  // L'œuf → la classe du familier qui en sort. Un œuf que deux familiers
  // partagent sort la plus petite classe.
  std::unordered_map<std::uint32_t, std::uint32_t> class_by_egg;
  // La classe → l'item qui la nourrit.
  std::unordered_map<std::uint32_t, std::uint32_t> food_by_class;
  // L'objet de capture → les classes qu'il capture, croissantes : le serveur
  // n'accepte la capture que du monstre dont c'est le `TameItem`.
  std::unordered_map<std::uint32_t, std::vector<std::uint32_t>> classes_by_tame_item;
  // L'accessoire → les classes que le serveur laisse le porter, croissantes.
  std::unordered_map<std::uint32_t, std::vector<std::uint32_t>> classes_by_accessory;
  // La classe → le nom de l'espèce, en UTF-8.
  std::unordered_map<std::uint32_t, std::string> species_by_class;
  // L'œuf SOURCE → ses évolutions, et leurs matériaux, dans l'ordre du fichier.
  std::unordered_map<std::uint32_t, std::vector<PetEvolutionRecipe>> recipes_by_egg;
  // Les œufs dont le familier peut être nourri par le serveur.
  std::unordered_set<std::uint32_t> auto_feed_eggs;
  // L'accessoire → le .act qui remplace celui du corps quand le familier le
  // porte, en UTF-8. La clé est l'identifiant d'item de l'accessoire, qui est
  // aussi l'apparence que le serveur annonce.
  std::unordered_map<std::uint32_t, std::string> accessory_act_by_item;

  // Zéro quand la table ne connaît pas l'œuf ou la classe.
  [[nodiscard]] std::uint32_t ClassOfEgg(std::uint32_t egg_id) const;
  [[nodiscard]] std::uint32_t FoodOf(std::uint32_t pet_class) const;
  // Vide quand l'objet ne capture rien.
  [[nodiscard]] std::span<const std::uint32_t> TamedBy(std::uint32_t item_id) const;
  // Vide quand aucun familier ne porte l'objet.
  [[nodiscard]] std::span<const std::uint32_t> WornBy(std::uint32_t item_id) const;
  // Les classes que l'objet nourrit, croissantes ; vide pour l'item zéro.
  [[nodiscard]] std::vector<std::uint32_t> FedBy(std::uint32_t item_id) const;
  // Vide quand la table ne nomme pas l'espèce.
  [[nodiscard]] std::string_view SpeciesOf(std::uint32_t pet_class) const;
  // Vide quand l'œuf n'a aucune recette.
  [[nodiscard]] std::span<const PetEvolutionRecipe> EvolutionsOf(std::uint32_t egg_id) const;
  // Nulle quand le couple n'a pas de recette.
  [[nodiscard]] const PetEvolutionRecipe* RecipeOf(std::uint32_t source_egg_id,
                                                   std::uint32_t target_egg_id) const;
  [[nodiscard]] bool AutoFeedListed(std::uint32_t egg_id) const {
    return auto_feed_eggs.contains(egg_id);
  }
  // LE CLIENT NE PROPOSE D'ÉQUIPER QUE CE QUE CETTE TABLE CONNAÎT.
  [[nodiscard]] bool IsPetAccessory(std::uint32_t item_id) const {
    return accessory_act_by_item.contains(item_id);
  }
  // Vide quand la table ne connaît pas l'accessoire.
  [[nodiscard]] std::string_view AccessoryActOf(std::uint32_t item_id) const;
};

// Remplit les espèces et les .act d'accessoire depuis le texte de
// `pet_info.yml`. Refuse un texte qui n'est pas du YAML, dont `version` n'est
// pas 1, dont `species` ou `accessories` n'est pas une table, ou dont une clé
// n'est pas un identifiant ou une valeur un texte. REFUSÉ, il ne touche pas
// `tables`.
[[nodiscard]] core::Result<void> ParsePetInfo(std::string_view yaml_text, PetTables& tables);

// Remplit l'œuf, la nourriture, la capture, l'accessoire, l'auto-alimentation
// et les évolutions depuis le texte de `pets.yml`. Refuse un texte qui n'est
// pas du YAML, dont `version` n'est pas 1, dont `pets` n'est pas une table, ou
// dont un familier n'a pas ses six champs, une classe ou un œuf nul, une
// évolution sans œuf cible ou un matériau qui n'est pas une paire [item,
// quantité]. REFUSÉ, il ne touche pas `tables`.
[[nodiscard]] core::Result<void> ParsePetDatabase(std::string_view yaml_text, PetTables& tables);

// Lit les deux sources. ABSENTE OU ILLISIBLE, chacune laisse ses tables vides et
// le dit au journal, sans coûter l'autre : la fiche du familier se montre alors
// sans nourriture ni évolution, ou sans nom d'espèce.
[[nodiscard]] PetTables ReadPetTables(Vfs& vfs);

}  // namespace ro::vfs
