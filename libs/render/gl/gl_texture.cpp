#include "ro/render/gl_texture.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

#include <glad/glad.h>

#include "ro/render/texture_bleed.h"

namespace ro::render {
namespace {

using core::ErrorCode;
using core::Fail;

GLuint CreateRgba(int width, int height, const void* pixels, GlTexture::Filtering filtering) {
  GLuint texture = 0;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
  // Aucun mipmap, sur aucune texture : voir docs/adr/0008.
  // Seule l'image qui glisse se répète. Toute autre doit être bornée : sinon le
  // filtrage rapporte au bord la couleur du bord opposé.
  const GLint wrap = filtering == GlTexture::Filtering::kSharp ? GL_REPEAT : GL_CLAMP_TO_EDGE;
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
  // L'art d'interface se lit au plus proche : voir Filtering::kPixelArt.
  const GLint sampling = filtering == GlTexture::Filtering::kPixelArt ? GL_NEAREST : GL_LINEAR;
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, sampling);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, sampling);
  return texture;
}

}  // namespace

GlTexture::GlTexture(GlTexture&& other) noexcept : texture_(other.texture_) { other.texture_ = 0; }

GlTexture& GlTexture::operator=(GlTexture&& other) noexcept {
  if (this != &other) {
    if (texture_ != 0) {
      glDeleteTextures(1, &texture_);
    }
    texture_ = other.texture_;
    other.texture_ = 0;
  }
  return *this;
}

GlTexture::~GlTexture() {
  if (texture_ != 0) {
    glDeleteTextures(1, &texture_);
  }
}

core::Result<GlTexture> GlTexture::Upload(const formats::Image& image, Filtering filtering) {
  if (image.width <= 0 || image.height <= 0 || image.pixels.empty()) {
    return Fail(ErrorCode::kCorruptData, "image vide");
  }
  // RIEN À ÉTALER : un atlas n'a pas de couleur clé, et une image sans texel
  // transparent n'a pas de bord à repeindre. Elle part telle quelle, sans copie.
  if (!FillsTransparentEdges(filtering) || !HasTransparentTexel(image)) {
    return GlTexture(CreateRgba(image.width, image.height, image.pixels.data(), filtering));
  }
  // Le remplissage a lieu ICI, et pas chez l'appelant, parce qu'il va de pair
  // avec le filtrage posé juste en dessous : c'est lui qui mélange un texel
  // opaque avec un texel de couleur clé. Voir texture_bleed.h.
  const formats::Image bled = BleedTransparentEdges(image);
  return GlTexture(CreateRgba(bled.width, bled.height, bled.pixels.data(), filtering));
}

core::Result<GlTexture> GlTexture::Reserve(const formats::Image& image, Filtering filtering) {
  if (image.width <= 0 || image.height <= 0 || image.pixels.empty()) {
    return Fail(ErrorCode::kCorruptData, "image vide");
  }
  return GlTexture(CreateRgba(image.width, image.height, nullptr, filtering));
}

void GlTexture::UploadRows(const formats::Image& image, int first_row, int rows) const {
  constexpr std::size_t kRgbaBytes = 4;
  if (texture_ == 0 || rows <= 0 || first_row < 0 || first_row + rows > image.height) {
    return;
  }
  glBindTexture(GL_TEXTURE_2D, texture_);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, first_row, image.width, rows, GL_RGBA, GL_UNSIGNED_BYTE,
                  image.pixels.data() + static_cast<std::size_t>(first_row) *
                                            static_cast<std::size_t>(image.width) * kRgbaBytes);
  glBindTexture(GL_TEXTURE_2D, 0);
}

core::Result<formats::Image> GlTexture::ReadPixels() const {
  constexpr std::size_t kRgbaBytes = 4;
  if (texture_ == 0) {
    return Fail(ErrorCode::kCorruptData, "texture absente");
  }
  glBindTexture(GL_TEXTURE_2D, texture_);
  GLint width = 0;
  GLint height = 0;
  glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
  glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);
  formats::Image image{.width = width, .height = height};
  image.pixels.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) *
                      kRgbaBytes);
  if (!image.pixels.empty()) {
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, image.pixels.data());
  }
  glBindTexture(GL_TEXTURE_2D, 0);
  return image;
}

core::Result<GlTexture> GlTexture::Placeholder() {
  // Magenta franc : une texture manquante doit se VOIR, pas se fondre dans le
  // décor. Un gris neutre laisserait croire à une carte correctement rendue.
  constexpr std::array<std::uint8_t, 4> kMagenta = {255, 0, 255, 255};
  return GlTexture(CreateRgba(1, 1, kMagenta.data(), Filtering::kSharp));
}

void GlTexture::Bind(int unit) const {
  glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + unit));
  glBindTexture(GL_TEXTURE_2D, texture_);
}

void GlTexture::GenerateMipmaps() {
  if (texture_ == 0) {
    return;
  }
  glBindTexture(GL_TEXTURE_2D, texture_);
  glGenerateMipmap(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, 0);
}

// L'anisotrope n'est au cœur d'OpenGL qu'à partir de la 4.6 : en 3.3, il
// vient d'une extension, qu'on cherche par son nom.
int GlTexture::MaxAnisotropy() {
  GLint count = 0;
  glGetIntegerv(GL_NUM_EXTENSIONS, &count);
  for (GLint index = 0; index < count; ++index) {
    const auto* name =
        reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS, static_cast<GLuint>(index)));
    if (name == nullptr) {
      continue;
    }
    const std::string_view extension(name);
    if (extension == "GL_EXT_texture_filter_anisotropic" ||
        extension == "GL_ARB_texture_filter_anisotropic") {
      GLfloat most = 1.0F;
      glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &most);
      return std::max(1, static_cast<int>(most));
    }
  }
  return 1;
}

}  // namespace ro::render
