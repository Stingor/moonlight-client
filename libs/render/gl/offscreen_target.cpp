#include "ro/render/offscreen_target.h"

#include <algorithm>
#include <string>
#include <utility>

#include <glad/glad.h>

namespace ro::render {
namespace {

using core::ErrorCode;
using core::Fail;

// Au-delà, l'allocation devient déraisonnable pour un rendu de contrôle.
constexpr int kMaxSide = 8192;

}  // namespace

OffscreenTarget::OffscreenTarget(OffscreenTarget&& other) noexcept
    : framebuffer_(other.framebuffer_),
      color_(other.color_),
      depth_(other.depth_),
      width_(other.width_),
      height_(other.height_) {
  other.framebuffer_ = 0;
  other.color_ = 0;
  other.depth_ = 0;
}

OffscreenTarget& OffscreenTarget::operator=(OffscreenTarget&& other) noexcept {
  if (this != &other) {
    std::swap(framebuffer_, other.framebuffer_);
    std::swap(color_, other.color_);
    std::swap(depth_, other.depth_);
    std::swap(width_, other.width_);
    std::swap(height_, other.height_);
  }
  return *this;
}

OffscreenTarget::~OffscreenTarget() {
  if (color_ != 0) {
    glDeleteTextures(1, &color_);
  }
  if (depth_ != 0) {
    glDeleteRenderbuffers(1, &depth_);
  }
  if (framebuffer_ != 0) {
    glDeleteFramebuffers(1, &framebuffer_);
  }
}

core::Result<OffscreenTarget> OffscreenTarget::Create(int width, int height) {
  if (width <= 0 || height <= 0 || width > kMaxSide || height > kMaxSide) {
    return Fail(ErrorCode::kCorruptData,
                "dimensions de rendu " + std::to_string(width) + "x" + std::to_string(height));
  }

  OffscreenTarget target;
  target.width_ = width;
  target.height_ = height;

  glGenFramebuffers(1, &target.framebuffer_);
  glBindFramebuffer(GL_FRAMEBUFFER, target.framebuffer_);

  glGenTextures(1, &target.color_);
  glBindTexture(GL_TEXTURE_2D, target.color_);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target.color_, 0);

  glGenRenderbuffers(1, &target.depth_);
  glBindRenderbuffer(GL_RENDERBUFFER, target.depth_);
  // La profondeur ET le stencil, comme la cible du jeu : voir
  // `SpriteRenderer::BodySilhouette`.
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                            target.depth_);

  const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  if (status != GL_FRAMEBUFFER_COMPLETE) {
    return Fail(ErrorCode::kIoFailure, "framebuffer incomplet (0x" + std::to_string(status) + ")");
  }
  return target;
}

void OffscreenTarget::BindAsDrawTarget() const {
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer_);
  glViewport(0, 0, width_, height_);
}

void OffscreenTarget::Bind() const {
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
  glViewport(0, 0, width_, height_);
}

core::Result<formats::Image> OffscreenTarget::ReadPixels() const {
  if (framebuffer_ == 0) {
    return Fail(ErrorCode::kIoFailure, "cible de rendu non initialisée");
  }

  formats::Image image;
  image.width = width_;
  image.height = height_;
  image.pixels.resize(static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_) * 4);

  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
  // Le pas par défaut est de quatre octets ; en RGBA8 il l'est déjà, mais le
  // poser explicitement évite de dépendre d'un état laissé par un autre appel.
  glPixelStorei(GL_PACK_ALIGNMENT, 4);
  glReadPixels(0, 0, width_, height_, GL_RGBA, GL_UNSIGNED_BYTE, image.pixels.data());
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  // OpenGL rend la première rangée en BAS ; on remet à l'endroit.
  const std::size_t row_bytes = static_cast<std::size_t>(width_) * 4;
  for (int y = 0; y < height_ / 2; ++y) {
    std::uint8_t* top = image.pixels.data() + static_cast<std::size_t>(y) * row_bytes;
    std::uint8_t* bottom =
        image.pixels.data() + static_cast<std::size_t>(height_ - 1 - y) * row_bytes;
    std::swap_ranges(top, top + row_bytes, bottom);
  }
  return image;
}

}  // namespace ro::render
