#include "ro/vfs/indoor_maps.h"

#include "install_table.h"

namespace ro::vfs {

core::Result<std::vector<std::string>> ParseIndoorMaps(std::string_view yaml_text) {
  std::vector<std::string> maps;
  auto read = VisitTableSection(yaml_text, kIndoorMapsPath, "maps", YAML::NodeType::Sequence,
                                [&maps](const YAML::Node& entries) {
                                  for (const YAML::Node& map : entries) {
                                    maps.push_back(ArchivedText(map));
                                  }
                                });
  if (!read) {
    return std::unexpected(read.error());
  }
  return maps;
}

core::Result<std::vector<std::string>> ReadIndoorMaps(Vfs& vfs) {
  auto text = ReadOptionalTable(vfs, kIndoorMapsPath);
  if (!text) {
    return std::unexpected(text.error());
  }
  return text->has_value() ? ParseIndoorMaps(**text) : std::vector<std::string>{};
}

}  // namespace ro::vfs
