#include "shell/backdrop_lens.h"

#include <utility>

#include "ro/core/log.h"

#include "shell/backdrop_camera.h"

namespace moonclient {

void BackdropLens::EnsurePieces() {
  if (pieces_tried_) {
    return;
  }
  pieces_tried_ = true;
  if (auto post = ro::render::ScenePostProcess::Create(); post) {
    post_ = std::move(*post);
  } else {
    ro::core::LogWarning("décor : le post-traitement ne s'est pas monté — {}",
                         ro::core::Describe(post.error()));
  }
  if (auto tint = ro::render::ScreenTint::Create(); tint) {
    tint_ = std::move(*tint);
  } else {
    ro::core::LogWarning("décor : la vignette ne s'est pas montée — {}",
                         ro::core::Describe(tint.error()));
  }
}

void BackdropLens::Begin(ro::render::GlContext& gl, const GraphicsOptions& graphics) {
  EnsurePieces();
  post_fx_ = graphics.post_fx();
  const ro::render::DrawableSize surface = gl.drawable_size();
  if (!post_.Begin(surface.width, surface.height, graphics.scene_target(), post_fx_.enabled)) {
    gl.BindForDisplay();
  }
}

void BackdropLens::Finish(ro::render::GlContext& gl, std::uint32_t now_ms) {
  gl.BindForDisplay();
  post_.Present(post_fx_, now_ms);
  const ro::render::DrawableSize surface = gl.drawable_size();
  tint_.Draw(ro::render::ScreenTintLook{.vignette = kBackdropVignette}, surface.width,
             surface.height, now_ms);
}

}  // namespace moonclient
