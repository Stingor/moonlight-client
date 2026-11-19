#include "patcher/update_watch.h"

#include <algorithm>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "ro/core/log.h"

#include "patcher/install_path.h"
#include "patcher/mirror_cache.h"
#include "patcher/mirror_manifest.h"
#include "patcher/mirror_sync.h"
#include "patcher/patch_list.h"
#include "patcher/patcher_state.h"

namespace moonclient::patcher {
namespace {

using ro::core::ErrorCode;
using ro::core::Fail;
using ro::core::Result;

// Vrai si le fichier local diffère SÛREMENT de celui que le miroir publie.
// Un fichier dont le cache ne garantit plus l'empreinte ne se tranche pas.
[[nodiscard]] bool SurelyDiffers(const MirrorFile& file, const std::filesystem::path& disk_path,
                                 const MirrorCache& cache) {
  std::error_code failed;
  if (!std::filesystem::is_regular_file(disk_path, failed)) {
    return true;
  }
  const std::uint64_t size_bytes = std::filesystem::file_size(disk_path, failed);
  if (failed) {
    return false;
  }
  if (size_bytes != file.size_bytes) {
    return true;
  }
  const auto written = std::filesystem::last_write_time(disk_path, failed);
  const auto known = cache.find(file.relative_path);
  if (failed || known == cache.end() || known->second.size_bytes != size_bytes ||
      known->second.write_ticks != static_cast<std::int64_t>(written.time_since_epoch().count())) {
    return false;
  }
  return known->second.sha256_hex != file.sha256_hex;
}

[[nodiscard]] bool MirrorHasNewer(const std::vector<MirrorFile>& files,
                                  const std::filesystem::path& install_root) {
  const MirrorCache cache = ReadMirrorCache(install_root);
  return std::ranges::any_of(files, [&](const MirrorFile& file) {
    auto disk_path = InstallPathOf(install_root, file.relative_path);
    // Un chemin refusé, le lancement le refusera aussi : ce n'est pas une
    // mise à jour qu'on pourra prendre.
    return disk_path.has_value() && SurelyDiffers(file, *disk_path, cache);
  });
}

}  // namespace

Result<bool> ServerHasNewer(const PatchRunSettings& settings, PatchFetcher& fetcher,
                            std::stop_token stop) {
  bool any_answer = false;
  std::string unused_url;
  if (auto patches = FetchPatchList(settings.config, fetcher, unused_url, stop)) {
    any_answer = true;
    if (!PendingPatches(*patches, ReadLastAppliedPatch(settings.install_root)).empty()) {
      return true;
    }
  }
  if (std::ranges::any_of(settings.config.mirrors,
                          [](const PatchMirror& mirror) { return !mirror.manifest_url.empty(); })) {
    if (auto files = FetchMirrorManifest(settings.config, fetcher, unused_url, stop)) {
      any_answer = true;
      if (MirrorHasNewer(*files, settings.install_root)) {
        return true;
      }
    }
  }
  if (!any_answer) {
    return Fail(ErrorCode::kIoFailure, "ni la liste des patchs ni le manifeste n'ont répondu");
  }
  return false;
}

UpdateWatch::UpdateWatch(PatchRunSettings settings, std::unique_ptr<PatchFetcher> fetcher,
                         std::chrono::milliseconds period)
    : settings_(std::move(settings)),
      fetcher_(std::move(fetcher)),
      period_(period),
      worker_([this](std::stop_token stop) { Watch(stop); }) {}

// `std::jthread` demande l'arrêt puis attend ; `wait_for` s'éveille de lui-même
// sur la demande.
UpdateWatch::~UpdateWatch() = default;

void UpdateWatch::Watch(std::stop_token stop) {
  while (!stop.stop_requested()) {
    {
      std::unique_lock<std::mutex> held(wait_lock_);
      static_cast<void>(wake_.wait_for(held, stop, period_, [] { return false; }));
    }
    if (stop.stop_requested()) {
      return;
    }
    auto newer = ServerHasNewer(settings_, *fetcher_, stop);
    if (!newer) {
      ro::core::LogWarning("mise à jour : le serveur n'a pas répondu — {}",
                           ro::core::Describe(newer.error()));
      continue;
    }
    if (*newer) {
      ro::core::LogInfo("mise à jour : le serveur publie plus neuf que l'installation");
      update_available_.store(true, std::memory_order_release);
      return;
    }
  }
}

}  // namespace moonclient::patcher
