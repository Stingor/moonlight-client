// Les Warp Agent du serveur. Voir l'en-tête.

#include "ro/vfs/warp_agents.h"

#include <expected>
#include <utility>

#include <yaml-cpp/yaml.h>

#include "ro/core/ascii.h"
#include "ro/vfs/vfs.h"

namespace ro::vfs {
namespace {

constexpr int kSupportedVersion = 1;

core::Error Refused(std::string detail) {
  return core::Error{core::ErrorCode::kCorruptData, "Warp Agent : " + std::move(detail)};
}

}  // namespace

core::Result<WarpAgents> WarpAgents::Load(Vfs& vfs) {
  auto bytes = vfs.Read(std::string(kWarpAgentsPath));
  if (!bytes) {
    return std::unexpected(core::Error{
        bytes.error().code, std::string(kWarpAgentsPath) + " : " + bytes.error().detail});
  }
  const std::string_view text(reinterpret_cast<const char*>(bytes->data()), bytes->size());
  return Parse(text);
}

core::Result<WarpAgents> WarpAgents::Parse(std::string_view yaml_text) {
  WarpAgents agents;
  // yaml-cpp signale par exception : elle ne franchit pas cette fonction. Une
  // classe négative ou non entière en est une.
  try {
    const YAML::Node whole = YAML::Load(std::string(yaml_text));
    if (!whole.IsMap()) {
      return std::unexpected(Refused("le texte n'est pas une table YAML"));
    }
    const YAML::Node version = whole["version"];
    if (!version || version.as<int>() != kSupportedVersion) {
      return std::unexpected(Refused("version non gérée"));
    }
    const YAML::Node maps = whole["agents"];
    if (!maps || !maps.IsMap()) {
      return std::unexpected(Refused("« agents » n'est pas une table"));
    }
    for (const auto& pair : maps) {
      const std::string map_name = core::AsciiLower(pair.first.as<std::string>());
      if (!pair.second.IsMap()) {
        return std::unexpected(Refused("le Warp Agent de " + map_name + " n'est pas une table"));
      }
      const YAML::Node job = pair.second["class"];
      if (!job || !job.IsScalar()) {
        return std::unexpected(Refused("le Warp Agent de " + map_name + " n'a pas de classe"));
      }
      agents.classes_.insert_or_assign(map_name, job.as<std::uint32_t>());
    }
  } catch (const YAML::Exception& failure) {
    return std::unexpected(Refused(failure.what()));
  }
  return agents;
}

std::optional<std::uint32_t> WarpAgents::ClassAt(std::string_view map_name) const {
  const auto found = classes_.find(map_name);
  if (found == classes_.end()) {
    return std::nullopt;
  }
  return found->second;
}

}  // namespace ro::vfs
