// Le service de téléportation des Kafra. Voir l'en-tête.

#include "ro/vfs/kafra_move_maps.h"

#include <expected>
#include <utility>

#include <yaml-cpp/yaml.h>

#include "ro/core/ascii.h"
#include "ro/vfs/vfs.h"

namespace ro::vfs {
namespace {

constexpr int kSupportedVersion = 1;

core::Error Refused(std::string detail) {
  return core::Error{core::ErrorCode::kCorruptData,
                     "destinations des Kafra : " + std::move(detail)};
}

}  // namespace

core::Result<KafraMoveMaps> KafraMoveMaps::Load(Vfs& vfs) {
  auto bytes = vfs.Read(std::string(kKafraMoveMapsPath));
  if (!bytes) {
    return std::unexpected(core::Error{
        bytes.error().code, std::string(kKafraMoveMapsPath) + " : " + bytes.error().detail});
  }
  const std::string_view text(reinterpret_cast<const char*>(bytes->data()), bytes->size());
  return Parse(text);
}

core::Result<KafraMoveMaps> KafraMoveMaps::Parse(std::string_view yaml_text) {
  KafraMoveMaps maps;
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
    const YAML::Node towns = whole["towns"];
    if (!towns || !towns.IsMap()) {
      return std::unexpected(Refused("« towns » n'est pas une table"));
    }
    for (const auto& pair : towns) {
      const std::string town = core::AsciiLower(pair.first.as<std::string>());
      if (!pair.second.IsSequence()) {
        return std::unexpected(Refused("les destinations de " + town + " ne sont pas une liste"));
      }
      std::vector<std::string> destinations;
      for (const YAML::Node& destination : pair.second) {
        if (!destination.IsScalar()) {
          return std::unexpected(Refused("une destination de " + town + " n'est pas un nom"));
        }
        destinations.push_back(core::AsciiLower(destination.as<std::string>()));
      }
      maps.destinations_.insert_or_assign(town, std::move(destinations));
    }
  } catch (const YAML::Exception& failure) {
    return std::unexpected(Refused(failure.what()));
  }
  return maps;
}

std::span<const std::string> KafraMoveMaps::DestinationsOf(std::string_view town) const {
  const auto found = destinations_.find(town);
  return found == destinations_.end() ? std::span<const std::string>() : found->second;
}

}  // namespace ro::vfs
