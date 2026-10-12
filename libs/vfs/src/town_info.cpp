// Les commodités de chaque ville. Voir l'en-tête.

#include "ro/vfs/town_info.h"

#include <algorithm>
#include <expected>
#include <optional>
#include <string>
#include <utility>

#include <yaml-cpp/yaml.h>

#include "ro/core/ascii.h"
#include "ro/vfs/vfs.h"

namespace ro::vfs {
namespace {

constexpr int kSupportedVersion = 1;
// La plus grande coordonnée qu'une carte puisse porter.
constexpr std::int32_t kLargestCell = 1023;

core::Error Refused(std::string detail) {
  return core::Error{core::ErrorCode::kCorruptData,
                     std::string(kTownInfoPath) + " : " + std::move(detail)};
}

[[nodiscard]] std::optional<TownFacility> FacilityNamed(std::string_view name) {
  const auto found = std::ranges::find(kTownFacilityNames, name);
  if (found == kTownFacilityNames.end()) return std::nullopt;
  return static_cast<TownFacility>(found - kTownFacilityNames.begin());
}

// Une commodité. Une exception de yaml-cpp, ou un refus.
[[nodiscard]] core::Result<TownFacilityEntry> EntryOf(const YAML::Node& node,
                                                      const std::string& map_name) {
  if (!node.IsMap())
    return std::unexpected(Refused(map_name + " : une commodité n'est pas une table"));
  TownFacilityEntry entry;
  if (const YAML::Node name = node["name"]; name) entry.name = name.as<std::string>();
  const YAML::Node x = node["x"];
  const YAML::Node y = node["y"];
  const YAML::Node facility = node["facility"];
  if (!x || !y || !facility)
    return std::unexpected(Refused(map_name + " : une commodité sans case ni nature"));
  entry.cell_x = x.as<std::int32_t>();
  entry.cell_y = y.as<std::int32_t>();
  if (entry.cell_x < 0 || entry.cell_x > kLargestCell || entry.cell_y < 0 ||
      entry.cell_y > kLargestCell)
    return std::unexpected(Refused(map_name + " : une case hors de la carte"));
  const std::optional<TownFacility> nature = FacilityNamed(facility.as<std::string>());
  if (!nature)
    return std::unexpected(Refused(map_name + " : nature inconnue " + facility.as<std::string>()));
  entry.facility = *nature;
  return entry;
}

}  // namespace

core::Result<TownInfo> TownInfo::Parse(std::string_view yaml_text) {
  TownInfo info;
  // yaml-cpp signale par exception : elle ne franchit pas cette fonction.
  try {
    const YAML::Node whole = YAML::Load(std::string(yaml_text));
    if (!whole.IsMap()) return std::unexpected(Refused("le texte n'est pas une table YAML"));
    const YAML::Node version = whole["version"];
    if (!version || version.as<int>() != kSupportedVersion)
      return std::unexpected(Refused("version non gérée"));
    const YAML::Node towns = whole["towns"];
    if (!towns || !towns.IsMap()) return std::unexpected(Refused("« towns » n'est pas une table"));
    for (const auto& pair : towns) {
      const auto map_name = pair.first.as<std::string>();
      if (!pair.second.IsSequence())
        return std::unexpected(Refused(map_name + " n'est pas une liste"));
      std::vector<TownFacilityEntry> entries;
      for (const YAML::Node& node : pair.second) {
        core::Result<TownFacilityEntry> entry = EntryOf(node, map_name);
        if (!entry) return std::unexpected(entry.error());
        entries.push_back(std::move(*entry));
      }
      info.by_map_[core::AsciiLower(map_name)] = std::move(entries);
    }
  } catch (const YAML::Exception& failure) {
    return std::unexpected(Refused(failure.what()));
  }
  return info;
}

core::Result<TownInfo> TownInfo::Load(Vfs& vfs) {
  auto bytes = vfs.Read(kTownInfoPath);
  if (!bytes) return std::unexpected(bytes.error());
  return Parse(std::string_view(reinterpret_cast<const char*>(bytes->data()), bytes->size()));
}

std::span<const TownFacilityEntry> TownInfo::On(std::string_view map_name) const {
  const auto found = by_map_.find(core::AsciiLower(map_name));
  if (found == by_map_.end()) return {};
  return found->second;
}

}  // namespace ro::vfs
