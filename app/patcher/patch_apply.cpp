#include "patcher/patch_apply.h"

#include <memory>
#include <optional>
#include <string_view>
#include <system_error>
#include <vector>

#include "ro/core/byte_source.h"
#include "ro/core/file_write.h"
#include "ro/formats/grf_merge.h"
#include "ro/vfs/encoding.h"

#include "patcher/install_path.h"
#include "patcher/patch_run.h"

namespace moonclient::patcher {
namespace {

using ro::core::ErrorCode;
using ro::core::Fail;
using ro::core::Result;

// Le chemin du disque d'un chemin d'archive, sous l'installation, ou une
// erreur s'il en sortirait.
//
// ⚠ LES SÉPARATEURS SE CONVERTISSENT APRÈS LE PASSAGE EN UTF-8 : un octet de
// queue CP949 peut valoir « \ ».
[[nodiscard]] Result<std::filesystem::path> InstallPathOfArchive(const std::filesystem::path& root,
                                                                 std::string_view archive_path) {
  auto utf8 = ro::vfs::Cp949ToUtf8(archive_path);
  if (!utf8) {
    return std::unexpected(utf8.error());
  }
  return InstallPathOf(root, *utf8);
}

[[nodiscard]] Result<void> MergeIntoGrf(ro::formats::ThorArchive& archive,
                                        const PatchTarget& target) {
  const std::string& grf_name =
      archive.target_grf_name().empty() ? target.default_grf : archive.target_grf_name();
  auto grf_path = InstallPathOfArchive(target.install_root, grf_name);
  if (!grf_path) {
    return std::unexpected(grf_path.error());
  }
  std::error_code probe_error;
  const bool grf_exists = std::filesystem::exists(*grf_path, probe_error);
  if (!grf_exists && !target.create_grf) {
    return Fail(ErrorCode::kNotFound, grf_path->string() + " : archive à patcher absente");
  }
  return ro::core::WriteFileAtomicallyWith(
      *grf_path, [&](ro::core::ByteSink& sink) -> Result<void> {
        // ⚠ L'ANCIENNE ARCHIVE SE REFERME ICI, avant le renommage qui la
        // remplace.
        std::unique_ptr<ro::core::FileByteSource> previous;
        if (grf_exists) {
          auto opened = ro::core::FileByteSource::Open(*grf_path);
          if (!opened) {
            return std::unexpected(opened.error());
          }
          previous = std::move(*opened);
        }
        auto merged = ro::formats::MergeThorIntoGrf(archive, previous.get(), sink);
        if (!merged) {
          return std::unexpected(merged.error());
        }
        return {};
      });
}

[[nodiscard]] Result<PatchApplied> WriteToInstall(ro::formats::ThorArchive& archive,
                                                  const PatchTarget& target) {
  // Tous les chemins se vérifient AVANT la première écriture.
  std::vector<std::filesystem::path> disk_paths;
  for (const ro::formats::ThorEntry& entry : archive.entries()) {
    auto disk_path = InstallPathOfArchive(target.install_root, entry.path);
    if (!disk_path) {
      return std::unexpected(disk_path.error());
    }
    disk_paths.push_back(std::move(*disk_path));
  }

  PatchApplied applied;
  // Dans l'ordre de la table : un chemin répété garde sa dernière version.
  for (std::size_t entry_index = 0; entry_index < disk_paths.size(); ++entry_index) {
    const ro::formats::ThorEntry& entry = archive.entries()[entry_index];
    const std::filesystem::path& disk_path = disk_paths[entry_index];
    // Une fois écarté, l'exécutable qui tourne est le `.old` : le nom se réécrit
    // ensuite comme un fichier ordinaire.
    const bool replaces_running =
        !applied.executable_replaced && IsRunningExecutable(disk_path, target.running_executable);
    if (entry.is_removed) {
      // L'exécutable qui tourne ne se retire pas : il sera remplacé ou gardé.
      std::error_code ignored;
      if (!replaces_running) {
        std::filesystem::remove(disk_path, ignored);
      }
      continue;
    }
    auto content = archive.Read(entry);
    if (!content) {
      return std::unexpected(content.error());
    }
    std::optional<std::filesystem::path> set_aside;
    if (replaces_running) {
      auto aside = SetRunningExecutableAside(target.running_executable);
      if (!aside) {
        return std::unexpected(aside.error());
      }
      set_aside = std::move(*aside);
    }
    auto written = ro::core::WriteFileAtomically(
        disk_path,
        std::string_view(reinterpret_cast<const char*>(content->data()), content->size()));
    if (!written) {
      if (set_aside.has_value()) {
        std::error_code ignored;
        std::filesystem::rename(*set_aside, target.running_executable, ignored);
      }
      return std::unexpected(written.error());
    }
    applied.executable_replaced = applied.executable_replaced || replaces_running;
  }
  return applied;
}

}  // namespace

Result<PatchApplied> ApplyPatchArchive(ro::formats::ThorArchive& archive,
                                       const PatchTarget& target) {
  if (auto verified = archive.VerifyIntegrity(); !verified) {
    return std::unexpected(verified.error());
  }
  if (archive.merges_into_grf()) {
    if (auto merged = MergeIntoGrf(archive, target); !merged) {
      return std::unexpected(merged.error());
    }
    return PatchApplied{};
  }
  return WriteToInstall(archive, target);
}

}  // namespace moonclient::patcher
