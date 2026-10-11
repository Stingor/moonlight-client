#include "ro/vfs/item_properties.h"

#include <cstdint>
#include <expected>
#include <string>
#include <utility>

#include <yaml-cpp/yaml.h>

namespace ro::vfs {
namespace {

constexpr int kSupportedVersion = 1;

core::Error Refused(std::string detail) {
  return core::Error{core::ErrorCode::kCorruptData, "propriétés d'item : " + std::move(detail)};
}

// Une liste d'identifiants, ou vide si le nœud n'en est pas une.
std::optional<std::unordered_set<std::uint32_t>> IdsOf(const YAML::Node& listed) {
  if (!listed || !listed.IsSequence()) {
    return std::nullopt;
  }
  std::unordered_set<std::uint32_t> ids;
  ids.reserve(listed.size());
  for (const YAML::Node& entry : listed) {
    ids.insert(entry.as<std::uint32_t>());
  }
  return ids;
}

}  // namespace

ItemProperties ItemProperties::Known(
    std::unordered_set<std::uint32_t> non_stackable_ids,
    std::unordered_set<std::uint32_t> buying_store_ids,
    std::unordered_map<std::uint32_t, std::uint32_t> weight_tenths_by_id) {
  ItemProperties properties;
  properties.known_ = true;
  properties.stacking_ = ItemStacking::Known(std::move(non_stackable_ids));
  properties.buying_store_ids_ = std::move(buying_store_ids);
  properties.weight_tenths_by_id_ = std::move(weight_tenths_by_id);
  return properties;
}

ItemProperties ItemProperties::StackingOnly(ItemStacking stacking) {
  ItemProperties properties;
  properties.stacking_ = std::move(stacking);
  return properties;
}

std::optional<bool> ItemProperties::BuyingStoreAllowed(std::uint32_t item_id) const {
  if (!known_) {
    return std::nullopt;
  }
  return buying_store_ids_.contains(item_id);
}

std::optional<std::uint32_t> ItemProperties::WeightTenthsOf(std::uint32_t item_id) const {
  if (!known_) {
    return std::nullopt;
  }
  const auto found = weight_tenths_by_id_.find(item_id);
  return found == weight_tenths_by_id_.end() ? 0U : found->second;
}

core::Result<ItemProperties> ParseItemProperties(std::string_view yaml_text) {
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
    auto non_stackable = IdsOf(whole["non_stackable"]);
    if (!non_stackable) {
      return std::unexpected(Refused("« non_stackable » n'est pas une liste"));
    }
    auto buying_store = IdsOf(whole["buying_store"]);
    if (!buying_store) {
      return std::unexpected(Refused("« buying_store » n'est pas une liste"));
    }
    const YAML::Node weights = whole["weight"];
    if (!weights || !weights.IsMap()) {
      return std::unexpected(Refused("« weight » n'est pas une table"));
    }
    std::unordered_map<std::uint32_t, std::uint32_t> weight_tenths_by_id;
    weight_tenths_by_id.reserve(weights.size());
    for (const auto& pair : weights) {
      weight_tenths_by_id[pair.first.as<std::uint32_t>()] = pair.second.as<std::uint32_t>();
    }
    return ItemProperties::Known(std::move(*non_stackable), std::move(*buying_store),
                                 std::move(weight_tenths_by_id));
  } catch (const YAML::Exception& failure) {
    return std::unexpected(Refused(failure.what()));
  }
}

core::Result<ItemProperties> ReadItemProperties(Vfs& vfs) {
  auto bytes = vfs.Read(std::string(kItemPropertiesPath));
  if (!bytes) {
    if (bytes.error().code != core::ErrorCode::kNotFound) {
      return std::unexpected(core::Error{
          bytes.error().code, std::string(kItemPropertiesPath) + " : " + bytes.error().detail});
    }
    auto stacking = ReadItemStacking(vfs);
    if (!stacking) {
      return std::unexpected(stacking.error());
    }
    return ItemProperties::StackingOnly(std::move(*stacking));
  }
  const std::string_view text(reinterpret_cast<const char*>(bytes->data()), bytes->size());
  return ParseItemProperties(text);
}

}  // namespace ro::vfs
