#include "shell/skin_folder.h"

#include <algorithm>
#include <array>
#include <string>
#include <system_error>

#include "ro/core/file_write.h"
#include "ro/core/log.h"
#include "ro/render/skin_pieces.h"
#include "ro/ui/login_window.h"

namespace moonclient {
namespace {

// Les pièces que le skin d'origine doit porter sur le disque.
constexpr std::array<std::string_view, 1> kPiecesReadWithoutArchives = {ro::ui::kLoginArtPath};

}  // namespace

std::optional<std::filesystem::path> SkinFilePath(const std::filesystem::path& install_root,
                                                  std::string_view skin_name,
                                                  std::string_view utf8_ui_path) {
  if (!utf8_ui_path.starts_with(ro::render::kSkinRoot) || skin_name.empty()) {
    return std::nullopt;
  }
  std::string relative(utf8_ui_path.substr(ro::render::kSkinRoot.size()));
  for (char& character : relative) {
    if (character == '\\') {
      character = '/';
    }
  }
  const std::filesystem::path inside =
      std::filesystem::path(
          std::u8string(reinterpret_cast<const char8_t*>(relative.data()), relative.size()))
          .lexically_normal();
  if (inside.empty() || inside.is_absolute() || inside.has_root_name() || *inside.begin() == "..") {
    return std::nullopt;
  }
  const std::filesystem::path skin(
      std::u8string(reinterpret_cast<const char8_t*>(skin_name.data()), skin_name.size()));
  return install_root / kSkinFolderName / skin / inside;
}

std::filesystem::path SkinFolder(const std::filesystem::path& install_root,
                                 std::string_view skin_name) {
  return install_root / kSkinFolderName /
         std::filesystem::path(
             std::u8string(reinterpret_cast<const char8_t*>(skin_name.data()), skin_name.size()));
}

std::vector<std::string> InstalledSkins(const std::filesystem::path& install_root) {
  std::vector<std::string> found;
  std::error_code walk_error;
  for (const std::filesystem::directory_entry& entry :
       std::filesystem::directory_iterator(install_root / kSkinFolderName, walk_error)) {
    std::error_code kind_error;
    if (!entry.is_directory(kind_error)) {
      continue;
    }
    const std::u8string name = entry.path().filename().u8string();
    std::string utf8(reinterpret_cast<const char*>(name.data()), name.size());
    if (utf8 != kDefaultSkinName) {
      found.push_back(std::move(utf8));
    }
  }
  std::ranges::sort(found);
  found.insert(found.begin(), std::string(kDefaultSkinName));
  return found;
}

std::optional<std::filesystem::path> SkinPieceOnDisk(const std::filesystem::path& install_root,
                                                     std::string_view skin_name,
                                                     std::string_view utf8_ui_path) {
  for (const std::string_view skin : {skin_name, kDefaultSkinName}) {
    const std::optional<std::filesystem::path> path =
        SkinFilePath(install_root, skin, utf8_ui_path);
    std::error_code probe_error;
    if (path.has_value() && std::filesystem::is_regular_file(*path, probe_error)) {
      return path;
    }
  }
  return std::nullopt;
}

void UseSkin(ro::vfs::Vfs& vfs, const std::filesystem::path& install_root,
             std::string_view skin_name) {
  vfs.OverlayFolder(ro::render::kSkinRoot, SkinFolder(install_root, skin_name));
}

void SeedDefaultSkin(ro::vfs::Vfs& vfs, const std::filesystem::path& install_root) {
  for (const std::string_view piece : kPiecesReadWithoutArchives) {
    const std::optional<std::filesystem::path> target =
        SkinFilePath(install_root, kDefaultSkinName, piece);
    std::error_code probe_error;
    if (!target.has_value() || std::filesystem::exists(*target, probe_error)) {
      continue;
    }
    auto bytes = vfs.Read(piece);
    if (!bytes) {
      continue;
    }
    auto written = ro::core::WriteFileAtomically(
        *target, std::string_view(reinterpret_cast<const char*>(bytes->data()), bytes->size()));
    if (!written) {
      ro::core::LogWarning("skin : {} non déposé ({})", target->string(),
                           ro::core::Describe(written.error()));
    }
  }
}

}  // namespace moonclient
