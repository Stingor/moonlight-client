#include "ro/render/screen_grab.h"

#include <utility>

#include <glad/glad.h>

namespace ro::render {
namespace {

using core::ErrorCode;
using core::Fail;

}  // namespace

core::Result<ScreenGrab> ScreenGrab::Create(int width, int height) {
  auto target = OffscreenTarget::Create(width, height);
  if (!target) {
    return std::unexpected(target.error());
  }
  return ScreenGrab(std::move(*target));
}

core::Result<formats::Image> ScreenGrab::Take(const ScreenRegion& zone, int drawable_height) const {
  if (zone.empty()) {
    return Fail(ErrorCode::kInvalidArgument, "zone de capture vide");
  }

  // Le repère d'OpenGL a son origine EN BAS : la rangée du haut de la zone est
  // la plus éloignée du zéro.
  const int gl_bottom = drawable_height - (zone.top + zone.height);
  const int gl_top = drawable_height - zone.top;

  glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
  target_.BindAsDrawTarget();
  // ⚠ NEAREST QUAND RIEN NE CHANGE DE TAILLE : à l'échelle un pour un, le filtre
  // linéaire échantillonne quand même entre deux pixels et adoucit une capture
  // qui devait être exacte.
  const bool same_size = zone.width == target_.width() && zone.height == target_.height();
  glBlitFramebuffer(zone.left, gl_bottom, zone.left + zone.width, gl_top, 0, 0, target_.width(),
                    target_.height(), GL_COLOR_BUFFER_BIT, same_size ? GL_NEAREST : GL_LINEAR);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  return target_.ReadPixels();
}

}  // namespace ro::render
