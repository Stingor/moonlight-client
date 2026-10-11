// Voir l'en-tête.

#include "ro/vfs/pet_tables.h"

#include <array>
#include <expected>
#include <limits>
#include <optional>
#include <utility>

#include <yaml-cpp/yaml.h>

#include "ro/core/log.h"
#include "ro/vfs/encoding.h"
#include "ro/vfs/lua_chain.h"

namespace ro::vfs {
namespace {

using formats::LuaValue;

constexpr std::string_view kSpeciesGlobal = "PetStringTable";
// Les .act d'accessoire : la table coréenne fait foi, l'anglaise complète.
constexpr std::array<std::string_view, 2> kAccessoryActGlobals = {"PetAccActNameTable",
                                                                  "PetAccActNameTable_Eng"};

constexpr int kSupportedVersion = 1;
// Un matériau s'écrit [item, quantité].
constexpr std::size_t kMaterialItemField = 0;
constexpr std::size_t kMaterialAmountField = 1;
constexpr std::size_t kMaterialFieldCount = 2;

constexpr std::uint32_t kAnyId = std::numeric_limits<std::uint32_t>::max();

[[nodiscard]] const formats::LuaTable* TableNamed(const formats::LuaTable& globals,
                                                  std::string_view name) {
  const LuaValue* value = globals.Find(name);
  if (value == nullptr || value->kind != LuaValue::Kind::kTable || !value->table) {
    return nullptr;
  }
  return value->table.get();
}

// Une table `[identifiant] = "nom"`, dans la page de codes du client. Un
// identifiant déjà connu GARDE son nom : la première table lue fait foi.
void FillNameMap(const formats::LuaTable* table,
                 std::unordered_map<std::uint32_t, std::string>& into) {
  if (table == nullptr) {
    return;
  }
  for (const auto& [key, value] : table->by_index) {
    const std::optional<std::uint32_t> id = formats::LuaWholeNumber(key, kAnyId);
    if (id.has_value() && value.kind == LuaValue::Kind::kString) {
      into.try_emplace(*id, Cp949ToUtf8Lossy(value.text));
    }
  }
}

core::Error Refused(std::string detail) {
  return core::Error{core::ErrorCode::kCorruptData, "table des familiers : " + std::move(detail)};
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

void FillPetInfo(const formats::LuaTable& globals, PetTables& tables) {
  FillNameMap(TableNamed(globals, kSpeciesGlobal), tables.species_by_class);
  for (const std::string_view global : kAccessoryActGlobals) {
    FillNameMap(TableNamed(globals, global), tables.accessory_act_by_item);
  }
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
  tables.recipes_by_egg = std::move(read.recipes_by_egg);
  tables.auto_feed_eggs = std::move(read.auto_feed_eggs);
  return {};
}

PetTables ReadPetTables(Vfs& vfs) {
  PetTables tables;
  auto globals = ReadLuaChain(vfs, kPetInfoChain);
  if (globals) {
    FillPetInfo(*globals, tables);
  } else {
    core::LogWarning("tables du familier : {} — la fiche se montrera sans nom d'espèce",
                     core::Describe(globals.error()));
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
