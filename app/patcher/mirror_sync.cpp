#include "patcher/mirror_sync.h"

#include <algorithm>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

#include "ro/core/log.h"
#include "ro/core/utf8_path.h"

#include "patcher/install_path.h"
#include "patcher/mirror_cache.h"
#include "patcher/mirror_manifest.h"

namespace moonclient::patcher {
namespace {

using ro::core::ErrorCode;
using ro::core::Fail;
using ro::core::Result;

// Ce qu'un manifeste peut peser : une ligne d'une centaine d'octets par
// fichier, pour des dizaines de milliers de fichiers.
constexpr std::uint64_t kManifestLimitBytes = 16U * 1024U * 1024U;
// Le préfixe des fichiers du patcher lui-même, à la racine du jeu : état de
// la chaîne, verrou, téléchargements, cache du miroir.
constexpr std::string_view kPatcherFilePrefix = "moonclient-patch";
constexpr std::string_view kMirrorDownloadName = "mirror.download";

[[nodiscard]] Result<std::vector<MirrorFile>> FetchManifest(const PatcherConfig& config,
                                                            PatchFetcher& fetcher,
                                                            std::string& chosen_files_url,
                                                            std::stop_token stop) {
  std::string failures;
  for (const PatchMirror& mirror : config.mirrors) {
    if (mirror.manifest_url.empty()) {
      continue;
    }
    auto text = fetcher.FetchText(mirror.manifest_url, kManifestLimitBytes, stop);
    if (!text) {
      ro::core::LogWarning("patcher : manifeste de {} injoignable ({}) — {}", mirror.name,
                           mirror.manifest_url, ro::core::Describe(text.error()));
      failures += "\n  " + mirror.name + " : " + ro::core::Describe(text.error());
      continue;
    }
    auto files = ParseMirrorManifest(*text);
    if (!files) {
      ro::core::LogWarning("patcher : manifeste de {} illisible — {}", mirror.name,
                           ro::core::Describe(files.error()));
      failures += "\n  " + mirror.name + " : " + ro::core::Describe(files.error());
      continue;
    }
    ro::core::LogInfo("patcher : manifeste de {} ({} fichier(s))", mirror.name, files->size());
    chosen_files_url = mirror.files_url;
    return files;
  }
  return Fail(ErrorCode::kIoFailure, "aucun miroir de fichiers n'a répondu :" + failures);
}

[[nodiscard]] bool TouchesPatcherFiles(std::string_view relative_path) {
  std::string first_segment(relative_path.substr(0, relative_path.find('/')));
  std::ranges::transform(first_segment, first_segment.begin(), [](char character) {
    return character >= 'A' && character <= 'Z' ? static_cast<char>(character - 'A' + 'a')
                                                : character;
  });
  return first_segment.starts_with(kPatcherFilePrefix) ||
         relative_path.ends_with(kMirrorSetAsideSuffix);
}

[[nodiscard]] std::optional<std::int64_t> WriteTicksOf(const std::filesystem::path& path) {
  std::error_code failed;
  const auto written = std::filesystem::last_write_time(path, failed);
  if (failed) {
    return std::nullopt;
  }
  return static_cast<std::int64_t>(written.time_since_epoch().count());
}

// Vrai si le fichier local porte déjà le contenu publié. Ce que l'on apprend
// de lui en chemin s'inscrit dans `fresh_cache`.
[[nodiscard]] bool IsUpToDate(const MirrorFile& file, const std::filesystem::path& disk_path,
                              const MirrorCache& known_cache, MirrorCache& fresh_cache) {
  std::error_code failed;
  if (!std::filesystem::is_regular_file(disk_path, failed)) {
    return false;
  }
  const std::uint64_t size_bytes = std::filesystem::file_size(disk_path, failed);
  const std::optional<std::int64_t> write_ticks = WriteTicksOf(disk_path);
  if (failed || !write_ticks.has_value() || size_bytes != file.size_bytes) {
    return false;
  }
  std::string sha256_hex;
  if (const auto known = known_cache.find(file.relative_path);
      known != known_cache.end() && known->second.size_bytes == size_bytes &&
      known->second.write_ticks == *write_ticks) {
    sha256_hex = known->second.sha256_hex;
  } else {
    auto digest = Sha256OfFile(disk_path);
    if (!digest) {
      return false;
    }
    sha256_hex = std::move(*digest);
  }
  const bool up_to_date = sha256_hex == file.sha256_hex;
  fresh_cache.insert_or_assign(file.relative_path,
                               KnownFile{size_bytes, *write_ticks, std::move(sha256_hex)});
  return up_to_date;
}

// Déplace le fichier reçu à sa place. Rend vrai quand il a fallu écarter un
// fichier en usage, ce qui demande une relance.
[[nodiscard]] Result<bool> PutInPlace(const std::filesystem::path& received,
                                      const std::filesystem::path& disk_path,
                                      const std::filesystem::path& running_executable) {
  std::error_code failed;
  if (std::filesystem::is_directory(disk_path, failed)) {
    return Fail(ErrorCode::kIoFailure,
                "un dossier occupe la place de " + ro::core::Utf8FromPath(disk_path));
  }
  std::filesystem::create_directories(disk_path.parent_path(), failed);
  if (IsRunningExecutable(disk_path, running_executable)) {
    auto aside = SetRunningExecutableAside(running_executable);
    if (!aside) {
      return std::unexpected(aside.error());
    }
    std::filesystem::rename(received, disk_path, failed);
    if (failed) {
      std::error_code ignored;
      std::filesystem::rename(*aside, running_executable, ignored);
      return Fail(ErrorCode::kIoFailure,
                  "impossible de poser le nouvel exécutable : " + failed.message());
    }
    return true;
  }
  failed.clear();
  std::filesystem::rename(received, disk_path, failed);
  if (!failed) {
    return false;
  }
  // Un fichier en usage ne se remplace pas, mais se renomme.
  std::filesystem::path aside = disk_path;
  aside += kMirrorSetAsideSuffix;
  std::error_code aside_failed;
  std::filesystem::remove(aside, aside_failed);
  std::filesystem::rename(disk_path, aside, aside_failed);
  if (aside_failed) {
    return Fail(
        ErrorCode::kIoFailure,
        "impossible de remplacer " + ro::core::Utf8FromPath(disk_path) + " : " + failed.message());
  }
  std::filesystem::rename(received, disk_path, failed);
  if (failed) {
    std::error_code ignored;
    std::filesystem::rename(aside, disk_path, ignored);
    return Fail(ErrorCode::kIoFailure, "impossible de poser " + ro::core::Utf8FromPath(disk_path) +
                                           " : " + failed.message());
  }
  return true;
}

}  // namespace

Result<MirrorSyncOutcome> SyncMirror(const PatchRunSettings& settings,
                                     const std::filesystem::path& running_executable,
                                     PatchFetcher& fetcher, PatchProgressBoard& board,
                                     PatchProgress& progress, std::stop_token stop) {
  MirrorSyncOutcome outcome;
  if (std::ranges::none_of(settings.config.mirrors, [](const PatchMirror& mirror) {
        return !mirror.manifest_url.empty();
      })) {
    return outcome;
  }
  progress = PatchProgress{};
  progress.phase = PatchPhase::kListing;
  board.Publish(progress);
  std::string files_url;
  auto files = FetchManifest(settings.config, fetcher, files_url, stop);
  if (!files) {
    return std::unexpected(files.error());
  }

  std::vector<std::filesystem::path> disk_paths;
  disk_paths.reserve(files->size());
  for (const MirrorFile& file : *files) {
    if (TouchesPatcherFiles(file.relative_path)) {
      return Fail(ErrorCode::kCorruptData,
                  "le miroir vise un fichier du patcher : " + file.relative_path);
    }
    auto disk_path = InstallPathOf(settings.install_root, file.relative_path);
    if (!disk_path) {
      return std::unexpected(disk_path.error());
    }
    disk_paths.push_back(std::move(*disk_path));
  }

  const MirrorCache known_cache = ReadMirrorCache(settings.install_root);
  MirrorCache fresh_cache;
  std::vector<std::size_t> stale_files;
  for (std::size_t file_index = 0; file_index < files->size(); ++file_index) {
    if (stop.stop_requested()) {
      return Fail(ErrorCode::kIoFailure, "vérification interrompue");
    }
    std::filesystem::path set_aside = disk_paths[file_index];
    set_aside += kMirrorSetAsideSuffix;
    std::error_code ignored;
    std::filesystem::remove(set_aside, ignored);
    if (!IsUpToDate((*files)[file_index], disk_paths[file_index], known_cache, fresh_cache)) {
      stale_files.push_back(file_index);
    }
  }
  ro::core::LogInfo("patcher : {} fichier(s) du miroir à télécharger sur {}", stale_files.size(),
                    files->size());

  // Le cache s'enregistre quelle que soit l'issue : ce qui a été haché ou
  // posé avant un échec n'a pas à l'être de nouveau.
  struct SaveCache {
    const std::filesystem::path& install_root;
    const MirrorCache& known;
    const MirrorCache& fresh;
    ~SaveCache() {
      if (fresh != known) {
        WriteMirrorCache(install_root, fresh);
      }
    }
  } save_cache{settings.install_root, known_cache, fresh_cache};

  const std::filesystem::path received_path =
      settings.install_root / kDownloadFolderName / kMirrorDownloadName;
  std::error_code failed;
  std::filesystem::create_directories(received_path.parent_path(), failed);
  std::filesystem::path running = running_executable;
  progress.patch_count = stale_files.size();
  for (const std::size_t file_index : stale_files) {
    if (stop.stop_requested()) {
      return Fail(ErrorCode::kIoFailure, "téléchargement interrompu");
    }
    const MirrorFile& file = (*files)[file_index];
    const std::filesystem::path& disk_path = disk_paths[file_index];
    ++progress.patch_number;
    progress.archive_name = file.relative_path;
    progress.phase = PatchPhase::kDownloading;
    progress.download = FetchProgress{};
    board.Publish(progress);
    auto fetched = fetcher.FetchToFile(
        MirrorFileUrl(files_url, file.relative_path), received_path, file.size_bytes,
        [&](const FetchProgress& download) {
          progress.download = download;
          board.Publish(progress);
        },
        stop);
    if (!fetched) {
      return Fail(fetched.error().code, file.relative_path + " : " + fetched.error().detail);
    }
    struct Discard {
      const std::filesystem::path& path;
      ~Discard() {
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
      }
    } discard{received_path};

    auto digest = Sha256OfFile(received_path);
    if (!digest) {
      return std::unexpected(digest.error());
    }
    if (*digest != file.sha256_hex) {
      return Fail(ErrorCode::kCorruptData,
                  file.relative_path +
                      " : le fichier reçu ne correspond pas au manifeste (dépôt en cours ?)");
    }

    progress.phase = PatchPhase::kApplying;
    board.Publish(progress);
    const bool replaces_running = IsRunningExecutable(disk_path, running);
    auto placed = PutInPlace(received_path, disk_path, running);
    if (!placed) {
      return Fail(placed.error().code, file.relative_path + " : " + placed.error().detail);
    }
    outcome.restart_needed = outcome.restart_needed || *placed;
    if (replaces_running) {
      // L'exécutable qui tourne a été écarté : sous son nom se trouve
      // désormais un fichier ordinaire.
      running.clear();
    }
    if (const std::optional<std::int64_t> write_ticks = WriteTicksOf(disk_path)) {
      fresh_cache.insert_or_assign(file.relative_path,
                                   KnownFile{file.size_bytes, *write_ticks, file.sha256_hex});
    }
    ++outcome.written_files;
    ro::core::LogInfo("patcher : {} posé depuis le miroir ({} octet(s))", file.relative_path,
                      file.size_bytes);
  }
  return outcome;
}

}  // namespace moonclient::patcher
