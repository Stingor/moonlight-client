// Voir l'en-tête.

#include "ro/vfs/pet_tables.h"

#include <algorithm>
#include <expected>
#include <utility>

#include <yaml-cpp/yaml.h>

#include "ro/core/log.h"

namespace ro::vfs {
namespace {

constexpr int kSupportedVersion = 1;
// Un matériau s'écrit [item, quantité].
constexpr std::size_t kMaterialItemField = 0;
constexpr std::size_t kMaterialAmountField = 1;
constexpr std::size_t kMaterialFieldCount = 2;

core::Error Refused(std::string detail) {
  return core::Error{core::ErrorCode::kCorruptData, "table des familiers : " + std::move(detail)};
}

core::Error RefusedInfo(std::string detail) {
  return core::Error{core::ErrorCode::kCorruptData, "noms des familiers : " + std::move(detail)};
}

// Une table `identifiant: "texte"`, facultative. Faux si elle n'en est pas une ;
// une clé ou une valeur d'un autre type lève l'exception de yaml-cpp.
[[nodiscard]] bool ReadNameMap(const YAML::Node& node,
                               std::unordered_map<std::uint32_t, std::string>& into) {
  if (!node) {
    return true;
  }
  if (!node.IsMap()) {
    return false;
  }
  for (const auto& pair : node) {
    if (!pair.second.IsScalar()) {
      throw YAML::RepresentationException(pair.second.Mark(), "une valeur n'est pas un texte");
    }
    into[pair.first.as<std::uint32_t>()] = pair.second.as<std::string>();
  }
  return true;
}

// Un champ obligatoire d'une table YAML, absent : une exception, comme yaml-cpp
// signale un champ qui n'entre pas dans son type.
[[nodiscard]] YAML::Node Required(const YAML::Node& map, const char* key) {
  YAML::Node value = map[key];
  if (!value) {
    throw YAML::KeyNotFound(map.Mark(), std::string(key));
  }
  return value;
}

[[nodiscard]] PetEvolutionRecipe ParseEvolution(const YAML::Node& node) {
  if (!node.IsMap()) {
    throw YAML::RepresentationException(node.Mark(), "une évolution n'est pas une table");
  }
  PetEvolutionRecipe recipe{.target_egg_id = Required(node, "egg").as<std::uint32_t>(),
                            .materials = {}};
  if (recipe.target_egg_id == 0) {
    throw YAML::RepresentationException(node.Mark(), "une évolution sans œuf cible");
  }
  const YAML::Node materials = Required(node, "materials");
  if (!materials.IsSequence()) {
    throw YAML::RepresentationException(materials.Mark(), "« materials » n'est pas une liste");
  }
  for (const YAML::Node& material : materials) {
    if (!material.IsSequence() || material.size() != kMaterialFieldCount) {
      throw YAML::RepresentationException(material.Mark(),
                                          "un matériau n'est pas une paire [item, quantité]");
    }
    recipe.materials.push_back(
        PetEvolutionMaterial{.item_id = material[kMaterialItemField].as<std::uint32_t>(),
                             .amount = material[kMaterialAmountField].as<std::uint32_t>()});
  }
  return recipe;
}

}  // namespace

std::string_view PetTables::AccessoryActOf(std::uint32_t item_id) const {
  const auto found = accessory_act_by_item.find(item_id);
  return found != accessory_act_by_item.end() ? std::string_view(found->second)
                                              : std::string_view();
}

std::uint32_t PetTables::ClassOfEgg(std::uint32_t egg_id) const {
  const auto found = class_by_egg.find(egg_id);
  return found != class_by_egg.end() ? found->second : 0;
}

std::span<const std::uint32_t> PetTables::TamedBy(std::uint32_t item_id) const {
  const auto found = classes_by_tame_item.find(item_id);
  if (found == classes_by_tame_item.end()) {
    return {};
  }
  return found->second;
}

std::span<const std::uint32_t> PetTables::WornBy(std::uint32_t item_id) const {
  const auto found = classes_by_accessory.find(item_id);
  if (found == classes_by_accessory.end()) {
    return {};
  }
  return found->second;
}

std::vector<std::uint32_t> PetTables::FedBy(std::uint32_t item_id) const {
  std::vector<std::uint32_t> classes;
  for (const auto& [pet_class, food] : food_by_class) {
    if (food == item_id && item_id != 0) {
      classes.push_back(pet_class);
    }
  }
  std::ranges::sort(classes);
  return classes;
}

std::uint32_t PetTables::FoodOf(std::uint32_t pet_class) const {
  const auto found = food_by_class.find(pet_class);
  return found != food_by_class.end() ? found->second : 0;
}

std::string_view PetTables::SpeciesOf(std::uint32_t pet_class) const {
  const auto found = species_by_class.find(pet_class);
  return found != species_by_class.end() ? std::string_view(found->second) : std::string_view();
}

std::span<const PetEvolutionRecipe> PetTables::EvolutionsOf(std::uint32_t egg_id) const {
  const auto found = recipes_by_egg.find(egg_id);
  if (found == recipes_by_egg.end()) {
    return {};
  }
  return found->second;
}

const PetEvolutionRecipe* PetTables::RecipeOf(std::uint32_t source_egg_id,
                                              std::uint32_t target_egg_id) const {
  for (const PetEvolutionRecipe& recipe : EvolutionsOf(source_egg_id)) {
    if (recipe.target_egg_id == target_egg_id) {
      return &recipe;
    }
  }
  return nullptr;
}

core::Result<void> ParsePetInfo(std::string_view yaml_text, PetTables& tables) {
  // Tout se lit à part, et n'entre dans `tables` qu'une fois le texte accepté.
  PetTables read;
  // yaml-cpp signale par exception : elle ne franchit pas cette fonction.
  try {
    const YAML::Node whole = YAML::Load(std::string(yaml_text));
    if (!whole.IsMap()) {
      return std::unexpected(RefusedInfo("le texte n'est pas une table YAML"));
    }
    const YAML::Node version = whole["version"];
    if (!version || version.as<int>() != kSupportedVersion) {
      return std::unexpected(RefusedInfo("version non gérée"));
    }
    if (!ReadNameMap(whole["species"], read.species_by_class)) {
      return std::unexpected(RefusedInfo("« species » n'est pas une table"));
    }
    if (!ReadNameMap(whole["accessories"], read.accessory_act_by_item)) {
      return std::unexpected(RefusedInfo("« accessories » n'est pas une table"));
    }
  } catch (const YAML::Exception& failure) {
    return std::unexpected(RefusedInfo(failure.what()));
  }
  tables.species_by_class = std::move(read.species_by_class);
  tables.accessory_act_by_item = std::move(read.accessory_act_by_item);
  return {};
}

core::Result<void> ParsePetDatabase(std::string_view yaml_text, PetTables& tables) {
  // Tout se lit à part, et n'entre dans `tables` qu'une fois le texte accepté.
  PetTables read;
  // yaml-cpp signale par exception : elle ne franchit pas cette fonction.
  try {
    const YAML::Node whole = YAML::Load(std::string(yaml_text));
    if (!whole.IsMap()) {
      return std::unexpected(Refused("le texte n'est pas une table YAML"));
    }
    const YAML::Node version = whole["version"];
    if (!version || version.as<int>() != kSupportedVersion) {
      return std::unexpected(Refused("version non gérée"));
    }
    const YAML::Node pets = whole["pets"];
    if (!pets || !pets.IsMap()) {
      return std::unexpected(Refused("« pets » n'est pas une table"));
    }
    for (const auto& pair : pets) {
      const YAML::Node& pet = pair.second;
      if (!pet.IsMap()) {
        return std::unexpected(Refused("un familier n'est pas une table"));
      }
      const auto pet_class = pair.first.as<std::uint32_t>();
      const auto egg = Required(pet, "egg").as<std::uint32_t>();
      if (pet_class == 0 || egg == 0) {
        return std::unexpected(Refused("une classe ou un œuf nul"));
      }
      read.food_by_class[pet_class] = Required(pet, "food").as<std::uint32_t>();
      if (const auto tame = Required(pet, "tame").as<std::uint32_t>(); tame != 0) {
        read.classes_by_tame_item[tame].push_back(pet_class);
      }
      if (const auto accessory = Required(pet, "accessory").as<std::uint32_t>(); accessory != 0) {
        read.classes_by_accessory[accessory].push_back(pet_class);
      }
      // UN ŒUF PARTAGÉ sort la plus petite classe, quel que soit l'ordre du texte.
      const auto [known, inserted] = read.class_by_egg.try_emplace(egg, pet_class);
      if (!inserted && pet_class < known->second) {
        known->second = pet_class;
      }
      if (Required(pet, "auto_feed").as<bool>()) {
        read.auto_feed_eggs.insert(egg);
      }
      const YAML::Node evolutions = Required(pet, "evolutions");
      if (!evolutions.IsSequence()) {
        return std::unexpected(Refused("« evolutions » n'est pas une liste"));
      }
      for (const YAML::Node& evolution : evolutions) {
        read.recipes_by_egg[egg].push_back(ParseEvolution(evolution));
      }
    }
  } catch (const YAML::Exception& failure) {
    return std::unexpected(Refused(failure.what()));
  }
  tables.class_by_egg = std::move(read.class_by_egg);
  tables.food_by_class = std::move(read.food_by_class);
  // Les classes d'un même objet de capture ou d'un même accessoire,
  // croissantes, quel que soit l'ordre du texte.
  for (auto& [tame, classes] : read.classes_by_tame_item) {
    std::ranges::sort(classes);
  }
  for (auto& [accessory, classes] : read.classes_by_accessory) {
    std::ranges::sort(classes);
  }
  tables.classes_by_accessory = std::move(read.classes_by_accessory);
  tables.recipes_by_egg = std::move(read.recipes_by_egg);
  tables.auto_feed_eggs = std::move(read.auto_feed_eggs);
  tables.classes_by_tame_item = std::move(read.classes_by_tame_item);
  return {};
}

PetTables ReadPetTables(Vfs& vfs) {
  PetTables tables;
  if (auto info = vfs.Read(std::string(kPetInfoPath)); !info) {
    core::LogWarning("noms des familiers : {} absente — la fiche se montrera sans nom d'espèce",
                     kPetInfoPath);
  } else if (auto parsed = ParsePetInfo(
                 std::string_view(reinterpret_cast<const char*>(info->data()), info->size()),
                 tables);
             !parsed) {
    core::LogWarning("{} — la fiche se montrera sans nom d'espèce", core::Describe(parsed.error()));
  }
  // ⚠ LES DEUX SOURCES ÉCHOUENT SÉPARÉMENT : une table du serveur illisible ne
  // coûte pas les espèces ni les accessoires.
  auto bytes = vfs.Read(std::string(kPetDatabasePath));
  if (!bytes) {
    core::LogWarning("table des familiers : {} absente — ni nourriture ni évolution",
                     kPetDatabasePath);
    return tables;
  }
  const std::string_view text(reinterpret_cast<const char*>(bytes->data()), bytes->size());
  if (auto parsed = ParsePetDatabase(text, tables); !parsed) {
    core::LogWarning("{} — ni nourriture ni évolution", core::Describe(parsed.error()));
  }
  return tables;
}

}  // namespace ro::vfs
