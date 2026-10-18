#include "ro/render/decor_vignette_cache.h"

#include <algorithm>
#include <fstream>
#include <ios>
#include <iterator>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "ro/core/file_write.h"
#include "ro/core/log.h"
#include "ro/core/utf8_path.h"
#include "ro/render/decor_vignette.h"

namespace ro::render {
namespace {

constexpr std::string_view kModelFolder = "data\\model\\";
constexpr std::string_view kVignetteExtension = ".vgn";
// Les deux familles de modèles du décor : deux listes, fusionnées puis triées.
constexpr std::string_view kModelSuffix = ".rsm";
constexpr std::string_view kSecondModelSuffix = ".rsm2";

}  // namespace

std::vector<std::string> ListDecorModels(const vfs::Vfs& vfs, std::string_view folder) {
  std::string prefix(kModelFolder);
  prefix += folder;
  std::vector<std::string> files = vfs.List(prefix, kModelSuffix);
  std::vector<std::string> second = vfs.List(prefix, kSecondModelSuffix);
  files.insert(files.end(), std::make_move_iterator(second.begin()),
               std::make_move_iterator(second.end()));
  for (std::string& file : files) {
    if (file.size() > kModelFolder.size()) {
      file.erase(0, kModelFolder.size());
    }
  }
  std::ranges::sort(files);
  const auto [doubled, end] = std::ranges::unique(files);
  files.erase(doubled, end);
  return files;
}

std::filesystem::path DecorVignetteCacheRoot(const std::filesystem::path& client_cache) {
  return client_cache / core::PathFromUtf8(kDecorVignetteCacheFolder);
}

std::filesystem::path DecorVignetteCacheRootOrNone(
    const core::Result<std::filesystem::path>& client_cache) {
  if (!client_cache) {
    return {};
  }
  return DecorVignetteCacheRoot(*client_cache);
}

std::filesystem::path DecorVignetteCachePath(const std::filesystem::path& cache_root,
                                             std::string_view rsm_file, std::int32_t side_px) {
  std::filesystem::path path = cache_root / std::to_string(side_px);
  // Un segment par dossier ; un segment vide, « . » ou « .. » ne remonte jamais
  // hors du dossier des vignettes.
  std::size_t start = 0;
  while (start <= rsm_file.size()) {
    std::size_t end = rsm_file.find_first_of("\\/", start);
    if (end == std::string_view::npos) {
      end = rsm_file.size();
    }
    const std::string_view segment = rsm_file.substr(start, end - start);
    if (!segment.empty() && segment != "." && segment != "..") {
      if (end == rsm_file.size()) {
        path /= core::PathFromUtf8(std::string(segment) + std::string(kVignetteExtension));
      } else {
        path /= core::PathFromUtf8(segment);
      }
    }
    start = end + 1;
  }
  return path;
}

std::optional<formats::VignetteKey> DecorVignetteKey(const vfs::Vfs& vfs, std::string_view rsm_file,
                                                     std::int32_t side_px) {
  std::string inner(kModelFolder);
  inner += rsm_file;
  std::optional<core::FileStamp> stamp = vfs.Stamp(inner);
  if (!stamp.has_value()) {
    return std::nullopt;
  }
  return formats::VignetteKey{.model_file = std::string(rsm_file),
                              .side_px = side_px,
                              .render_revision = kDecorVignetteRenderRevision,
                              .stamp = std::move(*stamp)};
}

std::optional<formats::Image> ReadCachedDecorVignette(const std::filesystem::path& path,
                                                      const formats::VignetteKey& key) {
  std::error_code missing;
  if (!std::filesystem::is_regular_file(path, missing)) {
    return std::nullopt;
  }
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    return std::nullopt;
  }
  const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(stream),
                                        std::istreambuf_iterator<char>()};
  core::Result<formats::VignetteFile> file = formats::DecodeVignetteFile(bytes);
  if (!file || file->key != key) {
    return std::nullopt;
  }
  return std::move(file->image);
}

core::Result<void> WriteCachedDecorVignette(const std::filesystem::path& path,
                                            const formats::VignetteKey& key,
                                            const formats::Image& image) {
  const std::vector<std::uint8_t> bytes =
      formats::EncodeVignetteFile(formats::VignetteFile{.key = key, .image = image});
  return core::WriteFileAtomically(
      path, std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
}

core::Result<CachedDecorVignette> LoadCachedDecorVignette(vfs::Vfs& vfs, std::string_view rsm_file,
                                                          std::int32_t side_px,
                                                          const std::filesystem::path& cache_root) {
  if (cache_root.empty()) {
    auto rendered = LoadDecorVignette(vfs, rsm_file, side_px);
    if (!rendered) {
      return std::unexpected(rendered.error());
    }
    return CachedDecorVignette{.image = std::move(*rendered), .origin = VignetteOrigin::kRendered};
  }
  const std::optional<formats::VignetteKey> key = DecorVignetteKey(vfs, rsm_file, side_px);
  if (!key.has_value()) {
    return core::Fail(core::ErrorCode::kNotFound,
                      std::string(kModelFolder) + std::string(rsm_file));
  }
  const std::filesystem::path path = DecorVignetteCachePath(cache_root, rsm_file, side_px);
  if (std::optional<formats::Image> kept = ReadCachedDecorVignette(path, *key); kept.has_value()) {
    if (kept->empty()) {
      return core::Fail(core::ErrorCode::kCorruptData,
                        std::string(rsm_file) + " : aucune face à montrer (vignette gardée)");
    }
    return CachedDecorVignette{.image = std::move(*kept), .origin = VignetteOrigin::kDisk};
  }
  auto rendered = LoadDecorVignette(vfs, rsm_file, side_px);
  const formats::Image failed;
  if (const core::Result<void> written =
          WriteCachedDecorVignette(path, *key, rendered ? *rendered : failed);
      !written) {
    core::LogWarning("vignette de {} non gardée — {}", rsm_file, core::Describe(written.error()));
  }
  if (!rendered) {
    return std::unexpected(rendered.error());
  }
  return CachedDecorVignette{.image = std::move(*rendered), .origin = VignetteOrigin::kRendered};
}

}  // namespace ro::render
