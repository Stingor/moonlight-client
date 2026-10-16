#include "world/map_edit_store.h"

#include <fstream>
#include <iterator>
#include <system_error>
#include <utility>

#include "ro/core/file_write.h"
#include "ro/core/log.h"
#include "ro/core/utf8_path.h"
#include "ro/render/map_scene.h"

namespace moonclient {

void MapEditStore::Load(std::string_view map_name, ro::vfs::Vfs& vfs) {
  if (by_map_.contains(map_name)) {
    return;
  }
  Entry& entry = by_map_[std::string(map_name)];
  std::ifstream stream(FileOf(map_name), std::ios::binary);
  if (!stream) {
    entry.edit = ro::render::LoadMapEdit(vfs, map_name);
    return;
  }
  const std::string text((std::istreambuf_iterator<char>(stream)),
                         std::istreambuf_iterator<char>());
  auto edit = ro::mapedit::ParseMapEdit(text, map_name);
  if (!edit) {
    ro::core::LogWarning("{} — {} s'ouvre sans édition", ro::core::Describe(edit.error()),
                         map_name);
    return;
  }
  ro::core::LogInfo("édition de {} lue dans {}", map_name,
                    ro::core::Utf8FromPath(FileOf(map_name)));
  entry.edit = std::move(*edit);
}

const ro::mapedit::MapEdit& MapEditStore::Document(std::string_view map_name) const {
  static const ro::mapedit::MapEdit kNoEdit;
  const auto found = by_map_.find(map_name);
  return found == by_map_.end() ? kNoEdit : found->second.edit;
}

void MapEditStore::Set(std::string_view map_name, ro::mapedit::MapEdit edit) {
  Entry& entry = by_map_[std::string(map_name)];
  entry.edit = std::move(edit);
  entry.unsaved = true;
}

bool MapEditStore::Unsaved(std::string_view map_name) const {
  const auto found = by_map_.find(map_name);
  return found != by_map_.end() && found->second.unsaved;
}

std::filesystem::path MapEditStore::FileOf(std::string_view map_name) const {
  return client_root_ / "data" / "map_edits" /
         ro::core::PathFromUtf8(std::string(map_name) + ".yml");
}

ro::core::Result<void> MapEditStore::Save(std::string_view map_name) {
  const std::filesystem::path path = FileOf(map_name);
  std::error_code ignored;
  std::filesystem::create_directories(path.parent_path(), ignored);
  auto written =
      ro::core::WriteFileAtomically(path, ro::mapedit::MapEditToYaml(Document(map_name), map_name));
  if (written) {
    by_map_[std::string(map_name)].unsaved = false;
  }
  return written;
}

}  // namespace moonclient
