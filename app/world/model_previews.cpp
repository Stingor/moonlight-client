#include "world/model_previews.h"

#include <chrono>
#include <filesystem>
#include <optional>
#include <utility>

#include "ro/core/error.h"
#include "ro/core/log.h"
#include "ro/core/user_cache_directory.h"
#include "ro/render/decor_vignette.h"
#include "ro/render/decor_vignette_cache.h"
#include "ro/render/gl_texture.h"

#include "game/skin_image_of.h"

namespace moonclient {
namespace {

double NowMs() {
  return std::chrono::duration<double, std::milli>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

ro::ui::MapEditorPreview Shown(ro::vfs::Vfs& vfs, const std::string& file,
                               const std::filesystem::path& cache_root,
                               PreviewCache<ro::render::IconTexture>& cache) {
  ro::ui::MapEditorPreview preview;
  if (file.empty()) {
    return preview;
  }
  const auto make =
      [&vfs, &cache_root](const std::string& wanted) -> std::optional<ro::render::IconTexture> {
    auto image = ro::render::LoadCachedDecorVignette(vfs, wanted, ro::render::kDecorVignetteSidePx,
                                                     cache_root);
    if (!image) {
      ro::core::LogWarning("aperçu de {} impossible — {}", wanted,
                           ro::core::Describe(image.error()));
      return std::nullopt;
    }
    auto texture =
        ro::render::GlTexture::Upload(image->image, ro::render::GlTexture::Filtering::kSprite);
    if (!texture) {
      ro::core::LogWarning("aperçu de {} non téléversé — {}", wanted,
                           ro::core::Describe(texture.error()));
      return std::nullopt;
    }
    return ro::render::IconTexture{.texture = std::move(*texture),
                                   .width_px = image->image.width,
                                   .height_px = image->image.height};
  };
  const PreviewCache<ro::render::IconTexture>::Shown shown = cache.Get(file, make, NowMs);
  if (shown.pending()) {
    return preview;
  }
  preview.file = file;
  preview.failed = shown.failed;
  if (shown.picture != nullptr) {
    preview.image = SizedSkinImageOf(*shown.picture);
  }
  return preview;
}

}  // namespace

const std::filesystem::path& ModelPreviews::CacheRoot() {
  if (!cache_root_.has_value()) {
    const ro::core::Result<std::filesystem::path> client_cache = ro::core::ClientCacheDirectory();
    cache_root_ = ro::render::DecorVignetteCacheRootOrNone(client_cache);
    if (!client_cache) {
      ro::core::LogWarning("vignettes des modèles : pas de cache disque, rendues en mémoire — {}",
                           ro::core::Describe(client_cache.error()));
    }
  }
  return *cache_root_;
}

ro::ui::MapEditorPreview ModelPreviews::Show(ro::vfs::Vfs& vfs, const std::string& file) {
  return Shown(vfs, file, CacheRoot(), cache_);
}

std::vector<ro::ui::MapEditorPreview> ModelPreviews::ShowMany(ro::vfs::Vfs& vfs,
                                                              std::span<const std::string> files) {
  std::vector<ro::ui::MapEditorPreview> previews;
  previews.reserve(files.size());
  for (const std::string& file : files) {
    ro::ui::MapEditorPreview preview = Shown(vfs, file, CacheRoot(), cache_);
    if (!preview.file.empty()) {
      previews.push_back(std::move(preview));
    }
  }
  return previews;
}

}  // namespace moonclient
