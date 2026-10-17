#include "world/model_previews.h"

#include <optional>
#include <utility>

#include <imgui.h>

#include "ro/core/error.h"
#include "ro/core/log.h"
#include "ro/render/decor_vignette.h"
#include "ro/render/gl_texture.h"

namespace moonclient {

ro::ui::MapEditorPreview ModelPreviews::Show(ro::vfs::Vfs& vfs, const std::string& file) {
  ro::ui::MapEditorPreview preview;
  if (file.empty()) {
    return preview;
  }
  const auto make = [&vfs](const std::string& wanted) -> std::optional<ro::render::IconTexture> {
    auto image = ro::render::LoadDecorVignette(vfs, wanted, ro::render::kDecorVignetteSidePx);
    if (!image) {
      ro::core::LogWarning("aperçu de {} impossible — {}", wanted,
                           ro::core::Describe(image.error()));
      return std::nullopt;
    }
    auto texture = ro::render::GlTexture::Upload(*image, ro::render::GlTexture::Filtering::kSprite);
    if (!texture) {
      ro::core::LogWarning("aperçu de {} non téléversé — {}", wanted,
                           ro::core::Describe(texture.error()));
      return std::nullopt;
    }
    return ro::render::IconTexture{
        .texture = std::move(*texture), .width_px = image->width, .height_px = image->height};
  };
  const PreviewCache<ro::render::IconTexture>::Shown shown = cache_.Get(file, make);
  if (shown.pending()) {
    return preview;
  }
  preview.file = file;
  preview.failed = shown.failed;
  if (shown.picture != nullptr) {
    preview.image = ro::ui::SkinImage{.id = static_cast<ImTextureID>(shown.picture->texture.name()),
                                      .width = static_cast<float>(shown.picture->width_px),
                                      .height = static_cast<float>(shown.picture->height_px)};
  }
  return preview;
}

}  // namespace moonclient
