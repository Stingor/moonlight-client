#include "world/closed_maps.h"

#include <expected>
#include <string>
#include <utility>

#include <yaml-cpp/yaml.h>

#include "ro/core/log.h"

namespace moonclient {
namespace {

constexpr int kSupportedVersion = 1;

ro::core::Error Refused(std::string detail) {
  return ro::core::Error{ro::core::ErrorCode::kCorruptData,
                         std::string(kClosedMapsPath) + " : " + std::move(detail)};
}

}  // namespace

ro::core::Result<ClosedMaps> ClosedMaps::Parse(std::string_view yaml_text) {
  ClosedMaps table;
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
    const YAML::Node maps = whole["maps"];
    if (!maps || !maps.IsSequence()) {
      return std::unexpected(Refused("« maps » n'est pas une liste"));
    }
    for (const YAML::Node& map : maps) {
      if (!map.IsScalar()) {
        return std::unexpected(Refused("une entrée de « maps » n'est pas un nom de carte"));
      }
      table.maps_.insert(map.as<std::string>());
    }
  } catch (const YAML::Exception& failure) {
    return std::unexpected(Refused(failure.what()));
  }
  return table;
}

bool ClosedMaps::Closes(std::string_view map_name) const { return maps_.contains(map_name); }

ClosedMaps LoadClosedMaps(ro::vfs::Vfs& vfs) {
  auto bytes = vfs.Read(kClosedMapsPath);
  if (!bytes) {
    ro::core::LogWarning("{} absente — le cycle jour/nuit s'applique à toute carte en plein air",
                         kClosedMapsPath);
    return {};
  }
  auto table = ClosedMaps::Parse(
      std::string_view(reinterpret_cast<const char*>(bytes->data()), bytes->size()));
  if (!table) {
    ro::core::LogWarning("{} — le cycle jour/nuit s'applique à toute carte en plein air",
                         ro::core::Describe(table.error()));
    return {};
  }
  ro::core::LogInfo("ciel : {} carte(s) fermée(s) au cycle jour/nuit", table->size());
  return std::move(*table);
}

}  // namespace moonclient
