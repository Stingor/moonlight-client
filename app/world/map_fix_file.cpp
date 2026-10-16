#include "world/map_fix_file.h"

#include <fstream>
#include <iterator>
#include <string>

#include "ro/core/file_write.h"
#include "ro/core/log.h"
#include "ro/core/utf8_path.h"

namespace moonclient {

std::filesystem::path MapFixFilePath(const std::filesystem::path& client_root) {
  return client_root / "data" / "yaml_files" / "map_fix.yml";
}

ro::render::MapFixTable LoadMapFixFile(const std::filesystem::path& client_root,
                                       ro::vfs::Vfs& vfs) {
  const std::filesystem::path path = MapFixFilePath(client_root);
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    return ro::render::ReadMapFixTable(vfs);
  }
  const std::string text((std::istreambuf_iterator<char>(stream)),
                         std::istreambuf_iterator<char>());
  auto table = ro::render::MapFixTable::Parse(text);
  if (!table) {
    ro::core::LogWarning("{} — aucune carte n'est retouchée", ro::core::Describe(table.error()));
    return {};
  }
  ro::core::LogInfo("retouches de carte : {} carte(s), lues dans {}", table->map_count(),
                    ro::core::Utf8FromPath(path));
  return std::move(*table);
}

ro::core::Result<void> SaveMapFixFile(const std::filesystem::path& client_root,
                                      const ro::render::MapFixTable& table) {
  const std::filesystem::path path = MapFixFilePath(client_root);
  std::error_code ignored;
  std::filesystem::create_directories(path.parent_path(), ignored);
  return ro::core::WriteFileAtomically(path, table.ToYaml());
}

}  // namespace moonclient
