#include "patcher/patch_run.h"

#include <array>
#include <fstream>
#include <string_view>
#include <system_error>
#include <vector>

#include "ro/core/file_write.h"
#include "ro/core/log.h"
#include "ro/core/sha256.h"
#include "ro/formats/thor.h"

#include "patcher/patch_apply.h"
#include "patcher/patch_list.h"
#include "patcher/patcher_state.h"

namespace moonclient::patcher {
namespace {

using ro::core::ErrorCode;
using ro::core::Fail;
using ro::core::Result;

// Ce qu'une liste de patchs peut peser : quelques kilo-octets en pratique.
constexpr std::uint64_t kPatchListLimitBytes = 1024U * 1024U;
// Ce qu'une archive peut peser. La plus grosse de la chaîne d'origine fait
// quelques dizaines de mégaoctets ; un gibioctet laisse toute la marge.
constexpr std::uint64_t kArchiveLimitBytes = 1024U * 1024U * 1024U;
constexpr std::size_t kHashChunkBytes = 64U * 1024U;

[[nodiscard]] Result<std::vector<PatchEntry>> FetchPatchList(const PatcherConfig& config,
                                                             PatchFetcher& fetcher,
                                                             std::string& chosen_archive_url,
                                                             std::stop_token stop) {
  std::string failures;
  for (const PatchMirror& mirror : config.mirrors) {
    auto text = fetcher.FetchText(mirror.list_url, kPatchListLimitBytes, stop);
    if (!text) {
      failures += "\n  " + mirror.name + " : " + ro::core::Describe(text.error());
      continue;
    }
    auto patches = ParsePatchList(*text);
    if (!patches) {
      failures += "\n  " + mirror.name + " : " + ro::core::Describe(patches.error());
      continue;
    }
    ro::core::LogInfo("patcher : liste de {} ({} patch(s))", mirror.name, patches->size());
    chosen_archive_url = mirror.archive_url;
    return patches;
  }
  return Fail(ErrorCode::kIoFailure, "aucun serveur de patchs n'a répondu :" + failures);
}

[[nodiscard]] Result<std::string> Sha256OfFile(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return Fail(ErrorCode::kIoFailure, "impossible de relire " + path.string());
  }
  ro::core::Sha256 hasher;
  std::array<char, kHashChunkBytes> chunk{};
  while (file) {
    file.read(chunk.data(), chunk.size());
    const auto read = static_cast<std::size_t>(file.gcount());
    hasher.Update(std::span(reinterpret_cast<const std::uint8_t*>(chunk.data()), read));
  }
  if (file.bad()) {
    return Fail(ErrorCode::kIoFailure, "lecture interrompue de " + path.string());
  }
  return ro::core::Sha256::ToHex(hasher.Finish());
}

// Un patch, du téléchargement à l'enregistrement de son index.
[[nodiscard]] Result<PatchApplied> RunOnePatch(const PatchEntry& patch,
                                               const PatchRunSettings& settings,
                                               const PatchTarget& target,
                                               const std::string& archive_url,
                                               PatchFetcher& fetcher, PatchProgressBoard& board,
                                               PatchProgress& progress, std::stop_token stop) {
  if (patch.sha256_hex.empty() && settings.config.require_hashes) {
    return Fail(ErrorCode::kCorruptData,
                patch.archive_name + " : aucune empreinte dans la liste, et elle est exigée");
  }
  const std::filesystem::path folder = settings.install_root / kDownloadFolderName;
  std::error_code failed;
  std::filesystem::create_directories(folder, failed);
  const std::filesystem::path local_path = folder / patch.archive_name;

  progress.phase = PatchPhase::kDownloading;
  progress.download = FetchProgress{};
  board.Publish(progress);
  auto fetched = fetcher.FetchToFile(
      archive_url + patch.archive_name, local_path, kArchiveLimitBytes,
      [&](const FetchProgress& download) {
        progress.download = download;
        board.Publish(progress);
      },
      stop);
  if (!fetched) {
    return std::unexpected(fetched.error());
  }

  // Le fichier téléchargé ne survit pas à ce patch, quelle qu'en soit l'issue.
  struct Discard {
    const std::filesystem::path& path;
    ~Discard() {
      std::error_code ignored;
      std::filesystem::remove(path, ignored);
    }
  } discard{local_path};

  if (!patch.sha256_hex.empty()) {
    auto digest = Sha256OfFile(local_path);
    if (!digest) {
      return std::unexpected(digest.error());
    }
    if (*digest != patch.sha256_hex) {
      return Fail(
          ErrorCode::kCorruptData,
          patch.archive_name + " : l'empreinte ne correspond pas à la liste (" + *digest + ")");
    }
  }

  progress.phase = PatchPhase::kApplying;
  board.Publish(progress);
  auto archive = ro::formats::ThorArchive::OpenFile(local_path);
  if (!archive) {
    return Fail(archive.error().code, patch.archive_name + " : " + archive.error().detail);
  }
  auto applied = ApplyPatchArchive(*archive, target);
  if (!applied) {
    return Fail(applied.error().code, patch.archive_name + " : " + applied.error().detail);
  }
  if (auto saved = ro::core::WriteFileAtomically(settings.install_root / kPatcherStateFileName,
                                                 FormatPatcherState(patch.patch_index));
      !saved) {
    return std::unexpected(saved.error());
  }
  return applied;
}

}  // namespace

