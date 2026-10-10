#include "ro/vfs/item_stacking.h"

#include <cstdint>
#include <expected>
#include <string>
#include <utility>

#include <yaml-cpp/yaml.h>

namespace ro::vfs {
namespace {

constexpr int kSupportedVersion = 1;

core::Error Refused(std::string detail) {
  return core::Error{core::ErrorCode::kCorruptData, "items non empilables : " + std::move(detail)};
}

}  // namespace

ItemStacking ItemStacking::Known(std::unordered_set<std::uint32_t> non_stackable_ids) {
  ItemStacking stacking;
  stacking.known_ = true;
  stacking.non_stackable_ids_ = std::move(non_stackable_ids);
  return stacking;
}

std::optional<bool> ItemStacking::IsStackable(std::uint32_t item_id) const {
  if (!known_) {
    return std::nullopt;
  }
  return !non_stackable_ids_.contains(item_id);
}

core::Result<ItemStacking> ParseItemStacking(std::string_view yaml_text) {
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
    const YAML::Node listed = whole["non_stackable"];
    if (!listed || !listed.IsSequence()) {
      return std::unexpected(Refused("« non_stackable » n'est pas une liste"));
    }
    std::unordered_set<std::uint32_t> ids;
    ids.reserve(listed.size());
    for (const YAML::Node& entry : listed) {
      ids.insert(entry.as<std::uint32_t>());
    }
    return ItemStacking::Known(std::move(ids));
  } catch (const YAML::Exception& failure) {
    return std::unexpected(Refused(failure.what()));
  }
}

core::Result<ItemStacking> ReadItemStacking(Vfs& vfs) {
  auto bytes = vfs.Read(std::string(kItemStackingPath));
  if (!bytes) {
    if (bytes.error().code == core::ErrorCode::kNotFound) {
      return ItemStacking{};
    }
    return std::unexpected(core::Error{
        bytes.error().code, std::string(kItemStackingPath) + " : " + bytes.error().detail});
  }
  const std::string_view text(reinterpret_cast<const char*>(bytes->data()), bytes->size());
  return ParseItemStacking(text);
}

}  // namespace ro::vfs
