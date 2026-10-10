// Voir l'en-tête.

#include "ro/vfs/pet_tables.h"

#include <array>
#include <limits>
#include <optional>

#include "ro/core/log.h"
#include "ro/formats/lua_calls.h"
#include "ro/vfs/encoding.h"
#include "ro/vfs/lua_chain.h"

namespace ro::vfs {
namespace {

using formats::LuaValue;

constexpr std::string_view kEggClassGlobal = "PetEggItemID_PetJobID";
constexpr std::string_view kFoodGlobal = "PetFoodTable";
constexpr std::string_view kSpeciesGlobal = "PetStringTable";
// Les .act d'accessoire : la table coréenne fait foi, l'anglaise complète.
constexpr std::array<std::string_view, 2> kAccessoryActGlobals = {"PetAccActNameTable",
                                                                  "PetAccActNameTable_Eng"};

constexpr std::string_view kEvolutionFunction = "main";
constexpr std::string_view kRecipeCall = "InsertEvolutionRecipeLGU";
constexpr std::string_view kAutoFeedCall = "InsertPetAutoFeeding";
// Œuf source, œuf cible, item, quantité.
constexpr std::size_t kRecipeArguments = 4;

constexpr std::uint32_t kAnyId = std::numeric_limits<std::uint32_t>::max();

[[nodiscard]] std::optional<std::uint32_t> WholeId(const LuaValue& value) {
  if (value.kind != LuaValue::Kind::kNumber) {
    return std::nullopt;
  }
  return formats::LuaWholeNumber(value.number, kAnyId);
}

[[nodiscard]] const formats::LuaTable* TableNamed(const formats::LuaTable& globals,
                                                  std::string_view name) {
  const LuaValue* value = globals.Find(name);
  if (value == nullptr || value->kind != LuaValue::Kind::kTable || !value->table) {
    return nullptr;
  }
  return value->table.get();
}

// Une table `[identifiant] = identifiant`, entrées illisibles sautées.
void FillIdMap(const formats::LuaTable* table,
               std::unordered_map<std::uint32_t, std::uint32_t>& into) {
  if (table == nullptr) {
    return;
  }
  for (const auto& [key, value] : table->by_index) {
    const std::optional<std::uint32_t> from = formats::LuaWholeNumber(key, kAnyId);
    const std::optional<std::uint32_t> to = WholeId(value);
    if (from.has_value() && to.has_value()) {
      into[*from] = *to;
    }
  }
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
  FillIdMap(TableNamed(globals, kEggClassGlobal), tables.class_by_egg);
  FillIdMap(TableNamed(globals, kFoodGlobal), tables.food_by_class);
  FillNameMap(TableNamed(globals, kSpeciesGlobal), tables.species_by_class);
  for (const std::string_view global : kAccessoryActGlobals) {
    FillNameMap(TableNamed(globals, global), tables.accessory_act_by_item);
  }
}

void FillPetEvolutions(std::span<const formats::LuaCall> calls, PetTables& tables) {
  for (const formats::LuaCall& call : calls) {
    if (call.name == kAutoFeedCall && call.arguments.size() == 1) {
      if (const std::optional<std::uint32_t> egg = WholeId(call.arguments[0]); egg.has_value()) {
        tables.auto_feed_eggs.insert(*egg);
      }
      continue;
    }
    if (call.name != kRecipeCall || call.arguments.size() != kRecipeArguments) {
      continue;
    }
    const std::optional<std::uint32_t> source = WholeId(call.arguments[0]);
    const std::optional<std::uint32_t> target = WholeId(call.arguments[1]);
    const std::optional<std::uint32_t> item = WholeId(call.arguments[2]);
    const std::optional<std::uint32_t> amount = WholeId(call.arguments[3]);
    // Une quantité nulle est refusée par le client, comme un argument non entier.
    if (!source.has_value() || !target.has_value() || !item.has_value() || !amount.has_value() ||
        *amount == 0) {
      continue;
    }
    // UNE RECETTE PAR APPEL ET PAR MATÉRIAU : les appels d'un même couple
    // s'additionnent sous la même recette.
    std::vector<PetEvolutionRecipe>& recipes = tables.recipes_by_egg[*source];
    PetEvolutionRecipe* recipe = nullptr;
    for (PetEvolutionRecipe& known : recipes) {
      if (known.target_egg_id == *target) {
        recipe = &known;
      }
    }
    if (recipe == nullptr) {
      recipe = &recipes.emplace_back(PetEvolutionRecipe{.target_egg_id = *target, .materials = {}});
    }
    recipe->materials.push_back(PetEvolutionMaterial{.item_id = *item, .amount = *amount});
  }
}

PetTables ReadPetTables(Vfs& vfs) {
  PetTables tables;
  auto globals = ReadLuaChain(vfs, kPetInfoChain);
  if (globals) {
    FillPetInfo(*globals, tables);
  } else {
    core::LogWarning("tables du familier : {} — la fiche se montrera sans espèce ni nourriture",
                     core::Describe(globals.error()));
  }
  // ⚠ LES DEUX FICHIERS ÉCHOUENT SÉPARÉMENT : une table d'évolution illisible
  // ne coûte pas les espèces ni la nourriture.
  auto bytes = vfs.Read(kPetEvolutionPath);
  if (!bytes) {
    core::LogWarning("évolutions du familier : {} absent — ni évolution ni auto-alimentation",
                     kPetEvolutionPath);
    return tables;
  }
  auto calls = formats::ReadLuaFunctionCalls(*bytes, kEvolutionFunction);
  if (!calls) {
    core::LogWarning(
        "évolutions du familier : {} illisible ({}) — ni évolution ni "
        "auto-alimentation",
        kPetEvolutionPath, core::Describe(calls.error()));
    return tables;
  }
  FillPetEvolutions(*calls, tables);
  return tables;
}

}  // namespace ro::vfs
