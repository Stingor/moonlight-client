#include "ro/vfs/map_music_table.h"

#include <map>
#include <string>
#include <utility>

#include "install_table.h"

namespace ro::vfs {

core::Result<formats::BgmTable> ParseMapMusic(std::string_view yaml_text) {
  std::map<std::string, std::string, std::less<>> tracks;
  auto read = VisitTableSection(
      yaml_text, kMapMusicPath, "maps", YAML::NodeType::Map, [&tracks](const YAML::Node& entries) {
        for (const auto& pair : entries) {
          tracks.insert_or_assign(ArchivedText(pair.first), ArchivedText(pair.second));
        }
      });
  if (!read) {
    return std::unexpected(read.error());
  }
  return formats::BgmTable(std::move(tracks));
}

core::Result<formats::BgmTable> ReadMapMusic(Vfs& vfs) {
  auto text = ReadOptionalTable(vfs, kMapMusicPath);
  if (!text) {
    return std::unexpected(text.error());
  }
  if (!text->has_value()) {
    return core::Fail(core::ErrorCode::kNotFound, std::string(kMapMusicPath) + " absente");
  }
  return ParseMapMusic(**text);
}

}  // namespace ro::vfs
