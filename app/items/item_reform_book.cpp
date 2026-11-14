#include "items/item_reform_book.h"

#include <algorithm>
#include <exception>
#include <string>
#include <utility>

#include <yaml-cpp/yaml.h>

#include "ro/core/log.h"

namespace moonclient {
namespace {

constexpr int kSupportedVersion = 1;

constexpr const char* kVersionField = "version";
constexpr const char* kRefineCapField = "refine_cap";
constexpr const char* kTriggersSection = "triggers";
constexpr const char* kItemField = "item";
constexpr const char* kRecipesField = "recipes";
constexpr const char* kBaseField = "base";
constexpr const char* kResultField = "result";
constexpr const char* kLeastRefineField = "least_refine";
constexpr const char* kMostRefineField = "most_refine";
constexpr const char* kLeastOptionsField = "least_options";
constexpr const char* kCardsField = "cards_allowed";
constexpr const char* kRefineChangeField = "refine_change";
constexpr const char* kMaterialsField = "materials";

// Une paire `[objet, quantité]` compte deux nœuds.
constexpr std::size_t kMaterialPairSize = 2;

[[nodiscard]] ro::core::Error Refused(std::string detail) {
  return ro::core::Error{ro::core::ErrorCode::kCorruptData, "item_reform : " + std::move(detail)};
}

[[nodiscard]] ro::sim::ItemReformRecipe RecipeOf(const YAML::Node& entry) {
  ro::sim::ItemReformRecipe recipe{
      .base_item_id = entry[kBaseField].as<std::uint32_t>(0),
      .result_item_id = entry[kResultField].as<std::uint32_t>(0),
      .least_refine = static_cast<std::uint8_t>(entry[kLeastRefineField].as<unsigned>(0)),
      .most_refine = static_cast<std::uint8_t>(entry[kMostRefineField].as<unsigned>(0)),
      .least_options = static_cast<std::uint8_t>(entry[kLeastOptionsField].as<unsigned>(0)),
      .cards_allowed = entry[kCardsField].as<bool>(true),
      .refine_change = static_cast<std::int8_t>(entry[kRefineChangeField].as<int>(0))};
  if (const YAML::Node materials = entry[kMaterialsField]; materials && materials.IsSequence()) {
    for (const YAML::Node& pair : materials) {
      if (pair.IsSequence() && pair.size() == kMaterialPairSize) {
        recipe.materials.push_back(ro::sim::ItemReformMaterial{
            .item_id = pair[0].as<std::uint32_t>(0), .amount = pair[1].as<std::uint16_t>(1)});
      }
    }
  }
  return recipe;
}

}  // namespace

ro::core::Result<ItemReformBook> ItemReformBook::Parse(std::string_view yaml_text) {
  // yaml-cpp signale par exception : elle ne franchit pas cette fonction.
  try {
    const YAML::Node root = YAML::Load(std::string(yaml_text));
    if (!root.IsMap()) {
      return std::unexpected(Refused("racine qui n'est pas une table"));
    }
    if (root[kVersionField].as<int>(0) != kSupportedVersion) {
      return std::unexpected(Refused("version inconnue"));
    }
    const auto refine_cap = static_cast<std::uint8_t>(root[kRefineCapField].as<unsigned>(0));
    ItemReformBook book;
    const YAML::Node section = root[kTriggersSection];
    if (!section || !section.IsSequence()) {
      return book;
    }
    for (const YAML::Node& entry : section) {
      ro::sim::ItemReformTrigger trigger{.trigger_item_id = entry[kItemField].as<std::uint32_t>(0),
                                         .refine_cap = refine_cap};
      if (const YAML::Node recipes = entry[kRecipesField]; recipes && recipes.IsSequence()) {
        for (const YAML::Node& recipe : recipes) {
          trigger.recipes.push_back(RecipeOf(recipe));
        }
      }
      book.triggers_.push_back(std::move(trigger));
    }
    return book;
  } catch (const std::exception& trouble) {
    return std::unexpected(Refused(std::string("YAML illisible — ") + trouble.what()));
  }
}

const ro::sim::ItemReformTrigger* ItemReformBook::Find(
    std::uint32_t trigger_item_id) const noexcept {
  const auto found =
      std::ranges::find_if(triggers_, [trigger_item_id](const ro::sim::ItemReformTrigger& entry) {
        return entry.trigger_item_id == trigger_item_id;
      });
  return found == triggers_.end() ? nullptr : &*found;
}

ItemReformBook ReadItemReformBook(ro::vfs::Vfs& vfs) {
  auto bytes = vfs.Read(std::string(kItemReformBookPath));
  if (!bytes) {
    ro::core::LogWarning("item_reform : {} — ses fenêtres se refermeront aussitôt",
                         ro::core::Describe(bytes.error()));
    return {};
  }
  const std::string_view text(reinterpret_cast<const char*>(bytes->data()), bytes->size());
  ro::core::Result<ItemReformBook> book = ItemReformBook::Parse(text);
  if (!book) {
    ro::core::LogWarning("item_reform : {} — ses fenêtres se refermeront aussitôt",
                         ro::core::Describe(book.error()));
    return {};
  }
  return std::move(*book);
}

}  // namespace moonclient
