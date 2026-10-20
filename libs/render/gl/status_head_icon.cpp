#include "ro/render/status_head_icon.h"

#include <utility>

#include "ro/formats/image.h"
#include "ro/render/gl_texture.h"

namespace ro::render {
namespace {

// Une texture d'interface, ou rien : une image vide ou un téléversement raté
// laisse l'icône sans tête, et le bouton retombe sur son art d'avant.
[[nodiscard]] IconTexture UploadButton(const formats::Image& image) {
  IconTexture made;
  if (image.empty()) {
    return made;
  }
  auto texture = GlTexture::Upload(image, GlTexture::Filtering::kPixelArt);
  if (!texture) {
    return made;
  }
  made.texture = std::move(*texture);
  made.width_px = image.width;
  made.height_px = image.height;
  return made;
}

}  // namespace

void StatusHeadIcon::Refresh(const std::optional<StatusHeadKey>& key) {
  if (!art_.Refresh(key)) {
    return;
  }
  const HeadButtonImages& images = art_.images();
  normal_ = UploadButton(images.normal);
  pressed_ = UploadButton(images.pressed);
  // ⚠ LES DEUX OU AUCUN : un enfoncé sans normal montrerait la tête au clic
  // seulement, sur une icône qui ne l'a pas.
  if (!normal_.usable() || !pressed_.usable()) {
    normal_ = IconTexture{};
    pressed_ = IconTexture{};
  }
}

}  // namespace ro::render
