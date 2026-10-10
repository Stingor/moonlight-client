#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ro/core/error.h"
#include "ro/formats/lua_table.h"
#include "ro/vfs/vfs.h"

namespace ro::vfs {

// LES TABLES DU FAMILIER, telles que l'installation les porte.
//
// Le serveur ne dit d'un familier que sa classe — un identifiant de monstre. Ce
// qui relie cette classe à son œuf, à sa nourriture, à son nom d'espèce, et ce
// que l'œuf peut devenir, vit côté client dans deux fichiers :
//
//   * `petinfo.lub`, qui indexe ses tables par les constantes de métier : il se
//     lit APRÈS `npcidentity` et `jobidentity`, dans le même état ;
//   * `SystemEN\PetEvolution.lub`, dont la donnée est une suite d'APPELS dans sa
//     fonction `main` : une recette par matériau, d'un œuf vers un œuf, et la
//     liste des œufs qui ont droit à l'auto-alimentation. C'est la table
//     traduite que le client lit ; elle est livrée en clair.
//
// ⚠ CE SONT LES TABLES DU CLIENT. Le serveur revalide l'évolution avec les
// siennes : une divergence donne une recette affichée qui échoue.

inline constexpr std::array<std::string_view, 3> kPetInfoChain = {
    "data\\luafiles514\\lua files\\datainfo\\npcidentity.lub",
    "data\\luafiles514\\lua files\\datainfo\\jobidentity.lub",
    "data\\luafiles514\\lua files\\datainfo\\petinfo.lub",
};
inline constexpr std::string_view kPetEvolutionPath = "SystemEN\\PetEvolution.lub";

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
  // L'œuf → la classe du familier qui en sort.
  std::unordered_map<std::uint32_t, std::uint32_t> class_by_egg;
  // La classe → l'item qui la nourrit.
  std::unordered_map<std::uint32_t, std::uint32_t> food_by_class;
  // La classe → le nom de l'espèce, en UTF-8.
  std::unordered_map<std::uint32_t, std::string> species_by_class;
  // L'œuf SOURCE → ses évolutions, dans l'ordre du fichier.
  std::unordered_map<std::uint32_t, std::vector<PetEvolutionRecipe>> recipes_by_egg;
  // Les œufs dont le familier peut être nourri par le serveur.
  std::unordered_set<std::uint32_t> auto_feed_eggs;
  // L'accessoire → le .act qui remplace celui du corps quand le familier le
  // porte, en UTF-8. La clé est l'identifiant d'item de l'accessoire, qui est
  // aussi l'apparence que le serveur annonce.
  //
  // ⚠ DEUX TABLES, LA CORÉENNE D'ABORD : c'est elle qui nomme les fichiers que
  // l'installation porte. La variante anglaise ne fait qu'ajouter les
  // accessoires que la coréenne ignore.
  std::unordered_map<std::uint32_t, std::string> accessory_act_by_item;

  // Zéro quand la table ne connaît pas l'œuf ou la classe.
  [[nodiscard]] std::uint32_t ClassOfEgg(std::uint32_t egg_id) const;
  [[nodiscard]] std::uint32_t FoodOf(std::uint32_t pet_class) const;
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

// Remplit les tables de `petinfo` depuis ses globales déjà évaluées.
void FillPetInfo(const formats::LuaTable& globals, PetTables& tables);

// Remplit les recettes et la liste d'auto-alimentation depuis les appels de la
// fonction `main`. Un appel d'un autre nom, ou dont les arguments ne sont pas
// des identifiants entiers, est ignoré : le client le refuserait aussi.
void FillPetEvolutions(std::span<const formats::LuaCall> calls, PetTables& tables);

// Lit les deux fichiers. ABSENT OU ILLISIBLE, chacun laisse ses tables vides et
// le dit au journal, sans coûter l'autre : la fiche du familier se montre alors
// sans évolution, ou sans nom d'espèce.
[[nodiscard]] PetTables ReadPetTables(Vfs& vfs);

}  // namespace ro::vfs
