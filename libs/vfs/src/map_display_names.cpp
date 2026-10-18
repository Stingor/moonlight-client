// Le nom lisible de chaque carte. Voir l'en-tête.

#include "ro/vfs/map_display_names.h"

#include <expected>
#include <optional>
#include <string>
#include <utility>

#include <yaml-cpp/yaml.h>

#include "yaml_required.h"

namespace ro::vfs {
namespace {

constexpr int kSupportedVersion = 1;

// La retouche de l'extension : un point suivi de trois caractères, sur un nom
// d'au moins cette longueur.
constexpr char kExtensionMark = '.';
constexpr std::size_t kExtensionSize = 4;
constexpr std::size_t kShortestRenamed = 5;
// La retouche d'une carte d'instance : le « @ » doit être à cette position au
// moins, et les caractères retirés commencent autant de positions avant lui.
constexpr char kInstanceMark = '@';
constexpr std::size_t kInstanceMarkLeast = 4;
constexpr std::size_t kInstanceDropped = 3;

core::Error Refused(std::string detail) {
  return core::Error{core::ErrorCode::kCorruptData, "noms de cartes : " + std::move(detail)};
}

// Un texte facultatif de l'entrée. Faux quand le champ n'est pas un texte.
[[nodiscard]] bool ReadText(const YAML::Node& entry, const char* field, std::string& text) {
  const YAML::Node node = entry[field];
  if (!node) {
    return true;
  }
  if (!node.IsScalar()) {
    return false;
  }
  text = node.as<std::string>();
  return true;
}

}  // namespace

std::string MapNameKey(std::string_view gat_name) {
  std::string key(gat_name);
  if (key.size() >= kShortestRenamed && key[key.size() - kExtensionSize] == kExtensionMark) {
    key.resize(key.size() - kExtensionSize);
  }
  const std::size_t mark = key.rfind(kInstanceMark);
  if (mark != std::string::npos && mark >= kInstanceMarkLeast) {
    key.erase(mark - kInstanceMarkLeast, kInstanceDropped);
  }
  return key;
}

core::Result<MapDisplayNames> MapDisplayNames::Load(Vfs& vfs) {
  auto bytes = vfs.Read(kMapNamesPath);
  if (!bytes) {
    return std::unexpected(bytes.error());
  }
  const std::string_view text(reinterpret_cast<const char*>(bytes->data()), bytes->size());
  return Parse(text);
}

core::Result<MapDisplayNames> MapDisplayNames::Parse(std::string_view yaml_text) {
  MapDisplayNames loaded;
  // yaml-cpp signale par exception : elle ne franchit pas cette fonction.
  try {
    const auto root = VersionedRoot(yaml_text, kSupportedVersion);
    if (!root) {
      return std::unexpected(Refused(root.error()));
    }
    const YAML::Node& whole = *root;
    const YAML::Node maps = whole["maps"];
    if (!maps || !maps.IsMap()) {
      return std::unexpected(Refused("« maps » n'est pas une table"));
    }
    for (const auto& pair : maps) {
      const std::string map_name = pair.first.as<std::string>();
      const YAML::Node& entry = pair.second;
      if (!entry.IsMap()) {
        return std::unexpected(Refused(map_name + " n'est pas une table"));
      }
      std::string shown;
      MapSign sign;
      std::string background;
      const YAML::Node announced = entry["announced"];
      if (announced) {
        sign.announced = announced.as<bool>();
      }
      if (!ReadText(entry, "name", shown) || !ReadText(entry, "subtitle", sign.subtitle) ||
          !ReadText(entry, "title", sign.title) || !ReadText(entry, "background", background)) {
        return std::unexpected(Refused(map_name + " : un champ n'est pas un texte"));
      }
      if (!background.empty()) {
        sign.background = std::move(background);
      }
      if (!shown.empty()) {
        loaded.names_.insert_or_assign(map_name, std::move(shown));
      }
      if (sign.announced || sign.titled()) {
        loaded.signs_.insert_or_assign(map_name, std::move(sign));
      }
    }
  } catch (const YAML::Exception& failure) {
    return std::unexpected(Refused(failure.what()));
  }
  return loaded;
}

const MapSign* MapDisplayNames::SignOf(std::string_view gat_name) const {
  const auto found = signs_.find(MapNameKey(gat_name));
  return found == signs_.end() ? nullptr : &found->second;
}

std::optional<std::string_view> MapDisplayNames::DisplayNameOf(std::string_view gat_name) const {
  const auto found = names_.find(MapNameKey(gat_name));
  if (found == names_.end()) {
    return std::nullopt;
  }
  return std::string_view(found->second);
}

}  // namespace ro::vfs
