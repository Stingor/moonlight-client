#include "items/laphine_book.h"

#include <algorithm>
#include <cstdint>
#include <exception>
#include <string>
#include <utility>

#include <yaml-cpp/yaml.h>

#include "ro/core/log.h"

namespace moonclient {
namespace {

constexpr int kSupportedVersion = 1;

constexpr const char* kVersionField = "version";
constexpr const char* kSynthesisSection = "synthesis";
constexpr const char* kUpgradeSection = "upgrade";
constexpr const char* kItemField = "item";
constexpr const char* kNeededField = "needed";
constexpr const char* kLeastRefineField = "least_refine";
constexpr const char* kMostRefineField = "most_refine";
constexpr const char* kLeastOptionsField = "least_options";
constexpr const char* kCardsField = "cards_allowed";
constexpr const char* kMaterialsField = "materials";
constexpr const char* kTargetsField = "targets";

// Une paire `[objet, quantité]` compte deux nœuds.
constexpr std::size_t kMaterialPairSize = 2;

[[nodiscard]] ro::core::Error Refused(std::string detail) {
  return ro::core::Error{ro::core::ErrorCode::kCorruptData, "laphine : " + std::move(detail)};
}

[[nodiscard]] ro::sim::LaphineRecipe CommonOf(const YAML::Node& entry) {
  return ro::sim::LaphineRecipe{
      .trigger_item_id = entry[kItemField].as<std::uint32_t>(0),
      .least_refine = static_cast<std::uint8_t>(entry[kLeastRefineField].as<unsigned>(0)),
      .most_refine = static_cast<std::uint8_t>(entry[kMostRefineField].as<unsigned>(0))};
}

void ReadSynthesis(const YAML::Node& section, std::vector<ro::sim::LaphineRecipe>& recipes) {
  if (!section || !section.IsSequence()) {
    return;
  }
  for (const YAML::Node& entry : section) {
    ro::sim::LaphineRecipe recipe = CommonOf(entry);
    recipe.needed_piles = entry[kNeededField].as<std::uint16_t>(1);
    if (const YAML::Node materials = entry[kMaterialsField]; materials && materials.IsSequence()) {
      for (const YAML::Node& pair : materials) {
        if (pair.IsSequence() && pair.size() == kMaterialPairSize) {
          recipe.materials.push_back(ro::sim::LaphineMaterial{
              .item_id = pair[0].as<std::uint32_t>(0), .amount = pair[1].as<std::uint16_t>(1)});
        }
      }
    }
    recipes.push_back(std::move(recipe));
  }
}

void ReadUpgrade(const YAML::Node& section, std::vector<ro::sim::LaphineRecipe>& recipes) {
  if (!section || !section.IsSequence()) {
    return;
  }
  for (const YAML::Node& entry : section) {
    ro::sim::LaphineRecipe recipe = CommonOf(entry);
    recipe.least_options = static_cast<std::uint8_t>(entry[kLeastOptionsField].as<unsigned>(0));
    recipe.cards_allowed = entry[kCardsField].as<bool>(false);
    if (const YAML::Node targets = entry[kTargetsField]; targets && targets.IsSequence()) {
      for (const YAML::Node& target : targets) {
        recipe.materials.push_back(
            ro::sim::LaphineMaterial{.item_id = target.as<std::uint32_t>(0)});
      }
    }
    recipes.push_back(std::move(recipe));
  }
}

}  // namespace

ro::core::Result<LaphineBook> LaphineBook::Parse(std::string_view yaml_text) {
  // yaml-cpp signale par exception : elle ne franchit pas cette fonction.
  try {
    const YAML::Node root = YAML::Load(std::string(yaml_text));
    if (!root.IsMap()) {
      return std::unexpected(Refused("racine qui n'est pas une table"));
    }
    if (root[kVersionField].as<int>(0) != kSupportedVersion) {
      return std::unexpected(Refused("version inconnue"));
    }
    LaphineBook book;
    ReadSynthesis(root[kSynthesisSection], book.synthesis_);
    ReadUpgrade(root[kUpgradeSection], book.upgrade_);
    return book;
  } catch (const std::exception& trouble) {
    return std::unexpected(Refused(std::string("YAML illisible — ") + trouble.what()));
  }
}

const ro::sim::LaphineRecipe* LaphineBook::Find(ro::sim::LaphineKind kind,
                                                std::uint32_t trigger_item_id) const noexcept {
  const std::vector<ro::sim::LaphineRecipe>& recipes =
      kind == ro::sim::LaphineKind::kSynthesis ? synthesis_ : upgrade_;
  const auto found = std::ranges::find_if(recipes, [&](const ro::sim::LaphineRecipe& recipe) {
    return recipe.trigger_item_id == trigger_item_id;
  });
  return found == recipes.end() ? nullptr : &*found;
}

bool LaphineBook::Triggers(std::uint32_t item_id) const noexcept {
  return Find(ro::sim::LaphineKind::kSynthesis, item_id) != nullptr ||
         Find(ro::sim::LaphineKind::kUpgrade, item_id) != nullptr;
}

LaphineBook ReadLaphineBook(ro::vfs::Vfs& vfs) {
  auto bytes = vfs.Read(std::string(kLaphineBookPath));
  if (!bytes) {
    ro::core::LogWarning("laphine : {} — ses fenêtres se refermeront aussitôt",
                         ro::core::Describe(bytes.error()));
    return {};
  }
  const std::string_view text(reinterpret_cast<const char*>(bytes->data()), bytes->size());
  ro::core::Result<LaphineBook> book = LaphineBook::Parse(text);
  if (!book) {
    ro::core::LogWarning("laphine : {} — ses fenêtres se refermeront aussitôt",
                         ro::core::Describe(book.error()));
    return {};
  }
  return std::move(*book);
}

}  // namespace moonclient
