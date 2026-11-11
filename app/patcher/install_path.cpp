#include "patcher/install_path.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <span>
#include <string>
#include <system_error>
#include <vector>

#include "ro/core/file_write.h"
#include "ro/core/log.h"
#include "ro/core/sha256.h"
#include "ro/core/utf8_path.h"

#include "patcher/patch_run.h"

namespace moonclient::patcher {
namespace {

using ro::core::ErrorCode;
using ro::core::Fail;
using ro::core::Result;

constexpr std::size_t kHashChunkBytes = 64U * 1024U;

[[nodiscard]] std::vector<std::string> ReadSetAsideJournal(
    const std::filesystem::path& install_root) {
  std::vector<std::string> lines;
  std::ifstream file(install_root / kSetAsideJournalFileName, std::ios::binary);
  std::string line;
  while (std::getline(file, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (!line.empty()) {
      lines.push_back(std::move(line));
    }
  }
  return lines;
}

// Réécrit le journal, ou l'efface quand il ne reste rien.
void WriteSetAsideJournal(const std::filesystem::path& install_root,
                          const std::vector<std::string>& lines) {
  const std::filesystem::path journal_path = install_root / kSetAsideJournalFileName;
  if (lines.empty()) {
    std::error_code ignored;
    std::filesystem::remove(journal_path, ignored);
    return;
  }
  std::string text;
  for (const std::string& line : lines) {
    text += line;
    text += '\n';
  }
  if (auto written = ro::core::WriteFileAtomically(journal_path, text); !written) {
    ro::core::LogWarning("patcher : journal des fichiers écartés non écrit — {}",
                         ro::core::Describe(written.error()));
  }
}

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

Result<std::filesystem::path> SetInUseFileAside(const std::filesystem::path& install_root,
                                                const std::filesystem::path& disk_path) {
  std::filesystem::path aside = disk_path;
  aside += kInUseSetAsideSuffix;
  std::error_code failed;
  std::filesystem::remove(aside, failed);
  failed.clear();
  std::filesystem::rename(disk_path, aside, failed);
  if (failed) {
    return Fail(ErrorCode::kIoFailure, "impossible d'écarter " + ro::core::Utf8FromPath(disk_path) +
                                           " : " + failed.message());
  }
  const std::string relative = ro::core::Utf8FromPath(aside.lexically_relative(install_root));
  std::vector<std::string> lines = ReadSetAsideJournal(install_root);
  if (std::find(lines.begin(), lines.end(), relative) == lines.end()) {
    lines.push_back(relative);
    WriteSetAsideJournal(install_root, lines);
  }
  return aside;
}

void RemoveSetAsideFiles(const std::filesystem::path& install_root) {
  const std::vector<std::string> lines = ReadSetAsideJournal(install_root);
  if (lines.empty()) {
    return;
  }
  std::vector<std::string> remaining;
  for (const std::string& line : lines) {
    auto disk_path = InstallPathOf(install_root, line);
    if (!disk_path || !line.ends_with(kInUseSetAsideSuffix)) {
      ro::core::LogWarning("patcher : ligne ignorée du journal des fichiers écartés : {}", line);
      continue;
    }
    std::error_code failed;
    std::filesystem::remove(*disk_path, failed);
    if (failed) {
      remaining.push_back(line);
    }
  }
  WriteSetAsideJournal(install_root, remaining);
}

void KeepExecutablePermissions(const std::filesystem::path& aside,
                               const std::filesystem::path& running_executable) {
  std::error_code ignored;
  const std::filesystem::file_status previous = std::filesystem::status(aside, ignored);
  if (!ignored) {
    std::filesystem::permissions(running_executable, previous.permissions(), ignored);
  }
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
