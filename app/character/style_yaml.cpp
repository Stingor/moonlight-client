#include "character/style_yaml.h"

#include <string>
#include <system_error>

#include "game/save_folder.h"
#include "game/yaml_load.h"

namespace moonclient {

std::filesystem::path StyleFilePath(const std::filesystem::path& client_root,
                                    std::string_view file_name) {
  return client_root / kSaveFolder / file_name;
}

std::optional<YAML::Node> ReadStyleFile(const std::filesystem::path& path,
                                        std::string_view subject) {
  return LoadYamlMap(path, subject, "rien de ce qu'il rangeait ne se reprend");
}

bool WriteStyleFile(const std::filesystem::path& path, std::string_view header,
                    const YAML::Emitter& document, std::string_view subject) {
  // ⚠ LE DOSSIER PEUT MANQUER : une installation neuve n'a encore rien rangé.
  std::error_code unused;
  std::filesystem::create_directories(path.parent_path(), unused);
  return SaveYamlFile(path, header, document, subject);
}

std::optional<ro::sim::StyleCode> StyleCodeAt(const YAML::Node& node) {
  if (!node || !node.IsScalar()) {
    return std::nullopt;
  }
  return ro::sim::DecodeStyleCode(node.as<std::string>(std::string{}));
}

}  // namespace moonclient
