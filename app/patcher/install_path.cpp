#include "patcher/install_path.h"

#include <array>
#include <fstream>
#include <span>
#include <system_error>

#include "ro/core/sha256.h"
#include "ro/core/utf8_path.h"

#include "patcher/patch_run.h"

namespace moonclient::patcher {
namespace {

using ro::core::ErrorCode;
using ro::core::Fail;
using ro::core::Result;

constexpr std::size_t kHashChunkBytes = 64U * 1024U;

}  // namespace

Result<std::filesystem::path> InstallPathOf(const std::filesystem::path& install_root,
                                            std::string_view utf8_path) {
  std::string slashed(utf8_path);
  for (char& character : slashed) {
    if (character == '\\') {
      character = '/';
    }
  }
  const std::filesystem::path relative(
      std::u8string(reinterpret_cast<const char8_t*>(slashed.data()), slashed.size()));
  const std::filesystem::path normal = relative.lexically_normal();
  if (relative.empty() || normal.is_absolute() || normal.has_root_name() ||
      normal.has_root_directory() || (!normal.empty() && *normal.begin() == "..")) {
    return Fail(ErrorCode::kCorruptData,
                "le patch vise un chemin hors de l'installation : " + slashed);
  }
  return install_root / normal;
}

bool IsRunningExecutable(const std::filesystem::path& candidate,
                         const std::filesystem::path& running_executable) {
  std::error_code ignored;
  return !running_executable.empty() && std::filesystem::exists(candidate, ignored) &&
         std::filesystem::equivalent(candidate, running_executable, ignored);
}

Result<std::filesystem::path> SetRunningExecutableAside(
    const std::filesystem::path& running_executable) {
  std::filesystem::path aside = running_executable;
  aside += kReplacedExecutableSuffix;
  std::error_code failed;
  std::filesystem::remove(aside, failed);
  std::filesystem::rename(running_executable, aside, failed);
  if (failed) {
    return Fail(ErrorCode::kIoFailure,
                "impossible d'écarter l'exécutable en cours : " + failed.message());
  }
  return aside;
}

Result<std::string> Sha256OfFile(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return Fail(ErrorCode::kIoFailure, "impossible de relire " + ro::core::Utf8FromPath(path));
  }
  ro::core::Sha256 hasher;
  std::array<char, kHashChunkBytes> chunk{};
  while (file) {
    file.read(chunk.data(), chunk.size());
    const auto read = static_cast<std::size_t>(file.gcount());
    hasher.Update(std::span(reinterpret_cast<const std::uint8_t*>(chunk.data()), read));
  }
  if (file.bad()) {
    return Fail(ErrorCode::kIoFailure, "lecture interrompue de " + ro::core::Utf8FromPath(path));
  }
  return ro::core::Sha256::ToHex(hasher.Finish());
}

}  // namespace moonclient::patcher