Result<PatchRunOutcome> RunPatches(const PatchRunSettings& settings, PatchFetcher& fetcher,
                                   PatchProgressBoard& board, std::stop_token stop) {
  PatchProgress progress;
  const auto fail = [&](const ro::core::Error& error) -> Result<PatchRunOutcome> {
    progress.phase = stop.stop_requested() ? PatchPhase::kStopped : PatchPhase::kFailed;
    progress.failure = ro::core::Describe(error);
    board.Publish(progress);
    return std::unexpected(error);
  };

  progress.phase = PatchPhase::kListing;
  board.Publish(progress);
  std::string archive_url;
  auto patches = FetchPatchList(settings.config, fetcher, archive_url, stop);
  if (!patches) {
    return fail(patches.error());
  }
  const std::vector<PatchEntry> pending =
      PendingPatches(*patches, ReadLastAppliedPatch(settings.install_root));

  PatchTarget target{settings.install_root, settings.config.default_grf, settings.config.create_grf,
                     settings.running_executable};
  PatchRunOutcome outcome;
  progress.patch_count = pending.size();
  for (const PatchEntry& patch : pending) {
    if (stop.stop_requested()) {
      return fail(ro::core::Error{ErrorCode::kIoFailure, "patch interrompu"});
    }
    ++progress.patch_number;
    progress.archive_name = patch.archive_name;
    auto applied =
        RunOnePatch(patch, settings, target, archive_url, fetcher, board, progress, stop);
    if (!applied) {
      return fail(applied.error());
    }
    ++outcome.applied_patches;
    if (applied->executable_replaced) {
      outcome.executable_replaced = true;
      // L'exécutable qui tourne porte désormais le suffixe : sous son ancien
      // nom se trouve le nouveau, qu'un patch suivant réécrit librement.
      target.running_executable.clear();
    }
    ro::core::LogInfo("patcher : {} appliqué (index {})", patch.archive_name, patch.patch_index);
  }
  progress.phase = PatchPhase::kDone;
  board.Publish(progress);
  return outcome;
}

void RemoveReplacedExecutable(const std::filesystem::path& running_executable) {
  std::filesystem::path aside = running_executable;
  aside += kReplacedExecutableSuffix;
  std::error_code ignored;
  std::filesystem::remove(aside, ignored);
}

}  // namespace moonclient::patcher
